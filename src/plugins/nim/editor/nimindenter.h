// Copyright (C) Filippo Cucchetto <filippocucchetto@gmail.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/textindenter.h>

namespace Nim {

TextEditor::Indenter *createNimIndenter(QTextDocument *doc);

} // namespace Nim
