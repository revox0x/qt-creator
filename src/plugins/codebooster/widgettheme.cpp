#include "widgettheme.h"

#include "codeboosterutils.h"

namespace CodeBooster::Internal{

Theme::Theme()
{
    // 暗色
    if (isDarkTheme())
    {
        SS_ChatBackground = QString("QWidget#scrollAreaWidgetContents {background: #252526}");

        SS_MessagePreview = QString("QWidget#MessagePreviewWidget {border: none; border-radius: 6px; background: #2e2f30}");
        SS_MessagePreview_ToolBar = QString("QWidget#MessagePreviewWidgetToolBar {border: none; border-radius: 6px;  background: #2e2f30}");
        SS_MessagePreview_UserTextBrowser = QString("QWidget#MessagePreviewWidget_mUserInput {border: none; border-radius: 6px; background: #2e2f30; color: #e1e1e1}");

        SS_MarkdownBlockWidget_CodeMode = QString("QWidget#MarkdownBlockWidget {border: none; background: #272727}");
        SS_MarkdownBlockWidget_CodeMode_PreWgt = R"(
QTextBrowser{
    border: none;
    background-color: #272727;
}

QScrollBar:vertical {
    border: 1px solid #333; /* 边框颜色 */
    background: #222; /* 背景颜色 */
    width: 10px;
    margin: 0; /* 外边距 */
}

QScrollBar::handle:vertical {
    background: #555; /* 滑块颜色 */
    min-height: 20px;
}

/* 隐藏滚动条的上下按钮 */
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    border: none;
    background: none;
    height: 0;
}

/* 隐藏滚动条的上下箭头 */
QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical {
    border: none;
    background: none;
    width: 0;
    height: 0;
}

QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: none;
}

/* 水平滚动条样式 */
QScrollBar:horizontal {
    border: 1px solid #333; /* 边框颜色 */
    background: #222; /* 背景颜色 */
    height: 10px;
    margin: 0; /* 外边距 */
}

QScrollBar::handle:horizontal {
    background: #555; /* 滑块颜色 */
    min-width: 20px;
}

/* 隐藏滚动条的左右按钮 */
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    border: none;
    background: none;
    width: 0;
}

/* 隐藏滚动条的左右箭头 */
QScrollBar::left-arrow:horizontal, QScrollBar::right-arrow:horizontal {
    border: none;
    background: none;
    width: 0;
    height: 0;
}

QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
    background: none;
})";
        SS_MarkdownBlockWidget_CodeToolBar = QString("QWidget#mToolBar {border: none; background: #272727}");
        SS_MarkdownBlockWidget_CodeToolBar_Highlight = QString("QWidget#mToolBar {border: none; background: #2e2f30}");
        SS_MarkdownBlockWidget_CodeToolBar_Label = QString("QLabel {color: #cccccc}");

        SS_InputWidget_CodeSnippet = QString("QFrame#CodeSnippetWidget {border: 1px solid #2e2f30; background: #272727}");

        Color_MarkdownBlockWidget_NomalBackground = QString("#2e2f30");
        Color_MarkdownBlockWidget_CodeBackground = QString("#272727");

        Color_MarkdownBlockWidget_CodeLine = QString("#414141");
    }
    // 亮色
    else
    {
        SS_ChatBackground = QString("QWidget#scrollAreaWidgetContents { background: #f3f3f3}");

        SS_MessagePreview = QString("QWidget#MessagePreviewWidget {border: none; border-radius: 6px; background: #e9e9e9}");
        SS_MessagePreview_ToolBar = QString("QWidget#MessagePreviewWidgetToolBar {border: none; border-radius: 6px;  background: #e9e9e9}");
        SS_MessagePreview_UserTextBrowser = QString("QWidget#MessagePreviewWidget_mUserInput {border: none; border-radius: 6px; background: #e9e9e9; color: #333333}");


        SS_MarkdownBlockWidget_CodeMode = QString("QWidget#MarkdownBlockWidget {border: none; background: #ffffff}");
        SS_MarkdownBlockWidget_CodeMode_PreWgt = R"(
QTextBrowser{
    border: none;
    background-color: #ffffff;
}

QScrollBar:vertical {
    border: 1px solid #ccc; /* 亮色边框 */
    background: #f0f0f0; /* 亮色背景 */
    width: 10px;
    margin: 0; /* 移除按钮后不需要额外的边距 */
}

QScrollBar::handle:vertical {
    background: #aaa; /* 亮色手柄 */
    min-height: 20px;
}

/* 移除两端的按钮 */
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    border: none;
    background: none;
    height: 0;
}

/* 移除箭头 */
QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical {
    border: none;
    background: none;
    width: 0;
    height: 0;
}

QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: none;
}

/* 水平滚动条样式 */
QScrollBar:horizontal {
    border: 1px solid #ccc; /* 亮色边框 */
    background: #f0f0f0; /* 亮色背景 */
    height: 10px;
    margin: 0; /* 移除按钮后不需要额外的边距 */
}

QScrollBar::handle:horizontal {
    background: #aaa; /* 亮色手柄 */
    min-width: 20px;
}

/* 移除两端的按钮 */
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    border: none;
    background: none;
    width: 0;
}

/* 移除箭头 */
QScrollBar::left-arrow:horizontal, QScrollBar::right-arrow:horizontal {
    border: none;
    background: none;
    width: 0;
    height: 0;
}

QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
    background: none;
})";
        SS_MarkdownBlockWidget_CodeToolBar = QString("QWidget#mToolBar {border: none; background: #ffffff}");
        SS_MarkdownBlockWidget_CodeToolBar_Highlight = QString("QWidget#mToolBar {border: none; background: #e9e9e9}");
        SS_MarkdownBlockWidget_CodeToolBar_Label = QString("QLabel {color: #616161}");

        SS_InputWidget_CodeSnippet = QString("QFrame#CodeSnippetWidget {border: 1px solid #e9e9e9; background: #ffffff}");

        Color_MarkdownBlockWidget_NomalBackground = QString("#e9e9e9");
        Color_MarkdownBlockWidget_CodeBackground = QString("#ffffff");

        Color_MarkdownBlockWidget_CodeLine = QString("#D3D3D3");
    }
}

Theme &Theme::instance()
{
    static Theme theme;
    return theme;
}

}
