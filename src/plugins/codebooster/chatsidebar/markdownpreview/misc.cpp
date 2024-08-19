/*
 * Copyright (c) 2014-2024 Patrizio Bekerle -- <patrizio@bekerle.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 */

#include "misc.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDataStream>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QHttpMultiPart>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QSettings>
#include <QStringBuilder>
#include <QTemporaryFile>
#include <QTextDocument>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QXmlStreamReader>
#include <QtGui/QIcon>
#include <utility>
#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
#include <QHostInfo>
#endif


#ifndef INTEGRATION_TESTS
#include <QScreen>
#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
#include <QGuiApplication>
#endif
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#include <QStandardPaths>
#endif

enum SearchEngines {
    Google = 0,
    Bing = 1,
    DuckDuckGo = 2,
    Yahoo = 3,
    GoogleScholar = 4,
    Yandex = 5,
    AskDotCom = 6,
    Qwant = 7,
    Startpage = 8
};


/**
 * Transforms remote preview image tags
 *
 * @param html
 */
void Utils::Misc::transformRemotePreviewImages(QString &html, int maxImageWidth,
                                               ExternalImageHash *externalImageHash) {
    static const QRegularExpression re(QStringLiteral(R"(<img src=\"(https?:\/\/.+)\".*\/?>)"),
                                       QRegularExpression::CaseInsensitiveOption |
                                           QRegularExpression::MultilineOption |
                                           QRegularExpression::InvertedGreedinessOption);
    QRegularExpressionMatchIterator i = re.globalMatch(html);

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString imageTag = match.captured(0);
        QString inlineImageTag;
        int imageWidth;
        ExternalImageHashItem hashItem;

        if (externalImageHash->contains(imageTag)) {
            hashItem = externalImageHash->value(imageTag);
            inlineImageTag = hashItem.imageTag;
            imageWidth = hashItem.imageWidth;
        } else {
            inlineImageTag = remotePreviewImageTagToInlineImageTag(imageTag, imageWidth);
            hashItem.imageTag = inlineImageTag;
            hashItem.imageWidth = imageWidth;
            externalImageHash->insert(imageTag, hashItem);
        }

        imageWidth = std::min(maxImageWidth, imageWidth);
        inlineImageTag.replace(">", QString("width=\"%1\">").arg(QString::number(imageWidth)));
        html.replace(imageTag, inlineImageTag);
    }
}

/**
 * Transforms a remote preview image tag to an inline image tag
 *
 * @param data
 * @param imageSuffix
 * @return
 */
QString Utils::Misc::remotePreviewImageTagToInlineImageTag(QString imageTag, int &imageWidth) {
    imageTag.replace(QStringLiteral("&amp;"), QStringLiteral("&"));

    static const QRegularExpression re(
        QStringLiteral(R"(<img src=\"(https?:\/\/.+)\")"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::InvertedGreedinessOption);

    QRegularExpressionMatch match = re.match(imageTag);
    if (!match.hasMatch()) {
        return imageTag;
    }

    const QString url = match.captured(1);
    const QByteArray data = downloadUrl(url);
    auto image = QImage::fromData(data);
    imageWidth = image.width();
    const QMimeDatabase db;
    const auto type = db.mimeTypeForData(data);

    // for now we do no caching, because we don't know when to invalidate the
    // cache
    const QString replace =
        QStringLiteral("data:") % type.name() % QStringLiteral(";base64,") % data.toBase64();
    return imageTag.replace(url, replace);
}

/**
 * Downloads an url and returns the data
 *
 * @param url
 * @return {QByteArray} the content of the downloaded url
 */
QByteArray Utils::Misc::downloadUrl(const QUrl &url, bool usePost, QByteArray postData) {
    int statusCode;

    return downloadUrlWithStatusCode(url, statusCode, usePost, std::move(postData));
}

QByteArray Utils::Misc::downloadUrlWithStatusCode(const QUrl &url, int &returnStatusCode,
                                                  bool usePost, QByteArray postData) {
    auto *manager = new QNetworkAccessManager();
    QEventLoop loop;
    QTimer timer;

    timer.setSingleShot(true);

    QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    QObject::connect(manager, SIGNAL(finished(QNetworkReply *)), &loop, SLOT(quit()));

    // 10 sec timeout for the request
    timer.start(10000);

    QNetworkRequest networkRequest = QNetworkRequest(url);
    networkRequest.setHeader(QNetworkRequest::UserAgentHeader,
                             Utils::Misc::friendlyUserAgentString());

#if QT_VERSION < QT_VERSION_CHECK(5, 9, 0)
    networkRequest.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#else
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, true);
#endif

    QByteArray data;
    QNetworkReply *reply;

    if (usePost) {
        if (postData == nullptr) {
            postData = QByteArray();
        }

        networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                                 "application/x-www-form-urlencoded");
        reply = manager->post(networkRequest, postData);
    } else {
        reply = manager->get(networkRequest);
    }

    loop.exec();

    // if we didn't get a timeout let us return the content
    if (timer.isActive()) {
        returnStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // only get the data if the status code was "success"
        // see: https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
        if (returnStatusCode >= 200 && returnStatusCode < 300) {
            // get the data from the network reply
            data = reply->readAll();
        }
    }

    reply->deleteLater();
    delete (manager);

    return data;
}

QByteArray Utils::Misc::friendlyUserAgentString() {
    const auto pattern = QStringLiteral("%1 (QOwnNotes - %2)");

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
    const auto userAgent = pattern.arg(QSysInfo::machineHostName(), platform());
#else
    const auto userAgent = pattern.arg(QHostInfo::localHostName(), platform());
#endif

    return userAgent.toUtf8();
}

QLatin1String Utils::Misc::platform() {
#if defined(Q_OS_WIN)
    return QLatin1String("Windows");
#elif defined(Q_OS_MAC)
    return QLatin1String("macOS");
#elif defined(Q_OS_LINUX)
    return QLatin1String("Linux");
#elif defined(Q_OS_FREEBSD) || defined(Q_OS_FREEBSD_KERNEL)
    return QLatin1String("FreeBSD");
#elif defined(Q_OS_NETBSD)
    return QLatin1String("NetBSD");
#elif defined(Q_OS_OPENBSD)
    return QLatin1String("OpenBSD");
#elif defined(Q_OS_SOLARIS)
    return QLatin1String("Solaris");
#else
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    return QSysInfo::productType();
#else
    return " Qt " + QString(QT_VERSION_STR);
#endif
#endif
}
