// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cpphighlighter.h"

#include "cppdoxygen.h"
#include "cppeditorlogging.h"
#include "cpptoolsreuse.h"

#include <extensionsystem/iplugin.h>
#include <texteditor/textdocumentlayout.h>
#include <utils/algorithm.h>
#include <utils/textutils.h>

#include <cplusplus/SimpleLexer.h>
#include <cplusplus/Lexer.h>

#include <QFile>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextLayout>

#ifdef WITH_TESTS
#include "cppeditorwidget.h"
#include "cpptoolstestcase.h"
#include <QtTest>
#include <utility>
#endif

using namespace TextEditor;
using namespace CPlusPlus;

namespace CppEditor {
using namespace Internal;

CppHighlighter::CppHighlighter(QTextDocument *document) :
    SyntaxHighlighter(document)
{
    setDefaultTextFormatCategories();
}

void CppHighlighter::highlightBlock(const QString &text)
{
    qCDebug(highlighterLog) << "highlighting line" << (currentBlock().blockNumber() + 1);

    const int previousBlockState_ = previousBlockState();
    int lexerState = 0, initialBraceDepth = 0;
    if (previousBlockState_ != -1) {
        lexerState = previousBlockState_ & 0xff;
        initialBraceDepth = previousBlockState_ >> 8;
        qCDebug(highlighterLog) << "initial brace depth carried over from previous block"
                                << initialBraceDepth;
    } else {
        qCDebug(highlighterLog) << "initial brace depth 0";
    }

    int braceDepth = initialBraceDepth;

    SimpleLexer tokenize;
    tokenize.setLanguageFeatures(m_languageFeatures);
    const QTextBlock prevBlock = currentBlock().previous();
    QByteArray inheritedRawStringSuffix;
    if (prevBlock.isValid()) {
        inheritedRawStringSuffix = TextDocumentLayout::expectedRawStringSuffix(prevBlock);
        tokenize.setExpectedRawStringSuffix(inheritedRawStringSuffix);
    }

    int initialLexerState = lexerState;
    const Tokens tokens = tokenize(text, initialLexerState);
    lexerState = tokenize.state(); // refresh lexer state

    static const auto lexerStateWithoutNewLineExpectedBit = [](int state) { return state & ~0x80; };
    initialLexerState = lexerStateWithoutNewLineExpectedBit(initialLexerState);
    int foldingIndent = initialBraceDepth;
    qCDebug(highlighterLog) << "folding indent initialized to brace depth" << foldingIndent;
    if (TextBlockUserData *userData = TextDocumentLayout::textUserData(currentBlock())) {
        qCDebug(highlighterLog) << "resetting stored folding data for current block";
        userData->setFoldingIndent(0);
        userData->setFoldingStartIncluded(false);
        userData->setFoldingEndIncluded(false);
    }

    if (tokens.isEmpty()) {
        setCurrentBlockState((braceDepth << 8) | lexerState);
        TextDocumentLayout::clearParentheses(currentBlock());
        if (!text.isEmpty())  {// the empty line can still contain whitespace
            if (initialLexerState == T_COMMENT)
                setFormatWithSpaces(text, 0, text.length(), formatForCategory(C_COMMENT));
            else if (initialLexerState == T_DOXY_COMMENT)
                setFormatWithSpaces(text, 0, text.length(), formatForCategory(C_DOXYGEN_COMMENT));
            else
                setFormat(0, text.length(), formatForCategory(C_VISUAL_WHITESPACE));
        }
        TextDocumentLayout::setFoldingIndent(currentBlock(), foldingIndent);
        TextDocumentLayout::setExpectedRawStringSuffix(currentBlock(), inheritedRawStringSuffix);
        qCDebug(highlighterLog) << "no tokens, storing brace depth" << braceDepth << "and foldingIndent"
                     << foldingIndent;
        return;
    }

    // Keep "semantic parentheses".
    Parentheses parentheses;
    if (TextBlockUserData *userData = TextDocumentLayout::textUserData(currentBlock())) {
        parentheses = Utils::filtered(userData->parentheses(), [](const Parenthesis &p) {
            return p.source.isValid();
        });
    }

    const auto insertParen = [&parentheses](const Parenthesis &p) { insertSorted(parentheses, p); };
    parentheses.reserve(5);

    bool expectPreprocessorKeyword = false;
    bool onlyHighlightComments = false;

    for (int i = 0; i < tokens.size(); ++i) {
        const bool isLastToken = i == tokens.size() - 1;
        const Token &tk = tokens.at(i);

        int previousTokenEnd = 0;
        if (i != 0) {
            inheritedRawStringSuffix.clear();

            // mark the whitespaces
            previousTokenEnd = tokens.at(i - 1).utf16charsBegin() +
                               tokens.at(i - 1).utf16chars();
        }

        if (previousTokenEnd != tk.utf16charsBegin()) {
            setFormat(previousTokenEnd,
                      tk.utf16charsBegin() - previousTokenEnd,
                      formatForCategory(C_VISUAL_WHITESPACE));
        }

        if (tk.is(T_LPAREN) || tk.is(T_LBRACE) || tk.is(T_LBRACKET)) {
            const QChar c = text.at(tk.utf16charsBegin());
            insertParen({Parenthesis::Opened, c, tk.utf16charsBegin()});
            if (tk.is(T_LBRACE)) {
                ++braceDepth;
                qCDebug(highlighterLog) << "encountered opening brace, increasing brace depth to" << braceDepth;

                // if a folding block opens at the beginning of a line, treat the line before
                // as if it were inside the folding block except if it is a comment or the line does
                // end with ;
                const int firstNonSpace = tokens.first().utf16charsBegin();
                const QString prevBlockText = currentBlock().previous().isValid()
                                                  ? currentBlock().previous().text().trimmed()
                                                  : QString();
                if (!prevBlockText.isEmpty() && !prevBlockText.startsWith("//")
                    && !prevBlockText.endsWith("*/") && !prevBlockText.endsWith(";")
                    && tk.utf16charsBegin() == firstNonSpace) {
                    ++foldingIndent;
                    TextDocumentLayout::userData(currentBlock())->setFoldingStartIncluded(true);
                    qCDebug(highlighterLog)
                        << "folding character is first on one line, increase folding indent to"
                        << foldingIndent << "and set foldingStartIncluded in stored data";
                }
            }
        } else if (tk.is(T_RPAREN) || tk.is(T_RBRACE) || tk.is(T_RBRACKET)) {
            const QChar c = text.at(tk.utf16charsBegin());
            insertParen({Parenthesis::Closed, c, tk.utf16charsBegin()});
            if (tk.is(T_RBRACE)) {
                --braceDepth;
                qCDebug(highlighterLog) << "encountered closing brace, decreasing brace depth to" << braceDepth;
                if (braceDepth < foldingIndent) {
                    // unless we are at the end of the block, we reduce the folding indent
                    if (isLastToken || tokens.at(i + 1).is(T_SEMICOLON)) {
                        qCDebug(highlighterLog) << "token is last token in statement or line, setting "
                                        "foldingEndIncluded in stored data";
                        TextDocumentLayout::userData(currentBlock())->setFoldingEndIncluded(true);
                    } else {
                        foldingIndent = qMin(braceDepth, foldingIndent);
                        qCDebug(highlighterLog) << "setting folding indent to minimum of current value and "
                                        "brace depth, which is"
                                     << foldingIndent;
                    }
                }
            }
        }

        bool highlightCurrentWordAsPreprocessor = expectPreprocessorKeyword;

        if (expectPreprocessorKeyword)
            expectPreprocessorKeyword = false;

        if (onlyHighlightComments && !tk.isComment())
            continue;

        if (i == 0 && tk.is(T_POUND)) {
            setFormatWithSpaces(text, tk.utf16charsBegin(), tk.utf16chars(),
                          formatForCategory(C_PREPROCESSOR));
            expectPreprocessorKeyword = true;
        } else if (highlightCurrentWordAsPreprocessor && (tk.isKeyword() || tk.is(T_IDENTIFIER))
                   && isPPKeyword(QStringView(text).mid(tk.utf16charsBegin(), tk.utf16chars()))) {
            setFormat(tk.utf16charsBegin(), tk.utf16chars(), formatForCategory(C_PREPROCESSOR));
            const QStringView ppKeyword = QStringView(text).mid(tk.utf16charsBegin(), tk.utf16chars());
            if (ppKeyword == QLatin1String("error")
                    || ppKeyword == QLatin1String("warning")
                    || ppKeyword == QLatin1String("pragma")) {
                onlyHighlightComments = true;
            }

        } else if (tk.is(T_NUMERIC_LITERAL)) {
            setFormat(tk.utf16charsBegin(), tk.utf16chars(), formatForCategory(C_NUMBER));
        } else if (tk.isStringLiteral() || tk.isCharLiteral()) {
            if (!highlightRawStringLiteral(text, tk, QString::fromUtf8(inheritedRawStringSuffix)))
                highlightStringLiteral(text, tk);
        } else if (tk.isComment()) {
            const int startPosition = initialLexerState ? previousTokenEnd : tk.utf16charsBegin();
            if (tk.is(T_COMMENT) || tk.is(T_CPP_COMMENT)) {
                setFormatWithSpaces(text, startPosition, tk.utf16charsEnd() - startPosition,
                              formatForCategory(C_COMMENT));
            }

            else // a doxygen comment
                highlightDoxygenComment(text, startPosition, tk.utf16charsEnd() - startPosition);

            // we need to insert a close comment parenthesis, if
            //  - the line starts in a C Comment (initalState != 0)
            //  - the first token of the line is a T_COMMENT (i == 0 && tk.is(T_COMMENT))
            //  - is not a continuation line (tokens.size() > 1 || !state)
            if (initialLexerState && i == 0 && (tk.is(T_COMMENT) || tk.is(T_DOXY_COMMENT))
                && (tokens.size() > 1 || !lexerState)) {
                --braceDepth;
                qCDebug(highlighterLog)
                    << "encountered some comment-related condition, decreasing brace depth to"
                    << braceDepth;
                // unless we are at the end of the block, we reduce the folding indent
                if (isLastToken) {
                    qCDebug(highlighterLog) << "token is last token on line, setting "
                                    "foldingEndIncluded in stored data";
                    TextDocumentLayout::userData(currentBlock())->setFoldingEndIncluded(true);
                } else {
                    foldingIndent = qMin(braceDepth, foldingIndent);
                    qCDebug(highlighterLog) << "setting folding indent to minimum of current value and "
                                    "brace depth, which is"
                                 << foldingIndent;
                }
                const int tokenEnd = tk.utf16charsBegin() + tk.utf16chars() - 1;
                insertParen({Parenthesis::Closed, QLatin1Char('-'), tokenEnd});

                // clear the initial state.
                initialLexerState = 0;
            }

        } else if (tk.isKeyword()
                   || (m_languageFeatures.qtKeywordsEnabled
                       && isQtKeyword(
                           QStringView{text}.mid(tk.utf16charsBegin(), tk.utf16chars())))
                   || (m_languageFeatures.objCEnabled && tk.isObjCAtKeyword())) {
            setFormat(tk.utf16charsBegin(), tk.utf16chars(), formatForCategory(C_KEYWORD));
        } else if (tk.isPrimitiveType()) {
            setFormat(tk.utf16charsBegin(), tk.utf16chars(),
                      formatForCategory(C_PRIMITIVE_TYPE));
        } else if (tk.isOperator()) {
            setFormat(tk.utf16charsBegin(), tk.utf16chars(), formatForCategory(C_OPERATOR));
        } else if (tk.isPunctuation()) {
            setFormat(tk.utf16charsBegin(), tk.utf16chars(), formatForCategory(C_PUNCTUATION));
        } else if (i == 0 && tokens.size() > 1 && tk.is(T_IDENTIFIER) && tokens.at(1).is(T_COLON)) {
            setFormat(tk.utf16charsBegin(), tk.utf16chars(), formatForCategory(C_LABEL));
        } else if (tk.is(T_IDENTIFIER)) {
            highlightWord(QStringView(text).mid(tk.utf16charsBegin(), tk.utf16chars()),
                          tk.utf16charsBegin(),
                          tk.utf16chars());
        }
    }

    // rehighlight the next block if it contains a folding marker since we move the folding
    // marker in some cases and we need to rehighlight the next block to update this floding indent
    int rehighlightNextBlock = 0;
    if (const QTextBlock nextBlock = currentBlock().next(); nextBlock.isValid()) {
        if (const auto nextData = TextDocumentLayout::textUserData(nextBlock)) {
            if (const auto foldingCheckData = TextDocumentLayout::textUserData(nextBlock.next())) {
                if (foldingCheckData->foldingIndent() > nextData->foldingIndent()) {
                    static const int rehighlightNextBlockMask = 1 << 24;
                    if (!(currentBlockState() & rehighlightNextBlockMask))
                        rehighlightNextBlock = rehighlightNextBlockMask;
                }
            }
        }
    }

    // mark the trailing white spaces
    const int lastTokenEnd = tokens.last().utf16charsEnd();
    if (text.length() > lastTokenEnd)
        formatSpaces(text, lastTokenEnd, text.length() - lastTokenEnd);

    if (!initialLexerState && lexerStateWithoutNewLineExpectedBit(lexerState)
        && !tokens.isEmpty()) {
        const Token &lastToken = tokens.last();
        if (lastToken.is(T_COMMENT) || lastToken.is(T_DOXY_COMMENT)) {
            insertParen({Parenthesis::Opened, QLatin1Char('+'), lastToken.utf16charsBegin()});
            ++braceDepth;
            qCDebug(highlighterLog)
                << "encountered some comment-related condition, increasing brace depth to"
                << braceDepth;
        }
    }

    TextDocumentLayout::setParentheses(currentBlock(), parentheses);

    TextDocumentLayout::setFoldingIndent(currentBlock(), foldingIndent);
    setCurrentBlockState(rehighlightNextBlock | (braceDepth << 8) | tokenize.state());
    qCDebug(highlighterLog) << "storing brace depth" << braceDepth << "and folding indent" << foldingIndent;

    TextDocumentLayout::setExpectedRawStringSuffix(currentBlock(),
                                                   tokenize.expectedRawStringSuffix());
}

void CppHighlighter::setLanguageFeaturesFlags(unsigned int flags)
{
    if (flags != m_languageFeatures.flags) {
        m_languageFeatures.flags = flags;
        rehighlight();
    }
}

bool CppHighlighter::isPPKeyword(QStringView text) const
{
    switch (text.length())
    {
    case 2:
        if (text.at(0) == QLatin1Char('i') && text.at(1) == QLatin1Char('f'))
            return true;
        break;

    case 4:
        if (text.at(0) == QLatin1Char('e')
            && (text == QLatin1String("elif") || text == QLatin1String("else")))
            return true;
        break;

    case 5:
        switch (text.at(0).toLatin1()) {
        case 'i':
            if (text == QLatin1String("ifdef"))
                return true;
            break;
          case 'u':
            if (text == QLatin1String("undef"))
                return true;
            break;
        case 'e':
            if (text == QLatin1String("endif") || text == QLatin1String("error"))
                return true;
            break;
        }
        break;

    case 6:
        switch (text.at(0).toLatin1()) {
        case 'i':
            if (text == QLatin1String("ifndef") || text == QLatin1String("import"))
                return true;
            break;
        case 'd':
            if (text == QLatin1String("define"))
                return true;
            break;
        case 'p':
            if (text == QLatin1String("pragma"))
                return true;
            break;
        }
        break;

    case 7:
        switch (text.at(0).toLatin1()) {
        case 'i':
            if (text == QLatin1String("include"))
                return true;
            break;
        case 'w':
            if (text == QLatin1String("warning"))
                return true;
            break;
        }
        break;

    case 12:
        if (text.at(0) == QLatin1Char('i') && text == QLatin1String("include_next"))
            return true;
        break;

    default:
        break;
    }

    return false;
}

void CppHighlighter::highlightWord(QStringView word, int position, int length)
{
    // try to highlight Qt 'identifiers' like QObject and Q_PROPERTY

    if (word.length() > 2 && word.at(0) == QLatin1Char('Q')) {
        if (word.at(1) == QLatin1Char('_') // Q_
            || (word.at(1) == QLatin1Char('T') && word.at(2) == QLatin1Char('_'))) { // QT_
            for (int i = 1; i < word.length(); ++i) {
                const QChar &ch = word.at(i);
                if (!(ch.isUpper() || ch == QLatin1Char('_')))
                    return;
            }

            setFormat(position, length, formatForCategory(C_TYPE));
        }
    }
}

bool CppHighlighter::highlightRawStringLiteral(QStringView text, const Token &tk,
                                               const QString &inheritedSuffix)
{
    // Step one: Does the lexer think this is a raw string literal?
    switch (tk.kind()) {
    case T_RAW_STRING_LITERAL:
    case T_RAW_WIDE_STRING_LITERAL:
    case T_RAW_UTF8_STRING_LITERAL:
    case T_RAW_UTF16_STRING_LITERAL:
    case T_RAW_UTF32_STRING_LITERAL:
        break;
    default:
        return false;
    }

    // Step two: Try to find all the components (prefix/string/suffix). We might be in the middle
    //           of a multi-line literal, though, so prefix and/or suffix might be missing.
    int delimiterOffset = -1;
    int stringOffset = 0;
    int stringLength = tk.utf16chars();
    int endDelimiterOffset = -1;
    QString expectedSuffix = inheritedSuffix;
    [&] {
        // If the "inherited" suffix is not empty, then this token is a string continuation and
        // can therefore not start a new raw string literal.
        // FIXME: The lexer starts the token at the first non-whitespace character, so
        //        we have to correct for that here.
        if (!inheritedSuffix.isEmpty()) {
            stringLength += tk.utf16charOffset;
            return;
        }

        // Conversely, since we are in a raw string literal that is not a continuation,
        // the start sequence must be in here.
        const int rOffset = text.indexOf(QLatin1String("R\""), tk.utf16charsBegin());
        QTC_ASSERT(rOffset != -1, return);
        const int tentativeDelimiterOffset = rOffset + 2;
        const int openParenOffset = text.indexOf('(', tentativeDelimiterOffset);
        QTC_ASSERT(openParenOffset != -1, return);
        const QStringView delimiter = text.mid(tentativeDelimiterOffset,
                                               openParenOffset - tentativeDelimiterOffset);
        expectedSuffix = ')' + delimiter + '"';
        delimiterOffset = tentativeDelimiterOffset;
        stringOffset = delimiterOffset + delimiter.length() + 1;
        stringLength -= delimiter.length() + 1;
    }();
    int operatorOffset = tk.utf16charsBegin() + tk.utf16chars();
    int operatorLength = 0;
    if (tk.f.userDefinedLiteral) {
        const int closingQuoteOffset = text.lastIndexOf('"', operatorOffset);
        QTC_ASSERT(closingQuoteOffset >= tk.utf16charsBegin(), return false);
        operatorOffset = closingQuoteOffset + 1;
        operatorLength = tk.utf16charsBegin() + tk.utf16chars() - operatorOffset;
        stringLength -= operatorLength;
    }
    if (text.mid(tk.utf16charsBegin(), operatorOffset - tk.utf16charsBegin())
            .endsWith(expectedSuffix)) {
        endDelimiterOffset = operatorOffset - expectedSuffix.size();
        stringLength -= expectedSuffix.size();
    }

    // Step three: Do the actual formatting. For clarity, we display only the actual content as
    //             a string, and the rest (including the delimiter) as a keyword.
    const QTextCharFormat delimiterFormat = formatForCategory(C_KEYWORD);
    if (delimiterOffset != -1)
        setFormat(tk.utf16charsBegin(), stringOffset - tk.utf16charsBegin(), delimiterFormat);
    setFormatWithSpaces(text.toString(), stringOffset, stringLength, formatForCategory(C_STRING));
    if (endDelimiterOffset != -1)
        setFormat(endDelimiterOffset, expectedSuffix.size(), delimiterFormat);
    if (operatorLength > 0)
        setFormat(operatorOffset, operatorLength, formatForCategory(C_OPERATOR));
    return true;
}

void CppHighlighter::highlightStringLiteral(QStringView text, const CPlusPlus::Token &tk)
{
    switch (tk.kind()) {
    case T_WIDE_STRING_LITERAL:
    case T_UTF8_STRING_LITERAL:
    case T_UTF16_STRING_LITERAL:
    case T_UTF32_STRING_LITERAL:
    case T_WIDE_CHAR_LITERAL:
    case T_UTF16_CHAR_LITERAL:
    case T_UTF32_CHAR_LITERAL:
        break;
    default:
        if (!tk.userDefinedLiteral()) { // Simple case: No prefix, no suffix.
            setFormatWithSpaces(text.toString(), tk.utf16charsBegin(), tk.utf16chars(),
                                formatForCategory(C_STRING));
            return;
        }
    }

    const char quote = tk.isStringLiteral() ? '"' : '\'';
    int stringOffset = 0;
    if (!tk.f.joined) {
        stringOffset = text.indexOf(quote, tk.utf16charsBegin());
        QTC_ASSERT(stringOffset > 0, return);
        setFormat(tk.utf16charsBegin(), stringOffset - tk.utf16charsBegin(),
                  formatForCategory(C_KEYWORD));
    }
    int operatorOffset = tk.utf16charsBegin() + tk.utf16chars();
    if (tk.userDefinedLiteral()) {
        const int closingQuoteOffset = text.lastIndexOf(quote, operatorOffset);
        QTC_ASSERT(closingQuoteOffset >= tk.utf16charsBegin(), return);
        operatorOffset = closingQuoteOffset + 1;
    }
    setFormatWithSpaces(text.toString(), stringOffset, operatorOffset - tk.utf16charsBegin(),
                        formatForCategory(C_STRING));
    if (const int operatorLength = tk.utf16charsBegin() + tk.utf16chars() - operatorOffset;
        operatorLength > 0) {
        setFormat(
            operatorOffset,
            operatorLength,
            formatForCategory(tk.userDefinedLiteral() ? C_OVERLOADED_OPERATOR : C_OPERATOR));
    }
}

void CppHighlighter::highlightDoxygenComment(const QString &text, int position, int)
{
    int initial = position;

    const QChar *uc = text.unicode();
    const QChar *it = uc + position;

    const QTextCharFormat &format = formatForCategory(C_DOXYGEN_COMMENT);
    const QTextCharFormat &kwFormat = formatForCategory(C_DOXYGEN_TAG);

    while (!it->isNull()) {
        if (it->unicode() == QLatin1Char('\\') ||
            it->unicode() == QLatin1Char('@')) {
            ++it;

            const QChar *start = it;
            while (isValidAsciiIdentifierChar(*it))
                ++it;

            int k = classifyDoxygenTag(start, it - start);
            if (k != T_DOXY_IDENTIFIER) {
                setFormatWithSpaces(text, initial, start - uc - initial, format);
                setFormat(start - uc - 1, it - start + 1, kwFormat);
                initial = it - uc;
            }
        } else
            ++it;
    }

    setFormatWithSpaces(text, initial, it - uc - initial, format);
}

namespace Internal {
#ifdef WITH_TESTS
using namespace CppEditor::Tests;
using namespace Tests;
class CppHighlighterTest : public CppHighlighter
{
    Q_OBJECT

public:
    CppHighlighterTest()
    {
        QFile source(":/cppeditor/testcases/highlightingtestcase.cpp");
        QVERIFY(source.open(QIODevice::ReadOnly));

        m_doc.setPlainText(QString::fromUtf8(source.readAll()));
        setDocument(&m_doc);
        rehighlight();
    }

private slots:
    void test_data()
    {
        QTest::addColumn<int>("line");
        QTest::addColumn<int>("column");
        QTest::addColumn<int>("lastLine");
        QTest::addColumn<int>("lastColumn");
        QTest::addColumn<TextStyle>("style");

        QTest::newRow("auto return type") << 1 << 1 << 1 << 4 << C_KEYWORD;
        QTest::newRow("opening brace") << 2 << 1 << 2 << 1 << C_PUNCTUATION;
        QTest::newRow("return") << 3 << 5 << 3 << 10 << C_KEYWORD;
        QTest::newRow("raw string prefix") << 3 << 12 << 3 << 14 << C_KEYWORD;
        QTest::newRow("raw string content (multi-line)") << 3 << 15 << 6 << 13 << C_STRING;
        QTest::newRow("raw string suffix") << 6 << 14 << 6 << 15 << C_KEYWORD;
        QTest::newRow("raw string prefix 2") << 6 << 17 << 6 << 19 << C_KEYWORD;
        QTest::newRow("raw string content 2") << 6 << 20 << 6 << 25 << C_STRING;
        QTest::newRow("raw string suffix 2") << 6 << 26 << 6 << 27 << C_KEYWORD;
        QTest::newRow("comment") << 6 << 29 << 6 << 41 << C_COMMENT;
        QTest::newRow("raw string prefix 3") << 6 << 53 << 6 << 45 << C_KEYWORD;
        QTest::newRow("raw string content 3") << 6 << 46 << 6 << 50 << C_STRING;
        QTest::newRow("raw string suffix 3") << 6 << 51 << 6 << 52 << C_KEYWORD;
        QTest::newRow("semicolon") << 6 << 53 << 6 << 53 << C_PUNCTUATION;
        QTest::newRow("closing brace") << 7 << 1 << 7 << 1 << C_PUNCTUATION;
        QTest::newRow("void") << 9 << 1 << 9 << 4 << C_PRIMITIVE_TYPE;
        QTest::newRow("bool") << 11 << 5 << 11 << 8 << C_PRIMITIVE_TYPE;
        QTest::newRow("true") << 11 << 15 << 11 << 18 << C_KEYWORD;
        QTest::newRow("false") << 12 << 15 << 12 << 19 << C_KEYWORD;
        QTest::newRow("nullptr") << 13 << 15 << 13 << 21 << C_KEYWORD;
        QTest::newRow("auto var type") << 18 << 15 << 18 << 8 << C_KEYWORD;
        QTest::newRow("integer literal") << 18 << 28 << 18 << 28 << C_NUMBER;
        QTest::newRow("floating-point literal 1") << 19 << 28 << 19 << 31 << C_NUMBER;
        QTest::newRow("floating-point literal 2") << 20 << 28 << 20 << 30 << C_NUMBER;
        QTest::newRow("template keyword") << 23 << 1 << 23 << 8 << C_KEYWORD;
        QTest::newRow("type in template type parameter") << 23 << 10 << 23 << 12 << C_PRIMITIVE_TYPE;
        QTest::newRow("integer literal as non-type template parameter default value")
            << 23 << 18 << 23 << 18 << C_NUMBER;
        QTest::newRow("class keyword") << 23 << 21 << 23 << 25 << C_KEYWORD;
        QTest::newRow("struct keyword") << 25 << 1 << 25 << 6 << C_KEYWORD;
        QTest::newRow("operator keyword") << 26 << 5 << 26 << 12 << C_KEYWORD;
        QTest::newRow("type in conversion operator") << 26 << 14 << 26 << 16 << C_PRIMITIVE_TYPE;
        QTest::newRow("concept keyword") << 29 << 22 << 29 << 28 << C_KEYWORD;
        QTest::newRow("user-defined UTF-16 string literal (prefix)")
            << 32 << 16 << 32 << 16 << C_KEYWORD;
        QTest::newRow("user-defined UTF-16 string literal (content)")
            << 32 << 17 << 32 << 21 << C_STRING;
        QTest::newRow("user-defined UTF-16 string literal (suffix)")
            << 32 << 22 << 32 << 23 << C_OPERATOR;
        QTest::newRow("wide string literal (prefix)") << 33 << 17 << 33 << 17 << C_KEYWORD;
        QTest::newRow("wide string literal (content)") << 33 << 18 << 33 << 24 << C_STRING;
        QTest::newRow("UTF-8 string literal (prefix)") << 34 << 17 << 34 << 18 << C_KEYWORD;
        QTest::newRow("UTF-8 string literal (content)") << 34 << 19 << 34 << 24 << C_STRING;
        QTest::newRow("UTF-32 string literal (prefix)") << 35 << 17 << 35 << 17 << C_KEYWORD;
        QTest::newRow("UTF-8 string literal (content)") << 35 << 18 << 35 << 23 << C_STRING;
        QTest::newRow("user-defined UTF-16 raw string literal (prefix)")
            << 36 << 17 << 36 << 20 << C_KEYWORD;
        QTest::newRow("user-defined UTF-16 raw string literal (content)")
            << 36 << 38 << 37 << 8 << C_STRING;
        QTest::newRow("user-defined UTF-16 raw string literal (suffix 1)")
            << 37 << 9 << 37 << 10 << C_KEYWORD;
        QTest::newRow("user-defined UTF-16 raw string literal (suffix 2)")
            << 37 << 11 << 37 << 12 << C_OPERATOR;
        QTest::newRow("multi-line user-defined UTF-16 string literal (prefix)")
            << 38 << 17 << 38 << 17 << C_KEYWORD;
        QTest::newRow("multi-line user-defined UTF-16 string literal (content)")
            << 38 << 18 << 39 << 3 << C_STRING;
        QTest::newRow("multi-line user-defined UTF-16 string literal (suffix)")
            << 39 << 4 << 39 << 5 << C_OPERATOR;
        QTest::newRow("multi-line raw string literal with consecutive closing parens (prefix)")
            << 48 << 18 << 48 << 20 << C_KEYWORD;
        QTest::newRow("multi-line raw string literal with consecutive closing parens (content)")
            << 49 << 1 << 49 << 1 << C_STRING;
        QTest::newRow("multi-line raw string literal with consecutive closing parens (suffix)")
            << 49 << 2 << 49 << 3 << C_KEYWORD;
        QTest::newRow("wide char literal with user-defined suffix (prefix)")
            << 73 << 16 << 73 << 16 << C_KEYWORD;
        QTest::newRow("wide char literal with user-defined suffix (content)")
            << 73 << 17 << 73 << 18 << C_STRING;
        QTest::newRow("wide char literal with user-defined suffix (suffix)")
            << 73 << 20 << 73 << 22 << C_OVERLOADED_OPERATOR;
    }

