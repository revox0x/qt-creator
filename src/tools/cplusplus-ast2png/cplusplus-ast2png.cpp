// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include <cplusplus/AST.h>
#include <cplusplus/ASTMatcher.h>
#include <cplusplus/ASTPatternBuilder.h>
#include <cplusplus/ASTVisitor.h>
#include <cplusplus/Control.h>
#include <cplusplus/CoreTypes.h>
#include <cplusplus/CppDocument.h>
#include <cplusplus/Literals.h>
#include <cplusplus/Names.h>
#include <cplusplus/Overview.h>
#include <cplusplus/Scope.h>
#include <cplusplus/SymbolVisitor.h>
#include <cplusplus/Symbols.h>
#include <cplusplus/TranslationUnit.h>

#include <utils/hostosinfo.h>

#include "utils.h"

#include <QDir>
#include <QFile>
#include <QList>
#include <QCoreApplication>
#include <QStringList>
#include <QFileInfo>
#include <QTime>
#include <QDebug>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#ifdef __GNUC__
#  include <cxxabi.h>
#endif

// For isatty(), _isatty()
#if defined(Q_OS_WIN)
#  include <io.h>
#else
#  include <unistd.h>
#endif

bool tty_for_stdin()
{
#if defined(Q_OS_WIN)
    return _isatty(_fileno(stdin));
#else
    return isatty(fileno(stdin));
#endif
}

using namespace CPlusPlus;

class ASTDump: protected ASTVisitor
{
public:
    ASTDump(TranslationUnit *unit)
        : ASTVisitor(unit) {}

    void operator()(AST *ast) {
        QByteArray basename = translationUnit()->fileName();
        basename.append(".ast.dot");
        out.open(basename.constData());

        out << "digraph AST { ordering=out;" << std::endl;
        // std::cout << "rankdir = \"LR\";" << std::endl;

        generateTokens();
        accept(ast);

        typedef QPair<QByteArray, QByteArray> Pair;

        for (const Pair &conn : qAsConst(_connections))
            out << conn.first.constData() << " -> " << conn.second.constData() << std::endl;

        alignTerminals();

        out << "}" << std::endl;
        out.close();
    }

    // the following file can be generated by using:
    //    cplusplus-update-frontend <frontend-dir> <dumpers-file>
#include "dumpers.inc"

protected:
    void alignTerminals() {
        out<<"{ rank=same;" << std::endl;
        for (const QByteArray &terminalShape : qAsConst(_terminalShapes)) {
            out << "  " << std::string(terminalShape.constData(), terminalShape.size()).c_str() << ";" << std::endl;
        }
        out<<"}"<<std::endl;
    }

    static QByteArray name(AST *ast) {
#ifdef __GNUC__
        QByteArray name = abi::__cxa_demangle(typeid(*ast).name(), 0, 0, 0) + 11;
        name.truncate(name.length() - 3);
#else
        QByteArray name = typeid(*ast).name();
#endif
        return name;
    }

    QByteArray terminalId(int token)
    { return 't' + QByteArray::number(token); }

    void terminal(int token, AST *node) {
        _connections.append(qMakePair(_id[node], terminalId(token)));
    }

    void generateTokens() {
        for (int token = 1; token < translationUnit()->tokenCount(); ++token) {
            if (translationUnit()->tokenKind(token) == T_EOF_SYMBOL)
                break;

            QByteArray t;

            t.append(terminalId(token));
            t.append(" [shape=rect label = \"");
            t.append(spell(token));
            t.append("\"]");

            if (token > 1) {
                t.append("; ");
                t.append(terminalId(token - 1));
                t.append(" -> ");
                t.append(terminalId(token));
                t.append(" [arrowhead=\"vee\" color=\"transparent\"]");
            }

            _terminalShapes.append(t);
        }
    }

    virtual void nonterminal(AST *ast) {
        accept(ast);
    }

    virtual void node(AST *ast) {
        out << _id[ast].constData() << " [label=\"" << name(ast).constData() << "\"];" << std::endl;
    }

