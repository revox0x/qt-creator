// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "../luaengine.h"

#include <coreplugin/editormanager/editormanager.h>
#include <texteditor/textdocument.h>

namespace Lua {

namespace Internal {

void addHookModule()
{
    LuaEngine::registerHook(
        "editors.documentOpened", [](const sol::protected_function &func, QObject *guard) {
            QObject::connect(
                Core::EditorManager::instance(),
                &Core::EditorManager::documentOpened,
                guard,
                [func](Core::IDocument *document) {
                    Utils::expected_str<void> res = LuaEngine::void_safe_call(func, document);
                    QTC_CHECK_EXPECTED(res);
                });
        });

    LuaEngine::registerHook(
        "editors.documentClosed", [](const sol::protected_function &func, QObject *guard) {
            QObject::connect(
                Core::EditorManager::instance(),
                &Core::EditorManager::documentClosed,
                guard,
                [func](Core::IDocument *document) {
                    Utils::expected_str<void> res = LuaEngine::void_safe_call(func, document);
                    QTC_CHECK_EXPECTED(res);
                });
        });
}

} // namespace Internal

} // namespace Lua
