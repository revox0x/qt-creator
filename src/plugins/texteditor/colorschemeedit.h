/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include "colorscheme.h"
#include "fontsettingspage.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListView;
class QModelIndex;
class QScrollArea;
class QToolButton;
QT_END_NAMESPACE

namespace TextEditor::Internal {

class FormatsModel;

/*!
  A widget for editing a color scheme. Used in the FontSettingsPage.
  */
class ColorSchemeEdit : public QWidget
{
    Q_OBJECT

public:
    ColorSchemeEdit(QWidget *parent = nullptr);
    ~ColorSchemeEdit() override;

    void setFormatDescriptions(const FormatDescriptions &descriptions);
    void setBaseFont(const QFont &font);
    void setReadOnly(bool readOnly);

    void setColorScheme(const ColorScheme &colorScheme);
    const ColorScheme &colorScheme() const;

signals:
    void copyScheme();

private:
    void currentItemChanged(const QModelIndex &index);
    void changeForeColor();
    void changeBackColor();
    void eraseForeColor();
    void eraseBackColor();
    void changeRelativeForeColor();
    void changeRelativeBackColor();
    void eraseRelativeForeColor();
    void eraseRelativeBackColor();
    void checkCheckBoxes();
    void changeUnderlineColor();
    void eraseUnderlineColor();
    void changeUnderlineStyle(int index);

    void updateControls();
    void updateForegroundControls();
    void updateBackgroundControls();
    void updateRelativeForegroundControls();
    void updateRelativeBackgroundControls();
    void updateFontControls();
    void updateUnderlineControls();
    void setItemListBackground(const QColor &color);
    void populateUnderlineStyleComboBox();

private:
    FormatDescriptions m_descriptions;
    ColorScheme m_scheme;
    int m_curItem = -1;
    FormatsModel *m_formatsModel;
    bool m_readOnly = false;
    QListView *m_itemList;
    QLabel *m_builtinSchemeLabel;
    QWidget *m_fontProperties;
    QLabel *m_foregroundLabel;
    QToolButton *m_foregroundToolButton;
    QToolButton *m_eraseForegroundToolButton;
    QLabel *m_backgroundLabel;
    QToolButton *m_backgroundToolButton;
    QToolButton *m_eraseBackgroundToolButton;
    QLabel *m_relativeForegroundHeadline;
    QLabel *m_foregroundLightnessLabel;
    QDoubleSpinBox *m_foregroundLightnessSpinBox;
    QLabel *m_foregroundSaturationLabel;
    QDoubleSpinBox *m_foregroundSaturationSpinBox;
    QLabel *m_relativeBackgroundHeadline;
    QLabel *m_backgroundSaturationLabel;
    QDoubleSpinBox *m_backgroundSaturationSpinBox;
    QLabel *m_backgroundLightnessLabel;
    QDoubleSpinBox *m_backgroundLightnessSpinBox;
    QLabel *m_fontHeadline;
    QCheckBox *m_boldCheckBox;
    QCheckBox *m_italicCheckBox;
    QLabel *m_underlineHeadline;
    QLabel *m_underlineLabel;
    QToolButton *m_underlineColorToolButton;
    QToolButton *m_eraseUnderlineColorToolButton;
    QComboBox *m_underlineComboBox;

};

} // TextEditor::Internal
