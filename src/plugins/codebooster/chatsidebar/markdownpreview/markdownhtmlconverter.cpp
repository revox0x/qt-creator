#include "markdownhtmlconverter.h"

#include <QRegularExpression>
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
#include <QTimer>
#include <QFont>

#include "../md4c/md4c-html.h"
#include "../md4c/md4c.h"

#include "codetohtmlconverter.h"
#include "schema.h"
#include "markdownpreviewsetting.h"

#include "instrumentor.h"

using namespace MarkdownPreview;

static void captureHtmlFragment(const MD_CHAR *data, MD_SIZE data_size, void *userData);

const QRegularExpression MarkdownHtmlConverter::cssFontSetting(QStringLiteral(R"(font-size: \d+\w+;)"));
const QRegularExpression MarkdownHtmlConverter::deleteLine(QStringLiteral("<del>([^<]+)<\\/del>"));

MarkdownHtmlConverter::MarkdownHtmlConverter()
{


}

QString MarkdownHtmlConverter::toMarkdownHtml(QString str, bool codeMode)
{
    //PROFILE_FUNCTION();

    // MD4C flags
    unsigned flags =
        MD_DIALECT_GITHUB | MD_FLAG_WIKILINKS | MD_FLAG_LATEXMATHSPANS | MD_FLAG_UNDERLINE;
    // we parse the task lists ourselves

    if (MarkdownPreviewSetting::instance().userUnderLine())
    {
        flags &= ~MD_FLAG_UNDERLINE;
    }

    QString windowsSlash = QLatin1String("");

#ifdef Q_OS_WIN32
    // we need another slash for Windows
    windowsSlash = "/";
#endif

    // remove frontmatter from Markdown text
    if (str.startsWith(QLatin1String("---"))) {
        static const QRegularExpression re(
            QStringLiteral(
                R"(^---((\r\n)|(\n\r)|\r|\n).+?((\r\n)|(\n\r)|\r|\n)---((\r\n)|(\n\r)|\r|\n))"),
            QRegularExpression::DotMatchesEverythingOption);
        str.remove(re);
    }

    /*CODE HIGHLIGHTING*/
    int cbCount = nonOverlapCount(str, '`');
    if (cbCount % 2 != 0) --cbCount;

    int cbTildeCount = nonOverlapCount(str, '~');
    if (cbTildeCount % 2 != 0) --cbTildeCount;

    // divide by two to get actual number of code blocks
    cbCount /= 2;
    cbTildeCount /= 2;

    // this will also add html in the code blocks, so we will do this at the very end
    highlightCode(str, QStringLiteral("```"), cbCount);
    highlightCode(str, QStringLiteral("~~~"), cbTildeCount);

    const auto data = str.toUtf8();
    if (data.size() == 0) {
        return QLatin1String("");
    }

    QByteArray array;
    const int renderResult =
        md_html(data.data(), MD_SIZE(data.size()), &captureHtmlFragment, &array, flags, 0);


    QString result;
    if (renderResult == 0) {
        result = QString::fromUtf8(array);
    } else {
        qWarning() << "MD4C Failure!";
        return QString();
    }

    // transform remote preview image tags
    //Utils::Misc::transformRemotePreviewImages(result, maxImageWidth, externalImageHash());


    // transform images without "file://" urls to file-urls
    // Note: this is currently handled above in Markdown
    //       if we want to activate this code again we need to take care of
    //       remote http(s) links to images! see:
    //       https://github.com/pbek/QOwnNotes/issues/1286
    /*
        const QString subFolderPath = getNoteSubFolder().relativePath("/");
        const QString notePath = notesPath + (subFolderPath.isEmpty() ? "" : "/"
       + subFolderPath); result.replace( QRegularExpression(R"((<img
       src=\")((?!file:\/\/).+)\")"),
                "\\1file://" + windowsSlash + notePath + "/\\2\"");
    */

    static const QString fontString = MarkdownPreviewSetting::instance().previewCodeFontString();

    // set the stylesheet for the <code> blocks
    QString codeStyleSheet = QLatin1String("");
    if (!fontString.isEmpty()) {
        // set the note text view font
        QFont font;
        font.fromString(fontString);

        // add the font for the code block
        codeStyleSheet =
            QStringLiteral("pre, code { %1; }").arg(Utils::Schema::encodeCssFont(font));

        // ignore code font size to allow zooming (#1202)
        if (MarkdownPreviewSetting::instance().ignoreCodeFontSize()) {
            //codeStyleSheet.remove(QRegularExpression(QStringLiteral(R"(font-size: \d+\w+;)")));;
            codeStyleSheet.remove(deleteLine);
        }
    } 

    const bool darkModeColors = MarkdownPreviewSetting::instance().useDarkMode();

    const QString codeForegroundColor =
        darkModeColors ? QStringLiteral("#ffffff") : QStringLiteral("#000000");
    const QString codeBackgroundColor =
        darkModeColors ? QStringLiteral("#444444") : QStringLiteral("#f1f1f1");

    // do some more code formatting
    // the "pre" styles are for the full-width code block background color
    codeStyleSheet += QString(
                          "pre { display: block; background-color: %1;"
                          " white-space: pre-wrap } "
                          "code { padding: 3px; overflow: auto;"
                          " line-height: 1.65em; background-color: %1;"
                          " border-radius: 5px; color: %2; }")
                          .arg(codeBackgroundColor, codeForegroundColor);

    // TODO: We should probably make a stylesheet for this
    codeStyleSheet += QStringLiteral(" .code-comment { color: #75715E;}");
    codeStyleSheet += QStringLiteral(" .code-string { color: #E6DB74;}");
    codeStyleSheet += QStringLiteral(" .code-literal { color: #AE81FF;}");
    codeStyleSheet += QStringLiteral(" .code-type { color: #66D9EF;}");
    codeStyleSheet += QStringLiteral(" .code-builtin { color: #A6E22E;}");
    codeStyleSheet += QStringLiteral(" .code-keyword { color: #F92672;}");
    codeStyleSheet += QStringLiteral(" .code-other { color: #F92672;}");

    // correct the strikeout tag
    // result.replace(QRegularExpression(QStringLiteral("<del>([^<]+)<\\/del>")),
    //                QStringLiteral("<s>\\1</s>"));
    result.replace(deleteLine, QStringLiteral("<s>\\1</s>"));
    const bool rtl = MarkdownPreviewSetting::instance().rightToLeft();
    const QString rtlStyle =
        rtl ? QStringLiteral("body {text-align: right; direction: rtl;}") : QLatin1String("");

    // 主要耗时函数：
    static const QString schemaStyles = Utils::Schema::getNormalSchemaStyle();

    // for preview
    result = QStringLiteral(
                 "<html><head><style>"
                 "h1 { margin: 5px 0 20px 0; }"
                 "h2, h3 { margin: 10px 0 15px 0; }"
                 "table {border-spacing: 0; border-style: solid; border-width: "
                 "1px; border-collapse: collapse; margin-top: 0.5em;}"
                 "th, td {padding: 2px 5px;}"
                 "li {margin-bottom: 0.4em;}" // 调整块间距
                 "a { color: #FF9137; text-decoration: none; } %1 %3 %4"
                 "</style></head><body class=\"preview\">%2</body></html>")
                 .arg(codeStyleSheet, result, rtlStyle, schemaStyles);
    // remove trailing newline in code blocks
    result.replace(QStringLiteral("\n</code>"), QStringLiteral("</code>"));

    return result;
}

