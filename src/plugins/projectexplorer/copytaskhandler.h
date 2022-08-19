// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "itaskhandler.h"

namespace ProjectExplorer {
namespace Internal {

class CopyTaskHandler : public ITaskHandler
{
    Q_OBJECT

public:
    CopyTaskHandler() : ITaskHandler(true) {}

    void handle(const Tasks &tasks) override;
    Utils::Id actionManagerId() const override;
    QAction *createAction(QObject *parent) const override;
};

} // namespace Internal
} // namespace ProjectExplorer
