// Copyright (C) 2019 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <qtsupport/qtversionfactory.h>

namespace Qdb::Internal {

class QdbQtVersionFactory : public QtSupport::QtVersionFactory
{
public:
    QdbQtVersionFactory();
};

} // Qdb::Internal
