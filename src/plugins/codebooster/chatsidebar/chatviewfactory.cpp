#include "chatviewfactory.h"

#include <coreplugin/actionmanager/command.h>
#include <coreplugin/inavigationwidgetfactory.h>

#include "codeboosterconstants.h"
#include "codeboostertr.h"
#include "codeboostericons.h"
#include "chatview.h"
#include "chatdatabase.h"

using namespace Core;

namespace CodeBooster::Internal {

ChatViewFactory::ChatViewFactory()
{
    setDisplayName(Tr::tr("Code Booster"));
    setPriority(500);
    setId(Constants::CODEBOOSTER_CHAT_VIEW_ID);
    setActivationSequence(QKeySequence(useMacShortcuts ? Tr::tr("Meta+L") : Tr::tr("Alt+L")));

    // 初始化对话数据库
    ChatDatabase::instance();
}

NavigationView ChatViewFactory::createWidget()
{
    m_view = new ChatView();
    return {m_view, m_view->createToolButtons()};
}

ChatView *ChatViewFactory::view() const
{
    return m_view;
}

void setupChatViewWidgetFactory()
{
    static ChatViewFactory theChatViewFactory;
}

}
