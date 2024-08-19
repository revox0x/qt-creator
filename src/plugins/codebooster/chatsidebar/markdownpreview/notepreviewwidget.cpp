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
 */

#include "notepreviewwidget.h"

#include "filedialog.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QLayout>
#include <QMenu>
#include <QMovie>
#include <QProxyStyle>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QScrollBar>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#include <QStandardPaths>
#endif

#include "instrumentor.h"

class NoDottedOutlineForLinksStyle : public QProxyStyle {
   public:
    int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                  QStyleHintReturn *returnData) const Q_DECL_OVERRIDE {
        if (hint == SH_TextControl_FocusIndicatorTextCharFormat) return 0;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

NotePreviewWidget::NotePreviewWidget(QWidget *parent) :
    QTextBrowser(parent)
    , mHeightMode(NoVerticalScroll)
    , mMaxHeightLimit(300)

{
    // add the hidden search widget
    _searchWidget = new QTextEditSearchWidget(this);
    _searchWidget->setReplaceEnabled(false);

    // add a layout to the widget
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch();
    this->setLayout(layout);
    this->layout()->addWidget(_searchWidget);

    installEventFilter(this);
    viewport()->installEventFilter(this);

    // 使用下面的代码会导致暗色主题下滚动条为亮色
    // auto proxyStyle = new NoDottedOutlineForLinksStyle;
    // proxyStyle->setParent(this);
    // setStyle(proxyStyle);

    // 默认禁用垂直滚动条
    setHeightMode(mHeightMode);

    // 连接内容变化的信号到槽函数
    connect(this, &QTextBrowser::textChanged, this, &NotePreviewWidget::adjustHeight);
}

void NotePreviewWidget::resizeEvent(QResizeEvent *event) {
    // emit resize(event->size(), event->oldSize());

    // we need this, otherwise the preview is always blank
    QTextBrowser::resizeEvent(event);
    adjustHeight();
}

bool NotePreviewWidget::eventFilter(QObject *obj, QEvent *event) {
    //    qDebug() << event->type();
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        // disallow keys if widget hasn't focus
        if (!this->hasFocus()) {
            return true;
        }

        if ((keyEvent->key() == Qt::Key_Escape) && _searchWidget->isVisible()) {
            _searchWidget->deactivate();
            return true;
        } else if ((keyEvent->key() == Qt::Key_F) &&
                   keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
            _searchWidget->activate();
            return true;
        } else if ((keyEvent->key() == Qt::Key_F3)) {
            _searchWidget->doSearch(!keyEvent->modifiers().testFlag(Qt::ShiftModifier));
            return true;
        }

        return false;
    }

    return QTextBrowser::eventFilter(obj, event);
}

/**
 * @brief Extract local gif urls from html
 * @param text
 * @return Urls to gif files
 */
QStringList NotePreviewWidget::extractGifUrls(const QString &text) const {
    QSet<QString> urlSet;
    static const QRegularExpression regex(R"(<img[^>]+src=\"(file:\/\/\/[^\"]+\.gif)\")",
                                          QRegularExpression::CaseInsensitiveOption);
    int pos = 0;

    while (true) {
        QRegularExpressionMatch match;
        pos = text.indexOf(regex, pos, &match);
        if (pos == -1 || !match.hasMatch()) break;
        QString url = match.captured(1);
        urlSet.insert(url);
        pos += match.capturedLength();
    }

#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
    return urlSet.toList();
#else
    return QStringList(urlSet.begin(), urlSet.end());
#endif
}

/**
 * @brief Setup animations for gif
 * @return
 */
void NotePreviewWidget::animateGif(const QString &text) {
    // clear resources
    if (QTextDocument *doc = document()) doc->clear();

    QStringList urls = extractGifUrls(text);

    for (QMovie *&movie : _movies) {
        QString url = movie->property("URL").toString();
        if (urls.contains(url))
            urls.removeAll(url);
        else {
            movie->deleteLater();
            movie = nullptr;
        }
    }
    _movies.removeAll(nullptr);

    for (const QString &url : urls) {
        auto *movie = new QMovie(this);
        movie->setFileName(QUrl(url).toLocalFile());
        movie->setCacheMode(QMovie::CacheNone);

        if (!movie->isValid() || movie->frameCount() < 2) {
            movie->deleteLater();
            continue;
        }

        movie->setProperty("URL", url);
        _movies.append(movie);

        connect(movie, &QMovie::frameChanged, this, [this, url, movie](int) {
            if (auto doc = document()) {
                doc->addResource(QTextDocument::ImageResource, url, movie->currentPixmap());
                doc->markContentsDirty(0, doc->characterCount());
            }
        });

        movie->start();
    }
}

