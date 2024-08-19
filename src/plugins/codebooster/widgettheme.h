#ifndef WIDGETTHEME_H
#define WIDGETTHEME_H

#define CB_THEME Theme::instance()

namespace CodeBooster::Internal{

class Theme
{
public:
    Theme();

public:
    static Theme &instance();

    QString SS_ChatBackground; ///< 对话控件背景样式表

    QString SS_MessagePreview_ToolBar; ///< 单挑对话工具栏样式表

    QString SS_MessagePreview;
    QString SS_MessagePreview_UserTextBrowser; ///< 用户消息预览控件中的文本显示控件
    QString SS_MessagePreview_AssistantMainTextBrowser;
    QString SS_MessagePreview_AssistantCodeTextBrowser;

    QString SS_MarkdownBlockWidget_CodeMode;
    QString SS_MarkdownBlockWidget_CodeMode_PreWgt;
    QString SS_MarkdownBlockWidget_CodeToolBar;
    QString SS_MarkdownBlockWidget_CodeToolBar_Highlight;
    QString SS_MarkdownBlockWidget_CodeToolBar_Label;

    QString SS_InputWidget_CodeSnippet;

    QString Color_MarkdownBlockWidget_NomalBackground; ///< 文本块背景颜色
    QString Color_MarkdownBlockWidget_CodeBackground; ///< 代码块背景颜色
    QString Color_MarkdownBlockWidget_CodeLine; ///< 代码块控件分隔线颜色 
};

}
#endif // WIDGETTHEME_H