    virtual bool preVisit(AST *ast) {
        static int count = 1;
        const QByteArray id = 'n' + QByteArray::number(count++);
        _id[ast] = id;


        if (! _stack.isEmpty())
            _connections.append(qMakePair(_id[_stack.last()], id));

        _stack.append(ast);

        node(ast);

        return true;
    }

    virtual void postVisit(AST *) {
        _stack.removeLast();
    }

private:
    QHash<AST *, QByteArray> _id;
    QList<QPair<QByteArray, QByteArray> > _connections;
    QList<AST *> _stack;
    QList<QByteArray> _terminalShapes;
    std::ofstream out;
};

class SymbolDump: protected SymbolVisitor
{
public:
    SymbolDump(TranslationUnit *unit)
        : translationUnit(unit)
    {
        o.showArgumentNames = true;
        o.showFunctionSignatures = true;
        o.showReturnTypes = true;
    }

    void operator()(Symbol *s) {
        QByteArray basename = translationUnit->fileName();
        basename.append(".symbols.dot");
        out.open(basename.constData());

        out << "digraph Symbols { ordering=out;" << std::endl;
        // std::cout << "rankdir = \"LR\";" << std::endl;
        accept(s);

        for (int i = 0; i < _connections.size(); ++i) {
            QPair<Symbol*,Symbol*> connection = _connections.at(i);
            QByteArray from = _id.value(connection.first);
            if (from.isEmpty())
                from = name(connection.first);
            QByteArray to = _id.value(connection.second);
            if (to.isEmpty())
                to = name(connection.second);
            out << from.constData() << " -> " << to.constData() << ";" << std::endl;
        }

        out << "}" << std::endl;
        out.close();
    }

protected:
    QByteArray name(Symbol *s) {
#ifdef __GNUC__
        QByteArray result = abi::__cxa_demangle(typeid(*s).name(), 0, 0, 0) + 11;
#else
        QByteArray result = typeid(*s).name();
#endif
        if (s->identifier()) {
            result.append("\\nid: ");
            result.append(s->identifier()->chars());
        }
        if (s->isDeprecated())
            result.append("\\n(deprecated)");

        return result;
    }

    virtual bool preVisit(Symbol *s) {
        static int count = 0;
        QByteArray nodeId("s");
        nodeId.append(QByteArray::number(++count));
        _id[s] = nodeId;

        if (!_stack.isEmpty())
            _connections.append(qMakePair(_stack.last(), s));

        _stack.append(s);

        return true;
    }

    virtual void postVisit(Symbol *) {
        _stack.removeLast();
    }

    virtual void simpleNode(Symbol *symbol) {
        out << _id[symbol].constData() << " [label=\"" << name(symbol).constData() << "\"];" << std::endl;
    }

    virtual bool visit(Class *symbol) {
        const char *id = _id.value(symbol).constData();
        out << id << " [label=\"";
        if (symbol->isClass()) {
            out << "class";
        } else if (symbol->isStruct()) {
            out << "struct";
        } else if (symbol->isUnion()) {
            out << "union";
        } else {
            out << "UNKNOWN";
        }

        out << "\\nid: ";
        if (symbol->identifier()) {
            out << symbol->identifier()->chars();
        } else {
            out << "NO ID";
        }
        if (symbol->isDeprecated())
            out << "\\n(deprecated)";
        out << "\"];" << std::endl;

        return true;
    }