void NotePreviewWidget::setHtml(const QString &text) {
    //animateGif(text);
    //_html = parseTaskList(text, true);
    //QTextBrowser::setHtml(_html);
    //PROFILE_FUNCTION();

    _html = text;
    QTextBrowser::setHtml(_html);
}

/**
 * @brief Returns the searchWidget instance
 * @return
 */
QTextEditSearchWidget *NotePreviewWidget::searchWidget() { return _searchWidget; }

/**
 * Uses another widget as parent for the search widget
 */
void NotePreviewWidget::initSearchFrame(QWidget *searchFrame, bool darkMode) {
    _searchFrame = searchFrame;

    // remove the search widget from our layout
    layout()->removeWidget(_searchWidget);

    QLayout *layout = _searchFrame->layout();

    // create a grid layout for the frame and add the search widget to it
    if (layout == nullptr) {
        layout = new QVBoxLayout(_searchFrame);
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);
    }

    _searchWidget->setDarkMode(darkMode);
    _searchWidget->setReplaceEnabled(false);
    layout->addWidget(_searchWidget);
    _searchFrame->setLayout(layout);
}

/**
 * Hides the preview and the search widget
 */
void NotePreviewWidget::hide() {
    _searchWidget->hide();
    QWidget::hide();
}

void NotePreviewWidget::openFolderSelect(const QString &absolutePath)
{
    const QString path = QDir::fromNativeSeparators(absolutePath);
#ifdef Q_OS_WIN
    if (QFileInfo(path).exists()) {
        // Syntax is: explorer /select, "C:\Folder1\Folder2\file_to_select"
        // Dir separators MUST be win-style slashes

        // QProcess::startDetached() has an obscure bug. If the path has
        // no spaces and a comma(and maybe other special characters) it doesn't
        // get wrapped in quotes. So explorer.exe can't find the correct path
        // anddisplays the default one. If we wrap the path in quotes and pass
        // it to QProcess::startDetached() explorer.exe still shows the default
        // path. In this case QProcess::startDetached() probably puts its
        // own quotes around ours.

        STARTUPINFO startupInfo;
        ::ZeroMemory(&startupInfo, sizeof(startupInfo));
        startupInfo.cb = sizeof(startupInfo);

        PROCESS_INFORMATION processInfo;
        ::ZeroMemory(&processInfo, sizeof(processInfo));

        QString cmd = QStringLiteral("explorer.exe /select,\"%1\"")
                          .arg(QDir::toNativeSeparators(absolutePath));
        LPWSTR lpCmd = new WCHAR[cmd.size() + 1];
        cmd.toWCharArray(lpCmd);
        lpCmd[cmd.size()] = 0;

        bool ret = ::CreateProcessW(NULL, lpCmd, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo,
                                    &processInfo);
        delete[] lpCmd;

        if (ret) {
            ::CloseHandle(processInfo.hProcess);
            ::CloseHandle(processInfo.hThread);
        }
    } else {
        // If the item to select doesn't exist, try to open its parent
        openPath(path.left(path.lastIndexOf("/")));
    }
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    static const auto fullXdgMimePath = QStandardPaths::findExecutable(QStringLiteral("xdg-mime"));
    if (!fullXdgMimePath.isEmpty() && QFileInfo(path).exists()) {
        QProcess proc;
        QString output;
        proc.start(fullXdgMimePath, QStringList{"query", "default", "inode/directory"});
        proc.waitForFinished();
        output = proc.readLine().simplified();
        if (output == QStringLiteral("dolphin.desktop") ||
            output == QStringLiteral("org.kde.dolphin.desktop")) {
            proc.startDetached(QStringLiteral("dolphin"),
                               QStringList{"--select", QDir::toNativeSeparators(path)});
        } else if (output == QStringLiteral("nautilus.desktop") ||
                   output == QStringLiteral("org.gnome.Nautilus.desktop") ||
                   output == QStringLiteral("nautilus-folder-handler.desktop")) {
            proc.startDetached(QStringLiteral("nautilus"),
                               QStringList{"--no-desktop", QDir::toNativeSeparators(path)});
        } else if (output == QStringLiteral("caja-folder-handler.desktop")) {
            const QDir dir = QFileInfo(path).absoluteDir();
            const QString absDir = dir.absolutePath();
            proc.startDetached(QStringLiteral("caja"),
                               QStringList{"--no-desktop", QDir::toNativeSeparators(absDir)});
        } else if (output == QStringLiteral("nemo.desktop")) {
            proc.startDetached(QStringLiteral("nemo"),
                               QStringList{"--no-desktop", QDir::toNativeSeparators(path)});
        } else if (output == QStringLiteral("konqueror.desktop") ||
                   output == QStringLiteral("kfmclient_dir.desktop")) {
            proc.startDetached(QStringLiteral("konqueror"),
                               QStringList{"--select", QDir::toNativeSeparators(path)});
        } else {
            openPath(path.left(path.lastIndexOf(QLatin1Char('/'))));
        }
    } else {
        // if the item to select doesn't exist, try to open its parent
        openPath(path.left(path.lastIndexOf(QLatin1Char('/'))));
    }
#else
    openPath(path.left(path.lastIndexOf(QLatin1Char('/'))));
#endif
}

