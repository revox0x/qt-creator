#include "replyparser.h"

#include <QJsonArray>
#include <QJsonObject>

#include "promptbuilder.h"

namespace CodeBooster::Internal {
ReplyParser::ReplyParser() {}

/**
 * @brief ReplyParser::getMessagesFromReply 从回复中解析消息
 * @param model
 * @param reply
 * @param removeStopCode
 * @return
 */
QStringList ReplyParser::getMessagesFromReply(const QString &model,
                                              const QJsonObject &reply,
                                              bool removeStopCode)
{
    QStringList msgs;

    // GLM: https://open.bigmodel.cn/dev/api#glm-4
    // Deep-Seek: https://platform.deepseek.com/api-docs/zh-cn/api/create-chat-completion
    // Deep-Seek和GLM的消息体结构相同
    if (model.contains("glm") || model.contains("deepseek"))
    {
        /* reply示例
        {
          "created": 1703487403,
          "id": "8239375684858666781",
          "model": "glm-4",
          "request_id": "8239375684858666781",
          "choices": [
              {
                  "finish_reason": "stop",
                  "index": 0,
                  "message": {
                      "content": "智绘蓝图，AI驱动 —— 智谱AI，让每一刻创新成为可能。",
                      "role": "assistant"
                  }
              }
          ],
          "usage": {
              "completion_tokens": 217,
              "prompt_tokens": 31,
              "total_tokens": 248
          }
        }
         */

        QJsonArray choices = reply["choices"].toArray();
        for (const QJsonValue &choice : choices)
        {
            msgs.append(getContentFromChoice(choice, removeStopCode));
        }
    }
    // 通义千问：https://help.aliyun.com/zh/dashscope/developer-reference/api-details
    else if (model.contains("qwen"))
    {
        /* reply示例
        {
            "status_code": 200,
            "request_id": "5d768057-2820-91ba-8c99-31cd520e7628",
            "code": "",
            "message": "",
            "output": {
                "text": null,
                "finish_reason": null,
                "choices": [
                    {
                        "finish_reason": "stop",
                        "message": {
                            "role": "assistant",
                            "content": "材料：\n西红柿2个，鸡蛋3个，油适量，盐适量，糖适量，葱花适量。\n\n步骤：\n\n1. 鸡蛋打入碗中，加入少许盐，用筷子搅拌均匀，放置一会儿让蛋白和蛋黄充分融合。\n\n2. 西红柿洗净，切成小块。如果喜欢口感更沙一些，可以切得稍微大一些；如果喜欢口感细腻，可以切得小一些。\n\n3. 热锅凉油，油热后倒入打好的鸡蛋液，用铲子快速搅拌，炒至鸡蛋凝固并变成金黄色，盛出备用。\n\n4. 锅中再加一点油，放入切好的西红柿，用中小火慢慢翻煮，让西红柿出汁，这样炒出来的西红柿才会更甜。\n\n5. 西红柿出汁后，加入适量的糖，继续翻煮，直到西红柿变得软烂。\n\n6. 将炒好的鸡蛋倒回锅中，与西红柿一起翻煮均匀，让鸡蛋充分吸收西红柿的汁水。\n\n7. 最后，根据个人口味加入适量的盐调味，撒上葱花进行提香，翻炒均匀即可出锅。\n\n8. 如果喜欢汤汁多一些，可以适当加点水，调整一下浓稠度。\n\n西红柿炒鸡蛋就做好了，简单易做，营养美味，是一道家常菜的经典之作。"
                        }
                    }
                ]
            },
            "usage": {
                "input_tokens": 25,
                "output_tokens": 289,
                "total_tokens": 314
            }
        }
    */

        QJsonArray choices = reply["output"].toObject()["choices"].toArray();
        for (const QJsonValue &choice : choices)
        {
            msgs.append(getContentFromChoice(choice, removeStopCode));;
        }
    }
    else
    {
        // TODO: 记录日志
    }

    return msgs;
}

QString ReplyParser::chopStopCode(const QString &content)
{
    QString result = content;

    for (const auto &code : PromptBuilder::stopCodes())
    {
        if (result.size() > code.size())
        {
            // 将内容开头的停止符移除
            if (result.left(code.size()) == code)
            {
                result = result.mid(code.size());
            }
            // 将内容结尾的停止符移除
            else if (result.right(code.size()) == code)
            {
                result = result.left(result.size() - code.size());
            }
        }
    }

    return result;
}

QString ReplyParser::getContentFromChoice(const QJsonValue &choice, bool removeStopCode)
{
    QJsonObject choiceObj = choice.toObject();
    QString content;

    // 使用流式传输时返回的结果里面message被delta替换
    /*
    data: {"id":"069b737af3019f51e48dc46746192d98","choices":[{"index":0,"delta":{"content":"","role":"assistant"},"finish_reason":null,"logprobs":null}],"created":1717901403,"model":"deepseek-coder","system_fingerprint":"fp_ded2115e5a","object":"chat.completion.chunk","usage":null}
    data: [DONE]
     */

    if (choiceObj.contains("message"))
    {
        QJsonObject message = choiceObj["message"].toObject();
        content = message["content"].toString();
        if (removeStopCode)
            content = chopStopCode(content);
    }
    else if (choiceObj.contains("delta"))
    {
        QJsonObject delta = choiceObj["delta"].toObject();
        content = delta["content"].toString();
        if (removeStopCode)
            content = chopStopCode(content);
    }

    // 去除开头的空格
    content = content.trimmed();

    return content;
}


} // namespace CodeBooster::Internal
