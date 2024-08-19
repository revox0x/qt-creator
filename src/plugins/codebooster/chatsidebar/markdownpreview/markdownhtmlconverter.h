#ifndef MARKDOWNHTMLCONVERTER_H
#define MARKDOWNHTMLCONVERTER_H

#include <QString>
#include <QUrl>
#include <QFont>

#include "../qmarkdowntextedit/markdownhighlighter.h"
#include "misc.h"

class MarkdownHtmlConverter
{
public:
    MarkdownHtmlConverter();

    static QString toMarkdownHtml(QString str, bool codeMode = false);

private:
    static int nonOverlapCount(const QString &str, const QChar c = '`');
    static void highlightCode(QString &str, const QString &type, int cbCount);
    static Utils::Misc::ExternalImageHash *externalImageHash();

    static const QRegularExpression cssFontSetting;
    static const QRegularExpression deleteLine;

    static const QString codeBlockStyleSchema;
    static const QString contendStyleSchema;
};

#endif // MARKDOWNHTMLCONVERTER_H