    void test()
    {
        QFETCH(int, line);
        QFETCH(int, column);
        QFETCH(int, lastLine);
        QFETCH(int, lastColumn);
        QFETCH(TextStyle, style);

        const int startPos = Utils::Text::positionInText(&m_doc, line, column);
        const int lastPos = Utils::Text::positionInText(&m_doc, lastLine, lastColumn);
        const auto getActualFormat = [&](int pos) -> QTextCharFormat {
            const QTextBlock block = m_doc.findBlock(pos);
            if (!block.isValid())
                return {};
            const QList<QTextLayout::FormatRange> &ranges = block.layout()->formats();
            for (const QTextLayout::FormatRange &range : ranges) {
                const int offset = block.position() + range.start;
                if (offset > pos)
                    return {};
                if (offset + range.length <= pos)
                    continue;
                return range.format;
            }
            return {};
        };

        const QTextCharFormat formatForStyle = formatForCategory(style);
        for (int pos = startPos; pos <= lastPos; ++pos) {
            const QChar c = m_doc.characterAt(pos);
            if (c == QChar::ParagraphSeparator)
                continue;
            const QTextCharFormat expectedFormat = asSyntaxHighlight(
                c.isSpace() ? whitespacified(formatForStyle) : formatForStyle);

            const QTextCharFormat actualFormat = getActualFormat(pos);
            if (actualFormat != expectedFormat) {
                int posLine;
                int posCol;
                Utils::Text::convertPosition(&m_doc, pos, &posLine, &posCol);
                qDebug() << posLine << posCol << c
                         << actualFormat.foreground() << expectedFormat.foreground()
                         << actualFormat.background() << expectedFormat.background();
            }
            QCOMPARE(actualFormat, expectedFormat);
        }
    }

