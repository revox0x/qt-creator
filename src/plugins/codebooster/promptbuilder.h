#ifndef PROMPTBUILDER_H
#define PROMPTBUILDER_H

#include <QString>

namespace CodeBooster::Internal {
class PromptBuilder
{
public:
    PromptBuilder();

    static QString getCompletionPrompt(const QString &prefix, const QString &suffix);
    static QString systemMessage();
    static QStringList stopCodes();

};
}

#endif // PROMPTBUILDER_H