int MarkdownHtmlConverter::nonOverlapCount(const QString &str, const QChar c)
{
    const auto len = str.length();
    int count = 0;
    for (int i = 0; i < len; ++i) {
        if (str[i] == c && i + 2 < len && str[i + 1] == c && str[i + 2] == c) {
            ++count;
            i += 2;
        }
    }
    return count;
}

void MarkdownHtmlConverter::highlightCode(QString &str, const QString &type, int cbCount)
{
    if (cbCount >= 1) {
        const int firstBlock = str.indexOf(type, 0);
        int currentCbPos = firstBlock;
        for (int i = 0; i < cbCount; ++i) {
            // find endline
            const int endline = str.indexOf(QChar('\n'), currentCbPos);
            // something invalid? => just skip it
            if (endline == -1) {
                break;
            }

            if (currentCbPos >= 4) {
                bool fourSpaces =
                    std::all_of(str.cbegin() + (currentCbPos - 4), str.cbegin() + currentCbPos,
                                [](QChar c) { return c == QChar(' '); });
                if (fourSpaces) {
                    continue;
                }
            }

            const QString lang = str.mid(currentCbPos + 3, endline - (currentCbPos + 3));
            // we skip it because it is inline code and not codeBlock
            if (lang.contains(type)) {
                int nextEnd = str.indexOf(type, currentCbPos + 3);
                nextEnd += 3;
                currentCbPos = str.indexOf(type, nextEnd);
                continue;
            }
            // move start pos to after the endline
            currentCbPos = endline + 1;
            // find the codeBlock end
            int next = str.indexOf(type, currentCbPos);
            if (next == -1) {
                break;
            }
            // extract the codeBlock
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 2)
            const QStringRef codeBlock = str.midRef(currentCbPos, next - currentCbPos);
#else
            QStringView str_view = str;
            QStringView codeBlock = str_view.mid(currentCbPos, next - currentCbPos);
#endif
            QString highlightedCodeBlock;
            if (!(codeBlock.isEmpty() && lang.isEmpty())) {
                const CodeToHtmlConverter c(lang);
                highlightedCodeBlock = c.process(codeBlock);
                // take care of the null char
                highlightedCodeBlock.replace(QChar('\u0000'), QLatin1String(""));
                str.replace(currentCbPos, next - currentCbPos, highlightedCodeBlock);
                // recalculate next because string has now changed
                next = str.indexOf(type, currentCbPos);
            }
            // move next pos to after the backticks
            next += 3;
            // find the start of the next code block
            currentCbPos = str.indexOf(type, next);
        }
    }
}


Utils::Misc::ExternalImageHash *MarkdownHtmlConverter::externalImageHash()
{
    auto *instance = qApp->property("Utils::Misc::ExternalImageHash").value<Utils::Misc::ExternalImageHash *>();

    if (instance == nullptr) {
        instance = new Utils::Misc::ExternalImageHash;

        qApp->setProperty("Utils::Misc::ExternalImageHash",
                          QVariant::fromValue<Utils::Misc::ExternalImageHash *>(instance));
    }

    return instance;
}


static void captureHtmlFragment(const MD_CHAR *data, MD_SIZE data_size, void *userData) {
    QByteArray *array = static_cast<QByteArray *>(userData);

    if (data_size > 0) {
        array->append(data, int(data_size));
    }
}