    void testParentheses_data()
    {
        QTest::addColumn<int>("line");
        QTest::addColumn<int>("expectedParenCount");

        QTest::newRow("function head") << 41 << 2;
        QTest::newRow("function opening brace") << 42 << 1;
        QTest::newRow("loop head") << 43 << 1;
        QTest::newRow("comment") << 44 << 0;
        QTest::newRow("loop end") << 45 << 3;
        QTest::newRow("function closing brace") << 46 << 1;
    }

    void testParentheses()
    {
        QFETCH(int, line);
        QFETCH(int, expectedParenCount);

        QTextBlock block = m_doc.findBlockByNumber(line - 1);
        QVERIFY(block.isValid());
        QCOMPARE(TextDocumentLayout::parentheses(block).count(), expectedParenCount);
    }

    void testFoldingIndent_data()
    {
        QTest::addColumn<int>("line");
        QTest::addColumn<int>("expectedFoldingIndent");
        QTest::addColumn<int>("expectedFoldingIndentNextLine");

        QTest::newRow("braces after one line comment") << 52 << 0 << 1;
        QTest::newRow("braces after multiline comment") << 59 << 0 << 1;
        QTest::newRow("braces after completed line") << 67 << 1 << 2;
    }

    void testFoldingIndent()
    {
        QFETCH(int, line);
        QFETCH(int, expectedFoldingIndent);
        QFETCH(int, expectedFoldingIndentNextLine);

        QTextBlock block = m_doc.findBlockByNumber(line - 1);
        QVERIFY(block.isValid());
        QCOMPARE(TextDocumentLayout::foldingIndent(block), expectedFoldingIndent);

        QTextBlock nextBlock = m_doc.findBlockByNumber(line);
        QVERIFY(nextBlock.isValid());
        QCOMPARE(TextDocumentLayout::foldingIndent(nextBlock), expectedFoldingIndentNextLine);
    }

private:
    QTextDocument m_doc;
};

class CodeFoldingTest : public QObject
{
    Q_OBJECT

private slots:
    void test()
    {
        const QByteArray content = R"(cpp // 0,0
int main() {                              // 1,0
#if 0                                     // 1,1
    if (true) {                           // 1,1
        //...                             // 1,1
    }                                     // 1,1
    else {                                // 1,1
        //...                             // 1,1
    }                                     // 1,1
#else                                     // 1,1
    if (true) {                           // 2,1
        //...                             // 2,2
    }                                     // 1,1
#endif                                    // 1,1
}                                         // 0,0
                                          // 0,0
cpp)";
        TemporaryDir temporaryDir;
        QVERIFY(temporaryDir.isValid());
        CppTestDocument testDocument("file.cpp", content);
        testDocument.setBaseDirectory(temporaryDir.path());
        QVERIFY(testDocument.writeToDisk());

