#pragma once

#include <coreplugin/inavigationwidgetfactory.h>
#include <utils/filepath.h>

#include <QPointer>
#include <QWidget>

namespace Utils {
class ElidingLabel;
class NavigationTreeView;
} // Utils


namespace CodeBooster::Internal {

class ChatView;

class ChatViewFactory : public Core::INavigationWidgetFactory
{
public:
    ChatViewFactory();
    ChatView *view() const;

private:
    Core::NavigationView createWidget() override;
    QPointer<ChatView> m_view;
};

void setupChatViewWidgetFactory();

}
