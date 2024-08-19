#ifndef MARKDOWNPREVIEWSETTING_H
#define MARKDOWNPREVIEWSETTING_H

#include <QString>

namespace MarkdownPreview
{

class MarkdownPreviewSetting
{
public:
    MarkdownPreviewSetting();

    static MarkdownPreviewSetting &instance();

    bool userUnderLine();
    bool ignoreCodeFontSize();
    bool useDarkMode();
    bool rightToLeft();

    QString previewFontString();
    QString previewCodeFontString();

    bool isValidFontString(const QString& fontString);
};

}

#endif // MARKDOWNPREVIEWSETTING_H
