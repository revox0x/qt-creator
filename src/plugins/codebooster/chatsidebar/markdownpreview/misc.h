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

#pragma once

#include <QHash>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>


/*  Miscellaneous functions that can be useful */
namespace Utils {
namespace Misc {
struct ExternalImageHashItem {
    QString imageTag;
    int imageWidth;
};

typedef QHash<QString, ExternalImageHashItem> ExternalImageHash;
}    // namespace Misc

// this adds const to non-const objects (like cxx-17 std::as_const)
// always use it with cxx-11 range-for when the container being iterated over is
// a temporary or non-const
template <typename T>
Q_DECL_CONSTEXPR typename std::add_const<T>::type &asConst(T &t) noexcept {
    return t;
}
// prevent rvalue arguments:
template <typename T>
void asConst(const T &&) = delete;
}    // namespace Utils

Q_DECLARE_METATYPE(Utils::Misc::ExternalImageHash *)

namespace Utils {
namespace Misc {


void transformRemotePreviewImages(QString &html, int maxImageWidth,
                                  ExternalImageHash *externalImageHash);
QString remotePreviewImageTagToInlineImageTag(QString imageTag, int &imageWidth);

QByteArray downloadUrl(const QUrl &url, bool usePost = false, QByteArray postData = nullptr);
QByteArray downloadUrlWithStatusCode(const QUrl &url, int &returnStatusCode, bool usePost = false,
                                     QByteArray postData = nullptr);

QByteArray friendlyUserAgentString();
QLatin1String platform();

}    // namespace Misc
}    // namespace Utils
