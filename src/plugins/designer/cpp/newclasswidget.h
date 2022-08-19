// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/filepath.h>

#include <QWidget>

namespace Designer {
namespace Internal {

struct NewClassWidgetPrivate;

class NewClassWidget : public QWidget
{
    Q_OBJECT
public:
    enum ClassType { NoClassType,
                     ClassInheritsQObject,
                     ClassInheritsQWidget,
                     ClassInheritsQDeclarativeItem,
                     ClassInheritsQQuickItem,
                     SharedDataClass
                   };
    Q_ENUM(ClassType)
    explicit NewClassWidget(QWidget *parent = nullptr);
    ~NewClassWidget() override;

    QString className() const;
    QString baseClassName() const;
    QString sourceFileName() const;
    QString headerFileName() const;
    QString formFileName() const;
    Utils::FilePath filePath() const;
    QString sourceExtension() const;
    QString headerExtension() const;
    QString formExtension() const;

    bool isValid(QString *error = nullptr) const;

    Utils::FilePaths files() const;

signals:
    void validChanged();
    void activated();

public slots:

    /**
     * The name passed into the new class widget will be reformatted to be a
     * valid class name.
     */
    void setClassName(const QString &suggestedName);
    void setFilePath(const Utils::FilePath &filePath);
    void setSourceExtension(const QString &e);
    void setHeaderExtension(const QString &e);
    void setLowerCaseFiles(bool v);
    void setClassType(ClassType ct);
    void setNamesDelimiter(const QString &delimiter);

    /**
     * Suggest a class name from the base class by stripping the leading 'Q'
     * character. This will happen automagically if the base class combo
     * changes until the class line edited is manually edited.
     */
    void suggestClassNameFromBase();

private:
    void slotUpdateFileNames(const QString &t);
    void slotValidChanged();
    void slotActivated();
    void classNameEdited();

    QString fixSuffix(const QString &suffix);
    NewClassWidgetPrivate *d;
};

} // namespace Internal
} // namespace Designer