    virtual bool visit(UsingNamespaceDirective *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(UsingDeclaration *symbol) { simpleNode(symbol); return true; }

    virtual bool visit(Declaration *symbol) {
        out << _id[symbol].constData() << " [label=\"";
        out << "Declaration\\n";
        out << qPrintable(o(symbol->name()));
        out << ": ";
        out << qPrintable(o(symbol->type()));
        if (symbol->isDeprecated())
            out << "\\n(deprecated)";
        if (Function *funTy = symbol->type()->asFunctionType()) {
            if (funTy->isPureVirtual())
                out << "\\n(pure virtual)";
            else if (funTy->isVirtual())
                out << "\\n(virtual)";

            if (funTy->isSignal())
                out << "\\n(signal)";
            if (funTy->isSlot())
                out << "\\n(slot)";
            if (funTy->isInvokable())
                out << "\\n(invokable)";
        }
        out << "\"];" << std::endl;

        return true;
    }

    virtual bool visit(Argument *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(TypenameArgument *symbol) { simpleNode(symbol); return true; }

    virtual bool visit(BaseClass *symbol) {
        out << _id[symbol].constData() << " [label=\"BaseClass\\n";
        out << qPrintable(o(symbol->name()));
        if (symbol->isDeprecated())
            out << "\\n(deprecated)";
        out << "\"];" << std::endl;

        return true;
    }

    virtual bool visit(Enum *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(Function *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(Namespace *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(Block *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(ForwardClassDeclaration *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(ObjCBaseClass *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(ObjCBaseProtocol *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(ObjCClass *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(ObjCForwardClassDeclaration *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(ObjCProtocol *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(ObjCForwardProtocolDeclaration *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(ObjCMethod *symbol) { simpleNode(symbol); return true; }
    virtual bool visit(ObjCPropertyDeclaration *symbol) { simpleNode(symbol); return true; }

private:
    TranslationUnit *translationUnit;
    QHash<Symbol *, QByteArray> _id;
    QList<QPair<Symbol *,Symbol*> >_connections;
    QList<Symbol *> _stack;
    std::ofstream out;
    Overview o;
};

static void createImageFromDot(const QString &inputFile, const QString &outputFile, bool verbose)
{
    const QString command = Utils::HostOsInfo::withExecutableSuffix("dot");
    const QStringList arguments = QStringList({"-Tpng", "-o", outputFile, inputFile});
    CplusplusToolsUtils::executeCommand(command, arguments, QString(), verbose);
}

static const char PATH_STDIN_FILE[] = "_stdincontents.cpp";

static QString example()
{
    return
#if defined(Q_OS_WIN)
    QString::fromLatin1("> echo int foo() {} | %1 && %2.ast.png")
#elif defined(Q_OS_MAC)
    QString::fromLatin1("$ echo \"int foo() {}\" | ./%1 && open %2.ast.png")
#else
    QString::fromLatin1("$ echo \"int foo() {}\" | ./%1 && xdg-open %2.ast.png")
#endif
    .arg(QFileInfo(QCoreApplication::arguments().at(0)).fileName(), QLatin1String(PATH_STDIN_FILE));
}

static QString parseModeToString(Document::ParseMode parseMode)
{
    switch (parseMode) {
    case Document::ParseTranlationUnit:
        return QLatin1String("TranlationUnit");
    case Document::ParseDeclaration:
        return QLatin1String("Declaration");
    case Document::ParseExpression:
        return QLatin1String("Expression");
    case Document::ParseDeclarator:
        return QLatin1String("Declarator");
    case Document::ParseStatement:
        return QLatin1String("Statement");
    default:
        return QLatin1String("UnknownParseMode");
    }
}

/// Counts errors and appends error messages containing the parse mode to an error string
class ErrorHandler: public DiagnosticClient {
public:
    int m_errorCount;
    QByteArray *m_errorString;
    Document::ParseMode m_parseMode;

    ErrorHandler(Document::ParseMode parseMode, QByteArray *errorStringOutput)
        : m_errorCount(0)
        , m_errorString(errorStringOutput)
        , m_parseMode(parseMode) {}

    void report(int level,
                const StringLiteral *fileName,
                int line, int column,
                const char *format, va_list ap)
    {
        ++m_errorCount;

        if (! m_errorString)
            return;

        static const char *const pretty[] = {"warning", "error", "fatal"};

        QString str = QString::asprintf("%s:%d:%d: When parsing as %s: %s: ", fileName->chars(), line, column,
                    parseModeToString(m_parseMode).toUtf8().constData(), pretty[level]);
        m_errorString->append(str.toUtf8());

        str += QString::vasprintf(format, ap);
        m_errorString->append(str.toUtf8());
        m_errorString->append('\n');
    }
};

/// Try to parse with given parseModes. Returns a document pointer if it was possible to
/// successfully parse with one of the given parseModes (one parse mode after the other
/// is tried), otherwise a null pointer.
static Document::Ptr parse(const QString &fileName, const QByteArray &source,
                           const QList<Document::ParseMode> parseModes, QByteArray *errors,
                           bool verbose = false)
{
    for (const Document::ParseMode parseMode : parseModes) {
        ErrorHandler *errorHandler = new ErrorHandler(parseMode, errors); // Deleted by ~Document.
        if (verbose)
            std::cout << "Parsing as " << qPrintable(parseModeToString(parseMode)) << "...";

        Document::Ptr doc = Document::create(fileName);
        doc->control()->setDiagnosticClient(errorHandler);
        doc->setUtf8Source(source);
        const bool parsed = doc->parse(parseMode);
        if (parsed && errorHandler->m_errorCount == 0) {
            if (verbose)
                std::cout << "succeeded." << std::endl;
            return doc;
        }

        if (verbose)
            std::cout << "failed." << std::endl;
    }

    return Document::Ptr();
}

/// Convenience function
static Document::Ptr parse(const QString &fileName, const QByteArray &source,
                           const Document::ParseMode parseMode, QByteArray *errors,
                           bool verbose = false)
{
    QList<Document::ParseMode> parseModes = QList<Document::ParseMode>() << parseMode;
    return parse(fileName, source, parseModes, errors, verbose);
}

static void printUsage()
{
    std::cout << "Usage: " << qPrintable(QFileInfo(QCoreApplication::arguments().at(0)).fileName())
              << " [-v] [-p ast] <file1> <file2> ...\n\n";

    std::cout
        << "Visualize AST and symbol hierarchy of given C++ files by generating png image files\n"
        << "in the same directory as the input files. Print paths to generated image files.\n"
        << "\n"
        << "Options:\n"
        << " -v       Run with increased verbosity.\n"
        << " -p <ast> Parse each file as <ast>. <ast> is one of:\n"
        << "             - 'declarator' or 'dr'\n"
        << "             - 'expression' or 'ex'\n"
        << "             - 'declaration' or 'dn'\n"
        << "             - 'statement' or 'st'\n"
        << "             - 'translationunit' or 'tr'\n"
        << "          If this option is not provided, each file is tried to be parsed as\n"
        << "          declarator, expression, etc. using the stated order.\n"
        << "\n";

    std::cout << QString::fromLatin1(
    "Standard input is also read. The resulting files start with \"%1\"\n"
    "and are created in the current working directory. To show the AST for simple snippets\n"
    "you might want to execute:\n"
    "\n"
    " %2\n"
    "\n"
    "Prerequisites:\n"
    " 1) Make sure to have 'dot' from graphviz locatable by PATH.\n"
    " 2) Make sure to have an up to date dumpers file by using 'cplusplus-update-frontend'.\n"
    ).arg(QLatin1String(PATH_STDIN_FILE), example()).toLocal8Bit().constData();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();
    args.removeFirst();

    bool optionVerbose = false;
    int optionParseMode = -1;

    // Test only for stdin if not input files are specified.
    const bool doTestForStdIn = args.isEmpty()
        || (args.count() == 1 && args.contains(QLatin1String("-v")));
    if (doTestForStdIn && !tty_for_stdin()) {
        QFile file((QLatin1String(PATH_STDIN_FILE)));
        if (! file.open(QFile::WriteOnly)) {
            std::cerr << "Error: Cannot open file for writing\"" << qPrintable(file.fileName())
                      << "\"" << std::endl;
            exit(EXIT_FAILURE);
        }
        file.write(QTextStream(stdin).readAll().toLocal8Bit());
        file.close();
        args.append(file.fileName());
    }

    // Process options & arguments
    const bool helpRequested = args.contains(QLatin1String("-h"))
        || args.contains(QLatin1String("-help"));
    if (helpRequested) {
        printUsage();
        return helpRequested ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (args.contains(QLatin1String("-v"))) {
        optionVerbose = true;
        args.removeOne(QLatin1String("-v"));
    }
    if (args.contains(QLatin1String("-p"))) {
        args.removeOne(QLatin1String("-p"));
        if (args.isEmpty()) {
            std::cerr << "Error: Expected ast after option \"-p\"." << std::endl;
            printUsage();
            exit(EXIT_FAILURE);
        }
        const QString parseAs = args.first();
        if (parseAs == QLatin1String("declarator") || parseAs == QLatin1String("dr")) {
            optionParseMode = Document::ParseDeclarator;
        } else if (parseAs == QLatin1String("expression") || parseAs == QLatin1String("ex")) {
            optionParseMode = Document::ParseExpression;
        } else if (parseAs == QLatin1String("declaration") || parseAs == QLatin1String("dn")) {
            optionParseMode = Document::ParseDeclaration;
        } else if (parseAs == QLatin1String("statement") || parseAs == QLatin1String("st")) {
            optionParseMode = Document::ParseStatement;
        } else if (parseAs == QLatin1String("translationunit") || parseAs == QLatin1String("tr")) {
            optionParseMode = Document::ParseTranlationUnit;
        } else {
            std::cerr << "Error: Invalid ast for option \"-p\"." << std::endl;
            printUsage();
            exit(EXIT_FAILURE);
        }
        args.removeOne(parseAs);
    }

    if (args.isEmpty()) {
        printUsage();
        return EXIT_SUCCESS;
    }

    // Process files
    const QStringList files = args;
    for (const QString &fileName : files) {
        if (! QFile::exists(fileName)) {
            std::cerr << "Error: File \"" << qPrintable(fileName) << "\" does not exist."
                      << std::endl;
            exit(EXIT_FAILURE);
        }

        // Run the preprocessor
        const QString fileNamePreprocessed = fileName + QLatin1String(".preprocessed");
        CplusplusToolsUtils::SystemPreprocessor preprocessor(optionVerbose);
        preprocessor.preprocessFile(fileName, fileNamePreprocessed);

        // Convert to dot
        QFile file(fileNamePreprocessed);
        if (! file.open(QFile::ReadOnly)) {
            std::cerr << "Error: Could not open file \"" << qPrintable(fileNamePreprocessed)
                      << "\"" << std::endl;
            exit(EXIT_FAILURE);
        }

        const QByteArray source = file.readAll();
        file.close();

        // Parse Document
        QByteArray errors;
        Document::Ptr doc;
        if (optionParseMode == -1) {
            QList<Document::ParseMode> parseModes;
            parseModes
                << Document::ParseDeclarator
                << Document::ParseExpression
                << Document::ParseDeclaration
                << Document::ParseStatement
                << Document::ParseTranlationUnit;
            doc = parse(fileName, source, parseModes, &errors, optionVerbose);
        } else {
            doc = parse(fileName, source, static_cast<Document::ParseMode>(optionParseMode),
                &errors, optionVerbose);
        }

        if (!doc) {
            std::cerr << "Error: Could not parse file \"" << qPrintable(fileName) << "\".\n";
            std::cerr << errors.constData();
            exit(EXIT_FAILURE);
        }

        doc->check();

        // Run AST dumper
        ASTDump dump(doc->translationUnit());
        dump(doc->translationUnit()->ast());

        SymbolDump dump2(doc->translationUnit());
        dump2(doc->globalNamespace());

        // Create images
        typedef QPair<QString, QString> Pair;
        QList<Pair> inputOutputFiles;
        inputOutputFiles.append(qMakePair(QString(fileName + QLatin1String(".ast.dot")),
                                          QString(fileName + QLatin1String(".ast.png"))));
        inputOutputFiles.append(qMakePair(QString(fileName + QLatin1String(".symbols.dot")),
                                          QString(fileName + QLatin1String(".symbols.png"))));
        for (const Pair &pair : qAsConst(inputOutputFiles)) {
            createImageFromDot(pair.first, pair.second, optionVerbose);
            std::cout << qPrintable(QDir::toNativeSeparators(pair.second)) << std::endl;
        }
    }

    return EXIT_SUCCESS;
}
