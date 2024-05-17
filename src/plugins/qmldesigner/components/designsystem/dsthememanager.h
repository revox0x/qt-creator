// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <qmldesignercorelib_global.h>

#include "dsconstants.h"

#include <modelnode.h>

namespace QmlDesigner {
class DSThemeGroup;

using ThemeName = PropertyName;

class DSTheme;
class QMLDESIGNERCORE_EXPORT DSThemeManager
{

public:
    DSThemeManager();
    ~DSThemeManager();

    std::optional<ThemeId> addTheme(const ThemeName &themeName);
    std::optional<ThemeId> themeId(const ThemeName &themeName) const;
    void removeTheme(ThemeId id);
    size_t themeCount() const;

    void duplicateTheme(ThemeId from, ThemeId to);

    bool addProperty(GroupType gType, const ThemeProperty &p);
    std::optional<ThemeProperty> property(ThemeId themeId,
                                               GroupType gType,
                                               const PropertyName &name) const;
    void removeProperty(GroupType gType, const PropertyName &p);
    void updateProperty(ThemeId id, GroupType gType, const ThemeProperty &p);
    void updateProperty(ThemeId id, GroupType gType, const ThemeProperty &p, const PropertyName &newName);

    void decorate(ModelNode rootNode) const;

private:
    DSThemeGroup *propertyGroup(GroupType type);
    void addGroupAliases(ModelNode rootNode) const;

private:
    std::map<ThemeId, ThemeName> m_themes;
    std::map<GroupType, std::unique_ptr<DSThemeGroup>> m_groups;
};
}
