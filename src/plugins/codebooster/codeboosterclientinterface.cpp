#include "codeboosterclientinterface.h"

#include "codeboostersettings.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QJsonArray>
#include <coreplugin/messagemanager.h>

#include "promptbuilder.h"
#include "replyparser.h"
#include "codeboosterconstants.h"
#include "codeboosterutils.h"

namespace CodeBooster {
namespace Internal {

QMap<QString,QString> CodeBoosterClientInterface::m_langMap;

CodeBoosterClientInterface::CodeBoosterClientInterface()
    :LanguageClient::BaseClientInterface()
{
    if(m_langMap.isEmpty()){
        m_langMap.insert("abap","Abap");
        m_langMap.insert("c","C");
        m_langMap.insert("cpp","C++");
        m_langMap.insert("csharp","C#");
        m_langMap.insert("css","CSS");
        m_langMap.insert("dart","Dart");
        m_langMap.insert("dockerfile", "Dockerfile");
        m_langMap.insert("elixir","Elixir");
        m_langMap.insert("erlang","Erlang");
        m_langMap.insert("fsharp","F#");
        m_langMap.insert("go","Go");
        m_langMap.insert("groovy","Groovy");
        m_langMap.insert("html","HTML");
        m_langMap.insert("java","Java");
        m_langMap.insert("javascript","JavaScript");
        m_langMap.insert("lua","Lua");
        m_langMap.insert("markdown","Markdown");
        m_langMap.insert("objective-c","Objective-C");
        m_langMap.insert("objective-cpp","Objective-C++");
        m_langMap.insert("perl","Perl");
        m_langMap.insert("php","PHP");
        m_langMap.insert("powershell","PowerShell");
        m_langMap.insert("python","Python");
        m_langMap.insert("r","R");
        m_langMap.insert("ruby","Ruby");
        m_langMap.insert("rust","Rust");
        m_langMap.insert("scala","Scala");
        m_langMap.insert("shellscript","Shell");
        m_langMap.insert("sql","SQL");
        m_langMap.insert("swift","Swift");
        m_langMap.insert("typescript","TypeScript");
        m_langMap.insert("tex","TeX");
        m_langMap.insert("vb","Visual Basic");
    }
}

CodeBoosterClientInterface::~CodeBoosterClientInterface()
{
}

Utils::FilePath CodeBoosterClientInterface::serverDeviceTemplate() const
{
    return "";
}

void CodeBoosterClientInterface::replyFinished()
{
    if (m_reply == nullptr) {
        return;
    }

    // 处理错误
    if (m_reply->error() != QNetworkReply::NoError)
    {
        QString errInfos;
        errInfos = "请求错误：";

        // 获取HTTP状态码
        QVariant statusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (statusCode.isValid()) {
            errInfos += "HTTP status code：" + QString::number(statusCode.toInt()) + ";";
        }

        errInfos += QString(" Network error code: %1;").arg(m_reply->error());
        errInfos += QString(" Network error string: %1;").arg(m_reply->errorString());

        CodeBooster::Internal::outputMessages({errInfos}, Error);

        m_reply->disconnect();
        m_reply=nullptr;
    }
    else
    {
        QByteArray qba=m_reply->readAll();
        m_reply->disconnect();
        m_reply=nullptr;

        if(qba.isEmpty()){
            QJsonObject errorObj;
            errorObj.insert("code",-32603);
            errorObj.insert("message","Request failed!");
            QJsonObject obj;
            obj.insert("id",m_id);
            obj.insert("error",errorObj);
            LanguageServerProtocol::JsonRpcMessage errMsg(obj);
            emit messageReceived(errMsg);
            return;
        }
        QJsonParseError err;
        QJsonDocument doc=QJsonDocument::fromJson(qba,&err);
        if(err.error!=QJsonParseError::NoError){
            QJsonObject errorObj;
            errorObj.insert("code",-32603);
            errorObj.insert("message","Request failed!");
            QJsonObject obj;
            obj.insert("id",m_id);
            obj.insert("error",errorObj);
            LanguageServerProtocol::JsonRpcMessage errMsg(obj);
            emit messageReceived(errMsg);
        }else{

            // 返回的文本包含上下文及补全，这里找到插入的位置，提取出补全的字符
            // 如果返回的文本中只包含补全，则不必执行此操作
            QJsonObject obj=doc.object();

            // TODO
            QStringList replies = ReplyParser::getMessagesFromReply(CodeBoosterSettings::instance().acmParam().modelName, obj, true);
            mLastReplies = replies;

            QString str=replies.isEmpty() ? QString() : replies.first();

            if(CodeBoosterSettings::instance().braceBalance()){
                for(int i=0;i<str.length();i++){
                    const QChar &ch=str.at(i);
                    if(ch=='{'){
                        m_braceLevel++;
                    }else if(ch=='}'){
                        m_braceLevel--;
                        if(m_braceLevel<0){
                            int j;
                            for(j=i-1;j>=0;j--){
                                if(!str.at(j).isSpace()){
                                    break;
                                }
                            }
                            str=str.left(j+1);
                            break;
                        }
                    }
                }
            }
            QJsonObject responseRangeObj;
            responseRangeObj.insert("start",m_position);
            responseRangeObj.insert("end",m_position);

            QJsonArray responseAry;
            for (const QString &reply : replies)
            {
                QJsonObject responseSubObj;
                responseSubObj.insert("position",m_position);
                responseSubObj.insert("range",responseRangeObj);
                responseSubObj.insert("text",reply);
                responseSubObj.insert("displayText",reply);
                responseSubObj.insert("uuid",QUuid::createUuid().toString());

                responseAry.push_back(responseSubObj);
            }

            QJsonObject objResult;
            objResult.insert("completions",responseAry);
            QJsonObject responseObj;
            responseObj.insert("id",m_id);
            responseObj.insert("result",objResult);
            LanguageServerProtocol::JsonRpcMessage responseMsg(responseObj);
            emit messageReceived(responseMsg);
        }
    }
}

void CodeBoosterClientInterface::requestTimeout()
{
    // 显示超时提示
    ModelParam param = CodeBoosterSettings::instance().acmParam();

    QStringList msgs;
    msgs << "请求超时，请检查网络参数：";
    msgs << "Title: " + param.title;
    msgs << "Model: " + param.modelName;
    msgs << "apiUrl: " + param.apiUrl;
    msgs << "apiKey: " + param.apiKey;

    CodeBooster::Internal::outputMessages(msgs, Error);

    // 重置请求状态
    m_reply->abort();
    m_reply->disconnect();
    m_reply=nullptr;
}

void CodeBoosterClientInterface::sendData(const QByteArray &data)
{
    m_writeBuffer.open(QBuffer::Append);
    m_writeBuffer.write(data);
    m_writeBuffer.close();
    LanguageServerProtocol::BaseMessage baseMsg;
    QString parseError;
    m_writeBuffer.open(QBuffer::ReadWrite);
    LanguageServerProtocol::BaseMessage::parse(&m_writeBuffer,parseError,baseMsg);
    m_writeBuffer.close();
    if(baseMsg.isValid()){
        if(baseMsg.isComplete()){
            LanguageServerProtocol::JsonRpcMessage msg(baseMsg);
            QJsonObject objSend = msg.toJsonObject();

            qDebug() << Q_FUNC_INFO << objSend.value("method");


            if(objSend.value("method")=="initialize"){
                QJsonObject InfoObj;
                InfoObj.insert("name","Fake server for CodeBooster");
                InfoObj.insert("version","0.1");
                QJsonObject capObj;
                capObj.insert("completionProvider",QJsonObject());
                capObj.insert("textDocumentSync",0);
                QJsonObject objResult;
                objResult.insert("capabilities",capObj);
                objResult.insert("serverInfo",InfoObj);
                QJsonObject obj;
                obj.insert("id",objSend.value("id"));
                obj.insert("result",objResult);
                LanguageServerProtocol::JsonRpcMessage responseMsg(obj);
                emit messageReceived(responseMsg);
            }else if(objSend.value("method")=="shutdown"){
                clearReply();
                QJsonObject obj;
                obj.insert("id",objSend.value("id"));
                obj.insert("result",QJsonValue());
                LanguageServerProtocol::JsonRpcMessage responseMsg(obj);
                emit messageReceived(responseMsg);
            }else if(objSend.value("method")=="textDocument/didOpen"){
                QJsonObject docParams=objSend.value("params").toObject().value("textDocument").toObject();
                QString langCode=m_langMap.value(docParams.value("languageId").toString(),"None");
                if(langCode!="None"){
                    m_fileLang.insert(docParams.value("uri").toString(),langCode);
                }
            }else if(objSend.value("method")=="getCompletionsCycling"){

                if (completionModelConfigExist())
                {
                    getCompletionRequest(objSend);
                }
            }
            QByteArray &bufferRaw=m_writeBuffer.buffer();
            bufferRaw.remove(0,bufferRaw.indexOf(baseMsg.header())+baseMsg.header().size()+baseMsg.contentLength);
        }
    }else{
        QJsonObject errorObj;
        errorObj.insert("code",-32700);
        errorObj.insert("message",parseError);
        QJsonObject obj;
        obj.insert("id",QJsonValue());
        obj.insert("error",errorObj);
        LanguageServerProtocol::JsonRpcMessage errMsg(obj);
        emit messageReceived(errMsg);
    }
}

void CodeBoosterClientInterface::getCompletionRequest(const QJsonObject &objSend)
{
    clearReply();
    QJsonObject objParams=objSend.value("params").toObject();
    QString uriStr=objParams.value("doc").toObject().value("uri").toString();
    QString langCode=m_fileLang.value(uriStr,"None");


    m_pos=objParams.value("pos").toInt();
    m_position=objParams.value("doc").toObject().value("position");

    // 当前编辑文件的文本
    QString origTxt=objParams.value("txt").toString();

    // 鼠标光标位置之前的文本
    QString context=origTxt.left(m_pos);

    // 获取前缀
    QString prefix = getPrefix(origTxt);

    // 计算后缀的最大token数量
    int prefixTokens = countTokens(prefix);
    int maxTokens = CodeBoosterSettings::instance().acmMaxContextTokens();
    int maxSuffixTokens = maxTokens * CodeBoosterSettings::instance().maxSuffixPercentate();
    int suffixTokens = qMin(maxTokens - prefixTokens, maxSuffixTokens);

    // 通过最大token数量获取后缀，前缀按行截取，因此可能会出现实际token小于预设比例的情况
    // 这时可以将后缀的token数量设置为最大token和前缀token的差值
    QString suffix = getSuffix(origTxt, suffixTokens);

    QString prompt = PromptBuilder::getCompletionPrompt(prefix, suffix);

    // 统计文件中左右括号之间的差值
    if(CodeBoosterSettings::instance().braceBalance()){
        m_braceLevel=origTxt.count('{')-origTxt.count('}');
    }

    // 可选：针对C/C++语言展开头文件
    bool trimmed = true; // 设置为true强制不展开头文件
    if(langCode=="C"||langCode=="C++"){
        if(!trimmed&&CodeBoosterSettings::instance().expandHeaders()){
            QUrl fileURI(uriStr);
            if(fileURI.isLocalFile()){
                static const QRegularExpression reHeaderQ("#include\\s+\"([^\"]+)\"");
                static const QRegularExpression reHeaderA("#include\\s+<([^>]+)>");
                QFileInfo qfi(fileURI.toLocalFile());
                QStringList searchPaths;
                QDir dir=qfi.dir();
                searchPaths.push_back(dir.absolutePath());
                while(dir.cdUp()){
                    searchPaths.push_back(dir.absolutePath());
                }
                QStringList headerNames;
                QStringList headerStrings;
                {
                    QRegularExpressionMatchIterator it=reHeaderQ.globalMatch(context);
                    while(it.hasNext()){
                        QRegularExpressionMatch match=it.next();
                        QString header=match.captured(1);
                        if(header.isEmpty()){
                            continue;
                        }
                        headerNames.push_back(header);
                        headerStrings.push_back(match.captured(0));
                    }
                }
                {
                    QRegularExpressionMatchIterator it=reHeaderA.globalMatch(context);
                    while(it.hasNext()){
                        QRegularExpressionMatch match=it.next();
                        QString header=match.captured(1);
                        if(header.isEmpty()){
                            continue;
                        }
                        headerNames.push_back(header);
                        headerStrings.push_back(match.captured(0));
                    }
                }
                int maxLen = 8192;// todo: 通过token进行计算
                int space=maxLen-context.length();
                bool full=false;
                for(int i=0;i<headerNames.size();i++){
                    const QString &nm=headerNames.at(i);
                    QFileInfo tmpFI(nm);
                    if(tmpFI.baseName()==qfi.baseName()){
                        for(const QString &dirPath:searchPaths){
                            QFileInfo qfiHeader(dirPath+"/"+nm);
                            if(qfiHeader.exists()){
                                if(!expandHeader(context,headerStrings.at(i),dirPath+"/"+nm,space,m_pos)){
                                    i=headerNames.size();
                                    full=true;
                                    break;
                                }
                                break;
                            }
                        }
                    }
                }
                if(!full){
                    for(int i=0;i<headerNames.size();i++){
                        const QString &nm=headerNames.at(i);
                        QFileInfo tmpFI(nm);
                        if(tmpFI.baseName()==qfi.baseName()){
                            continue;
                        }
                        for(const QString &dirPath:searchPaths){
                            QFileInfo qfiHeader(dirPath+"/"+nm);
                            if(qfiHeader.exists()){
                                if(!expandHeader(context,headerStrings.at(i),dirPath+"/"+nm,space,m_pos)){
                                    i=headerNames.size();
                                    break;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    // end

    m_id=objSend.value("id");
    if(m_manager==nullptr){
        m_manager=QSharedPointer<QNetworkAccessManager>(new QNetworkAccessManager());
    }

    // 判断是否使用缓存
    bool useCompletionCache = false;
    if (!mLastReplies.isEmpty())
    {
        QString prefixTrimmed = QString(prefix).remove("\n").remove(' ');
        QString suffixTrimmed = QString(suffix).remove("\n").remove(' ');

        if ((mLastPrefixTxt == prefixTrimmed) && (mLastSuffixTxt == suffixTrimmed))
        {
            useCompletionCache = true;
        }
        else
        {
            mLastPrefixTxt = prefixTrimmed;
            mLastSuffixTxt = suffixTrimmed;
        }
    }
    // end

    if (!useCompletionCache)
    {
        // 构建网络请求
        QUrl url(CodeBoosterSettings::instance().acmParam().apiUrl);
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
        req.setRawHeader("Authorization", QString("Bearer %1").arg(CodeBoosterSettings::instance().acmParam().apiKey).toUtf8());

        // 构建请求数据
        QJsonObject obj = getRequsetData(prompt);
        //qDebug().noquote() << obj;

        m_reply = QSharedPointer<QNetworkReply>(m_manager->post(req,QJsonDocument(obj).toJson()));
        connect(m_reply.get(),&QNetworkReply::finished,this,&CodeBoosterClientInterface::replyFinished);

        // 设置超时定时器
        mTimeoutTimer.setSingleShot(true);
        connect(&mTimeoutTimer, &QTimer::timeout, this, &CodeBoosterClientInterface::requestTimeout);
        mTimeoutTimer.start(5000); // 5秒超时
    }
    else
    {
        useCacheToCompletion();
    }
}

void CodeBoosterClientInterface::clearReply()
{
    if(m_reply!=nullptr){
        m_reply->disconnect();
        m_reply=nullptr;
        QJsonObject errorObj;
        errorObj.insert("code",-32603);
        errorObj.insert("message","Request canceled.");
        QJsonObject obj;
        obj.insert("id",m_id);
        obj.insert("error",errorObj);
        LanguageServerProtocol::JsonRpcMessage errMsg(obj);
        emit messageReceived(errMsg);
    }
}

bool CodeBoosterClientInterface::expandHeader(QString &txt, const QString &includeStr, const QString &path, int &space, int &pos)
{
    QFileInfo qfiHeader(path);
    if(qfiHeader.size()-includeStr.length()>space){
        return false;
    }
    int ind=txt.indexOf(includeStr);
    if(ind<0){
        return false;
    }
    QFile qf(path);
    QTextStream qts(&qf);
    qf.open(QIODevice::ReadOnly|QIODevice::Text);
    QString content=qts.readAll();
    qf.close();
    txt.replace(ind,includeStr.length(),content);
    space-=(content.length()-includeStr.length());
    pos+=(content.length()-includeStr.length());
    return true;
}

/**
 * @brief CodeBoosterClientInterface::completionModelConfigExist 检查代码补全的模型参数是否存在
 * @return
 */
bool CodeBoosterClientInterface::completionModelConfigExist()
{
    ModelParam param = CodeBoosterSettings::instance().acmParam();
    if (param.apiKey.isEmpty() || param.apiUrl.isEmpty() || param.modelName.isEmpty())
    {
        CodeBooster::Internal::outputMessages({"请配置代码补全模型参数"}, Error);
        return false;
    }

    return true;
}

void CodeBoosterClientInterface::useCacheToCompletion()
{
    if (mLastReplies.isEmpty())
        return;

    if (mLastReplies.first().isEmpty())
        return;

    QJsonObject responseRangeObj;
    responseRangeObj.insert("start",m_position);
    responseRangeObj.insert("end",m_position);

    QJsonArray responseAry;
    for (QString reply : mLastReplies)
    {
        QJsonObject responseSubObj;
        responseSubObj.insert("position",m_position);
        responseSubObj.insert("range",responseRangeObj);
        responseSubObj.insert("text",reply);
        responseSubObj.insert("displayText",reply);
        responseSubObj.insert("uuid",QUuid::createUuid().toString());

        responseAry.push_back(responseSubObj);
    }

    QJsonObject objResult;
    objResult.insert("completions",responseAry);
    QJsonObject responseObj;
    responseObj.insert("id",m_id);
    responseObj.insert("result",objResult);
    LanguageServerProtocol::JsonRpcMessage responseMsg(responseObj);
    emit messageReceived(responseMsg);

    qDebug() << Q_FUNC_INFO << "使用缓存进行补全";
}

int CodeBoosterClientInterface::countTokens(const QString &prompt)
{
    // TODO: 使用第三方库计算token数量
    // 通过使用在线平台（https://platform.openai.com/tokenizer）验证，
    // 一个token在代码中大概对应3.5~5个字符之间，这里取用一个保守的值，
    // 计算得到的token数量在大部分情况下都小于实际数量
    double ratio = 3.8;
    return (int)(prompt.size() / ratio) + 1;
}

/**
 * @brief CodeBoosterClientInterface::getPrefix 获取补全位置前缀上下文
 * @param originText                          整个文件的文本
 * @return
 */
QString CodeBoosterClientInterface::getPrefix(const QString &originText)
{
    int maxTokens = CodeBoosterSettings::instance().acmMaxContextTokens();
    double prefixTokenPercentage = CodeBoosterSettings::instance().prefixPercentage();

    int maxPrefixTokens = maxTokens * prefixTokenPercentage;
    QString fullPrefix = originText.left(m_pos);

    int prefixTokens = countTokens(fullPrefix);

    QStringList lines = fullPrefix.split("\n");
    while (prefixTokens > maxPrefixTokens && lines.length() > 0)
    {
        QString firstLine = lines.first();
        prefixTokens -= countTokens(firstLine);
        lines.removeFirst();
    }

    return lines.join("\n");
}

/**
 * @brief CodeBoosterClientInterface::getSuffix 获取补全位置后缀上下文
 * @param originText                          整个文件的文本
 * @return
 */
QString CodeBoosterClientInterface::getSuffix(const QString &originText, int maxSuffixTokens)
{
    // 从指定的位置截取剩余的文本
    QString fullSuffix = originText.mid(m_pos);

    int suffixTokens = countTokens(fullSuffix);

    QStringList lines = fullSuffix.split("\n");
    while (suffixTokens > maxSuffixTokens && lines.length() > 0) {
        QString lastLine = lines.last();
        suffixTokens -= countTokens(lastLine);
        lines.removeLast();
    }

    return lines.join("\n");
}

QJsonObject CodeBoosterClientInterface::getRequsetData(const QString &prompt)
{
    QJsonObject data = CodeBoosterSettings::buildRequestParamJson(CodeBoosterSettings::instance().acmParam(), false);

    // 构建消息
    QJsonObject systemMsg;
    systemMsg.insert("role", "system");
    systemMsg.insert("content", PromptBuilder::systemMessage());
    QJsonObject userMsg;
    userMsg.insert("role", "user");
    userMsg.insert("content", prompt);

    QJsonArray msg{systemMsg, userMsg};

    data.insert("messages", msg);

    return data;
}


} // namespace Internal
} // namespace CodeBooster
