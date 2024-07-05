#ifndef REPLYPARSER_H
#define REPLYPARSER_H

namespace CodeBooster::Internal {
class ReplyParser
{
public:
    ReplyParser();

    static QStringList getMessagesFromReply(const QString &model,
                                           const QJsonObject &reply,
                                           bool removeStopCode = true);

private:
    static QString chopStopCode(const QString &content);
    static QString getContentFromChoice(const QJsonValue &choice, bool removeStopCode);
};
}
#endif // REPLYPARSER_H
