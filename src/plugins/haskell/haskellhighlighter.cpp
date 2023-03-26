// Copyright (c) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "haskellhighlighter.h"

#include "haskelltokenizer.h"

#include <texteditor/fontsettings.h>
#include <texteditor/texteditorconstants.h>
#include <texteditor/texteditorsettings.h>

#include <QDebug>
#include <QVector>

Q_GLOBAL_STATIC_WITH_ARGS(QSet<QString>, IMPORT_HIGHLIGHTS, ({
    "qualified",
    "as",
    "hiding"
}));

using namespace TextEditor;

namespace Haskell {
namespace Internal {

HaskellHighlighter::HaskellHighlighter()
{
    setDefaultTextFormatCategories();
    updateFormats(TextEditorSettings::fontSettings());
}

void HaskellHighlighter::highlightBlock(const QString &text)
{
    const Tokens tokens = HaskellTokenizer::tokenize(text, previousBlockState());
    setCurrentBlockState(tokens.state);
    const Token *firstNonWS = 0;
    const Token *secondNonWS = 0;
    bool inType = false;
    bool inImport = false;
    for (const Token & token : tokens) {
        switch (token.type) {
        case TokenType::Variable:
            if (inType)
                setTokenFormat(token, C_LOCAL);
            else if (inImport && IMPORT_HIGHLIGHTS->contains(token.text.toString()))
                setTokenFormat(token, C_KEYWORD);
//            else
//                setTokenFormat(token, C_TEXT);
            break;
        case TokenType::Constructor:
        case TokenType::OperatorConstructor:
            setTokenFormat(token, C_TYPE);
            break;
        case TokenType::Operator:
            setTokenFormat(token, C_OPERATOR);
            break;
        case TokenType::Whitespace:
            setTokenFormat(token, C_VISUAL_WHITESPACE);
            break;
        case TokenType::Keyword:
            if (token.text == QLatin1String("::") && firstNonWS && !secondNonWS) { // toplevel declaration
                setFormat(firstNonWS->startCol, firstNonWS->length, m_toplevelDeclFormat);
                inType = true;
            } else if (token.text == QLatin1String("import")) {
                inImport = true;
            }
            setTokenFormat(token, C_KEYWORD);
            break;
        case TokenType::Integer:
        case TokenType::Float:
            setTokenFormat(token, C_NUMBER);
            break;
        case TokenType::String:
            setTokenFormatWithSpaces(text, token, C_STRING);
            break;
        case TokenType::Char:
            setTokenFormatWithSpaces(text, token, C_STRING);
            break;
        case TokenType::EscapeSequence:
            setTokenFormat(token, C_PRIMITIVE_TYPE);
            break;
        case TokenType::SingleLineComment:
            setTokenFormatWithSpaces(text, token, C_COMMENT);
            break;
        case TokenType::MultiLineComment:
            setTokenFormatWithSpaces(text, token, C_COMMENT);
            break;
        case TokenType::Special:
//            setTokenFormat(token, C_TEXT);
            break;
        case TokenType::StringError:
        case TokenType::CharError:
        case TokenType::Unknown:
            setTokenFormat(token, C_PARENTHESES_MISMATCH);
            break;
        }
        if (token.type != TokenType::Whitespace) {
            if (!firstNonWS)
                firstNonWS = &token;
            else if (!secondNonWS)
                secondNonWS = &token;
        }
    }
}

void HaskellHighlighter::setFontSettings(const FontSettings &fontSettings)
{
    SyntaxHighlighter::setFontSettings(fontSettings);
    updateFormats(fontSettings);
}

void HaskellHighlighter::updateFormats(const FontSettings &fontSettings)
{
    m_toplevelDeclFormat = fontSettings.toTextCharFormat(
                TextStyles::mixinStyle(C_FUNCTION, C_DECLARATION));
}

void HaskellHighlighter::setTokenFormat(const Token &token, TextStyle style)
{
    setFormat(token.startCol, token.length, formatForCategory(style));
}

void HaskellHighlighter::setTokenFormatWithSpaces(const QString &text, const Token &token,
                                                  TextStyle style)
{
    setFormatWithSpaces(text, token.startCol, token.length, formatForCategory(style));
}

} // Internal
} // Haskell
