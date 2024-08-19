#ifndef CODEBOOSTERUTILS_H
#define CODEBOOSTERUTILS_H

#include <QFile>
#include <QTextStream>
#include <QDir>

#include <QStringList>
#include <coreplugin/messagemanager.h>
#include <utils/theme/theme.h>
#include <utils/stringutils.h>
#include <QStandardPaths>
#include <QDateTime>

#include "codeboosterconstants.h"

namespace CodeBooster::Internal{

enum MessageType
{
    Normal,
    Sucess,
    Error
};

static QString addPrefix(const QString &str, MessageType type)
{
    Utils::Theme::Color color = Utils::Theme::Token_Text_Muted;
    if (Sucess == type) color = Utils::Theme::Token_Notification_Success;
    else if (Error == type) color = Utils::Theme::Token_Notification_Danger;

    const QString prefix =
        Utils::ansiColoredText(Constants::OUTPUT_PREFIX, Utils::creatorColor(color));
    return prefix + str;
}

static void outputMessages(const QStringList &messages, MessageType type = Normal)
{
    for (const QString &msg : messages)
    {
        //QString message = Constants::OUTPUT_PREFIX + msg;
        QString message = addPrefix(msg, type);
        Core::MessageManager::writeDisrupting(message);
    }
}

static void outputMessage(const QString &message, MessageType type = Normal)
{
    outputMessages({message}, type);
}

static bool isDarkTheme()
{
    static bool darkTheme = Utils::creatorTheme() && Utils::creatorTheme()->flag(Utils::Theme::DarkUserInterface);
    return darkTheme;
}

static QString dataFolderPath()
{
    // 获取AppData/Local目录的路径
    static QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
        "/CodeBooster";
    return path;
}

/**
 * @brief saveToTxtFile 将文本保存在文件中
 * @param text
 * @note 调试用的功能
 */
static void saveToTxtFile(const QString &text)
{
    // 获取当前时间并格式化为字符串
    QString currentDateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");

    // 指定保存的文件路径，使用当前时间作为文件名
    QString filePath = QApplication::applicationDirPath() + "/output_" + currentDateTime + ".txt";

    // 创建一个QFile对象
    QFile file(filePath);

    // 打开文件，如果文件不存在则会创建新文件
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        // 创建一个QTextStream对象，用于写入文件
        QTextStream out(&file);

        // 将result文本写入文件
        out << text;

        // 关闭文件
        file.close();

        // 输出成功信息
        qDebug() << "文件保存成功:" << filePath;
    } else {
        // 输出错误信息
        qDebug() << "无法打开文件进行写入:" << filePath;
    }
}

static QString languageFromFileSuffix(const QString &fileSuffix)
{
    static const QMap<QString, QString> suffixToLanguage = {
        {"cpp", "cpp"},
        {"c", "c"},
        {"hpp", "cpp"},
        {"h", "cpp"},
        {"pro", "pro"}
    };

    return suffixToLanguage.value(fileSuffix, QString());
}

}

#endif // CODEBOOSTERUTILS_H
