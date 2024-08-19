// Copyright (C) 2020 Alexis Jeandet.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "buildoptions.h"

#include <utils/qtcassert.h>
#include <utils/treemodel.h>

#include <QAbstractTableModel>
#include <QFont>
#include <QItemEditorFactory>
#include <QStyledItemDelegate>

namespace MesonProjectManager::Internal {

class CancellableOption
{
    std::unique_ptr<BuildOption> m_savedValue;
    std::unique_ptr<BuildOption> m_currentValue;
    bool m_changed = false;
    bool m_locked = false;

public:
    CancellableOption(BuildOption *option, bool locked = false)
        : m_savedValue{option->copy()}
        , m_currentValue{option->copy()}
        , m_locked{locked}
    {}
    inline bool isLocked() { return m_locked; }
    inline bool hasChanged() { return m_changed; }
    inline void apply()
    {
        if (m_changed) {
            m_savedValue = std::unique_ptr<BuildOption>(m_currentValue->copy());
            m_changed = false;
        }
    }
    inline void cancel()
    {
        if (m_changed) {
            m_currentValue = std::unique_ptr<BuildOption>(m_savedValue->copy());
            m_changed = false;
        }
    }

    inline const QString &name() const { return m_currentValue->name; }
    inline const QString &section() const { return m_currentValue->section; }
    inline const QString &description() const { return m_currentValue->description; }
    inline const std::optional<QString> &subproject() const
    {
        return m_currentValue->subproject;
    };
    inline QVariant value() { return m_currentValue->value(); }
    inline QString valueStr() { return m_currentValue->valueStr(); };
    inline QString savedValueStr() { return m_savedValue->valueStr(); };
    inline QString mesonArg() { return m_currentValue->mesonArg(); }
    inline void setValue(const QVariant &v)
    {
        if (!m_locked) {
            m_currentValue->setValue(v);
            m_changed = m_currentValue->valueStr() != m_savedValue->valueStr();
        }
    }
    inline BuildOption::Type type() { return m_currentValue->type(); }
};

using CancellableOptionsList = std::vector<std::unique_ptr<CancellableOption>>;
class BuidOptionsModel final : public Utils::TreeModel<>
{
    Q_OBJECT
public:
    explicit BuidOptionsModel(QObject *parent = nullptr);

    void setConfiguration(const BuildOptionsList &options);
    bool setData(const QModelIndex &idx, const QVariant &data, int role) override;

    QStringList changesAsMesonArgs();

    Q_SIGNAL void configurationChanged();

private:
    bool hasChanges() const;
    CancellableOptionsList m_options;
};

class BuildOptionTreeItem final : public Utils::TreeItem
{
    CancellableOption *m_option;

public:
    BuildOptionTreeItem(CancellableOption *option) { m_option = option; }
    QVariant data(int column, int role) const final
    {
        QTC_ASSERT(column >= 0 && column < 2, return {});
        QTC_ASSERT(m_option, return {});
        if (column == 0) {
            switch (role) {
            case Qt::DisplayRole:
                return m_option->name();
            case Qt::ToolTipRole:
                return toolTip();
            case Qt::FontRole:
                QFont font;
                font.setBold(m_option->hasChanged());
                return font;
            }
        }
        if (column == 1) {
            switch (role) {
            case Qt::DisplayRole:
                return m_option->valueStr();
            case Qt::EditRole:
                return m_option->value();
            case Qt::UserRole:
                return m_option->isLocked();
            case Qt::ToolTipRole:
                if (m_option->hasChanged())
                    return QString("%1<br>Initial value was <b>%2</b>")
                        .arg(toolTip())
                        .arg(m_option->savedValueStr());
                else
                    return toolTip();
            case Qt::FontRole:
                QFont font;
                font.setBold(m_option->hasChanged());
                return font;
            }
        }
        return {};
    };
    bool setData(int column, const QVariant &data, int role) final
    {
        Q_UNUSED(role);
        QTC_ASSERT(column == 1, return false);
        m_option->setValue(data);
        return true;
    };
    Qt::ItemFlags flags(int column) const final
    {
        QTC_ASSERT(column >= 0 && column < 2, return Qt::NoItemFlags);
        if (column == 0)
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }
    BuildOption::Type type() const { return m_option->type(); }
    QString toolTip() const { return m_option->description(); }
};

class BuildOptionDelegate final : public QStyledItemDelegate
{
    Q_OBJECT
    static QWidget *makeWidget(QWidget *parent, const QVariant &data);

public:
    BuildOptionDelegate(QObject *parent = nullptr);
    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setModelData(QWidget *editor,
                      QAbstractItemModel *model,
                      const QModelIndex &index) const override;
};

} // namespace MesonProjectManager::Internal
