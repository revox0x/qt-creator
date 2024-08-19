#include "markdownpreviewsetting.h"

#include <QFont>
#include <QStringList>
#include <QFontDatabase>
#include <QDebug>

namespace MarkdownPreview {

MarkdownPreviewSetting::MarkdownPreviewSetting()
{
}


MarkdownPreviewSetting &MarkdownPreviewSetting::instance()
{
    static MarkdownPreviewSetting settings;
    return settings;
}

bool MarkdownPreviewSetting::userUnderLine()
{
    return false;
}

bool MarkdownPreviewSetting::ignoreCodeFontSize()
{
    return true;
}

bool MarkdownPreviewSetting::useDarkMode()
{
    // return false;
    return true;
}

bool MarkdownPreviewSetting::rightToLeft()
{
    return false;
}

QString MarkdownPreviewSetting::previewFontString()
{
    QString font = "Microsoft YaHei UI,9,-1,5,400,0,0,0,0,0,0,0,0,0,0,1";
    return font;
}

QString MarkdownPreviewSetting::previewCodeFontString()
{
    QString font = "Consolas,9,-1,2,400,0,0,0,0,0,0,0,0,0,0,1";
    return font;
}

bool MarkdownPreviewSetting::isValidFontString(const QString& fontString)
{
    QStringList fontDetails = fontString.split(',');
    if (fontDetails.size() != 16) {
        return false;
    }

    QString family = fontDetails[0];

    // 使用 QFontDatabase 的静态方法检查字体家族是否存在
    if (!QFontDatabase::hasFamily(family)) {
        return false;
    }

    return true;
}

}