void NotePreviewWidget::openPath(const QString &absolutePath)
{
    const QString path = QDir::fromNativeSeparators(absolutePath);
    // Hack to access samba shares with QDesktopServices::openUrl
    if (path.startsWith(QStringLiteral("//")))
        QDesktopServices::openUrl(QDir::toNativeSeparators(QStringLiteral("file:") % path));
    else
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString NotePreviewWidget::parseTaskList(const QString &html, bool clickable)
{
    auto text = html;

    // set a list item style for todo items
    // using a css class didn't work because the styling seems to affects the sub-items too
    const auto listTag = QStringLiteral("<li style=\"list-style-type:square\">");

    static const QRegularExpression re1(QStringLiteral(R"(<li>(\s*(<p>)*\s*)\[ ?\])"),
                                        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression re2(QStringLiteral(R"(<li>(\s*(<p>)*\s*)\[[xX]\])"),
                                        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression re3(QStringLiteral(R"(<li>(\s*(<p>)*\s*)\[-\])"),
                                        QRegularExpression::CaseInsensitiveOption);
    if (!clickable) {
        text.replace(re1, listTag % QStringLiteral("\\1&#9744;"));
        text.replace(re2, listTag % QStringLiteral("\\1&#9745;"));
        text.replace(re3, listTag % QStringLiteral("\\1&#10005;"));
        return text;
    }

    // TODO
    // to ensure the clicking behavior of checkboxes,
    // line numbers of checkboxes in the original Markdown text
    // should be provided by the Markdown parser

    text.replace(re3, listTag % QStringLiteral("\\1&#10005;"));

    const QString checkboxStart =
        QStringLiteral(R"(<a class="task-list-item-checkbox" href="checkbox://_)");
    text.replace(
        re1, listTag % QStringLiteral("\\1") % checkboxStart % QStringLiteral("\">&#9744;</a>"));
    text.replace(
        re2, listTag % QStringLiteral("\\1") % checkboxStart % QStringLiteral("\">&#9745;</a>"));

    int count = 0;
    int pos = 0;
    while (true) {
        pos = text.indexOf(checkboxStart % QStringLiteral("\""), pos);
        if (pos == -1) break;

        pos += checkboxStart.length();
        text.insert(pos, QString::number(count++));
    }

    return text;
}

void NotePreviewWidget::adjustHeight()
{
    if (mHeightMode == NoVerticalScroll)
    {
        // 获取内容高度并调整控件高度
        document()->setTextWidth(this->viewport()->width());
        int docHeight = document()->size().height();
        // 控件高度比文本高一点防止内容可以被鼠标滚动
        int widgetHeight = docHeight + 6;
        if (horizontalScrollBar()->isVisible())
        {
            widgetHeight += horizontalScrollBar()->height() - 2;
        }

        setFixedHeight(widgetHeight);
    }
    else if (mHeightMode == MaxLimit)
    {
        document()->setTextWidth(this->viewport()->width());
        int docHeight = document()->size().height();

        // 一种非常丑陋的解决方案：
        // 在显示编辑器代码块时，如果连续框选一大段文本，docHeight会突然变成一个很小的值（30，应该是单行高度）
        // 所以这里对高度设置进行过滤，如果已经存在一段文本的情况下禁止设置比较小的高度，防止出现代码块控件高度
        // 随着鼠标框选编辑器文本发生跳变的情况;
        if (docHeight < 30)
        {
            if (_html.size() > 3000)
                return;
        }
        // end

        int widgetHeight = docHeight + 10;
        if (widgetHeight > mMaxHeightLimit)
        {
            setFixedHeight(mMaxHeightLimit);
        }
        else
        {
            setFixedHeight(widgetHeight);
        }
    }
}

/**
 * Shows a context menu for the note preview
 *
 * @param event
 */
void NotePreviewWidget::contextMenuEvent(QContextMenuEvent *event) {
    QPoint pos = event->pos();
    QPoint globalPos = event->globalPos();
    QMenu *menu = this->createStandardContextMenu();

    QTextCursor c = this->cursorForPosition(pos);
    QTextFormat format = c.charFormat();
    const QString &anchorHref = format.toCharFormat().anchorHref();
    bool isImageFormat = format.isImageFormat();
    bool isAnchor = !anchorHref.isEmpty();

    if (isImageFormat || isAnchor) {
        menu->addSeparator();
    }

    QAction *copyImageAction = nullptr;
    QAction *copyLinkLocationAction = nullptr;

    // check if clicked object was an image
    if (isImageFormat) {
        copyImageAction = menu->addAction(tr("Copy image file path"));
        auto *copyImageClipboardAction = menu->addAction(tr("Copy image to clipboard"));

        connect(copyImageClipboardAction, &QAction::triggered, this, [format]() {
            QString imagePath = format.toImageFormat().name();
            QUrl imageUrl = QUrl(imagePath);
            QClipboard *clipboard = QApplication::clipboard();
            if (imageUrl.isLocalFile()) {
                clipboard->setImage(QImage(imageUrl.toLocalFile()));
            } else if (imagePath.startsWith(QLatin1String("data:image/"), Qt::CaseInsensitive)) {
                QStringList parts = imagePath.split(QStringLiteral(";base64,"));
                if (parts.count() == 2) {
                    clipboard->setImage(
                        QImage::fromData(QByteArray::fromBase64(parts[1].toLatin1())));
                }
            }
        });
    }

    if (isAnchor) {
        copyLinkLocationAction = menu->addAction(tr("Copy link location"));
    }

    auto *htmlFileExportAction = menu->addAction(tr("Export generated raw HTML"));

    QAction *selectedItem = menu->exec(globalPos);

    if (selectedItem) {
        // copy the image file path to the clipboard
        if (selectedItem == copyImageAction) {
            QString imagePath = format.toImageFormat().name();
            QUrl imageUrl = QUrl(imagePath);

            if (imageUrl.isLocalFile()) {
                imagePath = imageUrl.toLocalFile();
            }

            QClipboard *clipboard = QApplication::clipboard();
            clipboard->setText(imagePath);
        }
        // copy link location to the clipboard
        else if (selectedItem == copyLinkLocationAction) {
            QClipboard *clipboard = QApplication::clipboard();
            clipboard->setText(anchorHref);
        }
        // export the generated html as html file
        else if (selectedItem == htmlFileExportAction) {
            exportAsHTMLFile();
        }
    }
}

QVariant NotePreviewWidget::loadResource(int type, const QUrl &file) {
    if (type == QTextDocument::ImageResource && file.isValid()) {
        QString fileName = file.toLocalFile();
        int fileSize = QFileInfo(fileName).size();

        // We only use our cache for images > 512KB
        if (fileSize > (512 * 1000)) {
            QPixmap pm;
            if (lookupCache(fileName, pm)) {
                return pm;
            }

            pm = QPixmap(fileName);
            insertInCache(fileName, pm);
            return pm;
        }
    }

    return QTextBrowser::loadResource(type, file);
}

bool NotePreviewWidget::lookupCache(const QString &key, QPixmap &pm) {
    auto it = std::find_if(_largePixmapCache.begin(), _largePixmapCache.end(),
                           [key](const LargePixmap &l) { return key == l.fileName; });
    if (it == _largePixmapCache.end()) return false;
    pm = it->pixmap;
    return true;
}

void NotePreviewWidget::insertInCache(const QString &key, const QPixmap &pm) {
    _largePixmapCache.push_back({key, std::move(pm)});

    // limit to 6 images
    while (_largePixmapCache.size() > 6) _largePixmapCache.erase(_largePixmapCache.begin());
}

void NotePreviewWidget::exportAsHTMLFile() {
    FileDialog dialog(QStringLiteral("PreviewHTMLFileExport"));
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter(tr("HTML files") + " (*.html)");
    dialog.setWindowTitle(
        tr("Export preview as raw HTML file",
           "\"Raw\" means that actually the html that was fed to the preview will be stored (the "
           "QTextBrowser modifies the html that it is showing)"));
    dialog.selectFile(QStringLiteral("preview.html"));
    int ret = dialog.exec();

    if (ret == QDialog::Accepted) {
        QString fileName = dialog.selectedFile();

        if (!fileName.isEmpty()) {
            if (QFileInfo(fileName).suffix().isEmpty()) {
                fileName.append(".html");
            }

            QFile file(fileName);

            qDebug() << "exporting raw preview html file: " << fileName;

            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                qCritical() << file.errorString();
                return;
            }
            QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            out.setCodec("UTF-8");
#endif
            out << _html;
            file.flush();
            file.close();
            openFolderSelect(fileName);
        }
    }
}

void NotePreviewWidget::disableLineWrap()
{
    setLineWrapMode(QTextEdit::NoWrap);
}


void NotePreviewWidget::setHeightMode(HeightMode mode)
{
    mHeightMode = mode;

    if (mode == NoVerticalScroll)
    {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    else if (mode == MaxLimit)
    {
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }
}

void NotePreviewWidget::setHeightLimit(int limit)
{
    mMaxHeightLimit = limit;
}
