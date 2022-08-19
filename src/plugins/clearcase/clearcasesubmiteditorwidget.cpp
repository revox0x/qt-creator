// Copyright (C) 2016 AudioCodes Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "clearcasesubmiteditorwidget.h"

#include "activityselector.h"

#include <QCheckBox>
#include <QFrame>
#include <QVBoxLayout>

using namespace ClearCase::Internal;

ClearCaseSubmitEditorWidget::ClearCaseSubmitEditorWidget()
{
    setDescriptionMandatory(false);
    auto checkInWidget = new QWidget(this);

    m_verticalLayout = new QVBoxLayout(checkInWidget);

    m_chkIdentical = new QCheckBox(tr("Chec&k in even if identical to previous version"));
    m_verticalLayout->addWidget(m_chkIdentical);

    m_chkPTime = new QCheckBox(tr("&Preserve file modification time"));
    m_verticalLayout->addWidget(m_chkPTime);

    insertTopWidget(checkInWidget);
}

QString ClearCaseSubmitEditorWidget::activity() const
{
    return m_actSelector ? m_actSelector->activity() : QString();
}

bool ClearCaseSubmitEditorWidget::isIdentical() const
{
    return m_chkIdentical->isChecked();
}

bool ClearCaseSubmitEditorWidget::isPreserve() const
{
    return m_chkPTime->isChecked();
}

void ClearCaseSubmitEditorWidget::setActivity(const QString &act)
{
    if (m_actSelector)
        m_actSelector->setActivity(act);
}

bool ClearCaseSubmitEditorWidget::activityChanged() const
{
    return m_actSelector ? m_actSelector->changed() : false;
}

void ClearCaseSubmitEditorWidget::addKeep()
{
    if (m_actSelector)
        m_actSelector->addKeep();
}

//! Add the ActivitySelector if \a isUcm is set
void ClearCaseSubmitEditorWidget::addActivitySelector(bool isUcm)
{
    if (!isUcm || m_actSelector)
        return;

    m_actSelector = new ActivitySelector;
    m_verticalLayout->insertWidget(0, m_actSelector);

    auto line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    m_verticalLayout->insertWidget(1, line);
}

QString ClearCaseSubmitEditorWidget::commitName() const
{
    return tr("&Check In");
}
