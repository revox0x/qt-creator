#ifndef CODEGEEX2_INTERNAL_CODEGEEX2CLIENTINTERFACE_H
#define CODEGEEX2_INTERNAL_CODEGEEX2CLIENTINTERFACE_H

#include <languageclient/languageclientinterface.h>

#include <QNetworkAccessManager>

class QNetworkReply;

namespace CodeBooster {
namespace Internal {

class CodeBoosterClientInterface : public LanguageClient::BaseClientInterface
{
public:
    CodeBoosterClientInterface();
    ~CodeBoosterClientInterface();

    Utils::FilePath serverDeviceTemplate() const override;

public slots:
    void replyFinished();

protected:
    void sendData(const QByteArray &data) override;
private:
    QBuffer m_writeBuffer;
    QSharedPointer<QNetworkReply> m_reply;
    QJsonValue m_id;
    int m_pos;
    QJsonValue m_position;
    int m_braceLevel;
    QMap<QString,QString> m_fileLang;

    QSharedPointer<QNetworkAccessManager> m_manager;

    void clearReply();
    bool expandHeader(QString &txt,const QString &includeStr,const QString &path,int &space,int &pos);

    static QMap<QString,QString> m_langMap;


    // 补全缓存
    QString mLastPrefixTxt; ///< 上次补全请求的鼠标位置之前的文本
    QString mLastSuffixTxt; ///< 上次补全请求鼠标之后的文本
    QStringList mLastReplies; ///< 上次补全请求的结果
    void useCacheToCompletion();
    // end
private:
    int countTokens(const QString &prompt);

    QString getPrefix(const QString &originText);
    QString getSuffix(const QString &originText, int maxSuffixTokens);
    QJsonObject getRequsetData(const QString &prompt);

};

} // namespace Internal
} // namespace CodeBooster

#endif // CODEGEEX2_INTERNAL_CODEGEEX2CLIENTINTERFACE_H
