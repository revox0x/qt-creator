// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/navigationtreeview.h>

namespace Autotest {
namespace Internal {

class TestTreeView : public Utils::NavigationTreeView
{
    Q_OBJECT

public:
    explicit TestTreeView(QWidget *parent = nullptr);

    void selectAll() override;
    void deselectAll();
};

} // namespace Internal
} // namespace Autotest