        QVERIFY(TestCase::openCppEditor(testDocument.filePath(), &testDocument.m_editor,
                                        &testDocument.m_editorWidget));

        QEventLoop loop;
        QTimer t;
        t.setSingleShot(true);
        connect(&t, &QTimer::timeout, &loop, [&] {loop.exit(1); });
        const auto check = [&] {
            const struct LoopHandler {
                LoopHandler(QEventLoop &loop) : loop(loop) {}
                ~LoopHandler() { loop.quit(); }

            private:
                QEventLoop &loop;
            } loopHandler(loop);

            const auto getExpectedBraceDepthAndFoldingIndent = [](const QTextBlock &block) {
                const QString &text = block.text();
                if (text.size() < 3)
                    return std::make_pair(-1, -1);
                bool ok;
                const int braceDepth = text.mid(text.size() - 3, 1).toInt(&ok);
                if (!ok)
                    return std::make_pair(-1, -1);
                const int foldingIndent = text.last(1).toInt(&ok);
                if (!ok)
                    return std::make_pair(-1, -1);
                return std::make_pair(braceDepth, foldingIndent);
            };
            const auto getActualBraceDepthAndFoldingIndent = [](const QTextBlock &block) {
                const int braceDepth = block.userState() >> 8;
                const int foldingIndent = TextDocumentLayout::foldingIndent(block);
                return std::make_pair(braceDepth, foldingIndent);
            };
            TextDocument * const doc = testDocument.m_editorWidget->textDocument();
            const QTextBlock lastBlock = doc->document()->lastBlock();
            for (QTextBlock b = doc->document()->firstBlock(); b.isValid() && b != lastBlock;
                 b = b.next()) {
                const auto actual = getActualBraceDepthAndFoldingIndent(b);
                const auto expected = getExpectedBraceDepthAndFoldingIndent(b);
                if (actual != expected)
                    qDebug() << "In line" << (b.blockNumber() + 1);
                QCOMPARE(actual, expected);
            }
        };
        connect(testDocument.m_editorWidget, &CppEditorWidget::ifdefedOutBlocksChanged,
                this, check);
        t.start(5000);
        QCOMPARE(loop.exec(), 0);
    }

    void cleanup()
    {
        QVERIFY(Core::EditorManager::closeAllEditors(false));
        QVERIFY(TestCase::garbageCollectGlobalSnapshot());
    }
};

#endif // WITH_TESTS

void registerHighlighterTests(ExtensionSystem::IPlugin &plugin)
{
#ifdef WITH_TESTS
    plugin.addTest<CppHighlighterTest>();
    plugin.addTest<CodeFoldingTest>();
#else
    Q_UNUSED(plugin)
#endif
}

} // namespace Internal
} // namespace CppEditor

#ifdef WITH_TESTS
#include <cpphighlighter.moc>
#endif
