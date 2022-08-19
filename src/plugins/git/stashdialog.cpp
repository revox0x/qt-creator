// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "stashdialog.h"

#include "gitclient.h"
#include "gitplugin.h"
#include "gitutils.h"

#include <utils/algorithm.h>
#include <utils/fancylineedit.h>
#include <utils/itemviews.h>
#include <utils/layoutbuilder.h>
#include <utils/qtcassert.h>

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

using namespace Utils;

enum { NameColumn, BranchColumn, MessageColumn, ColumnCount };

namespace Git {
namespace Internal {

static QList<QStandardItem*> stashModelRowItems(const Stash &s)
{
    Qt::ItemFlags itemFlags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    auto nameItem = new QStandardItem(s.name);
    nameItem->setFlags(itemFlags);
    auto branchItem = new QStandardItem(s.branch);
    branchItem->setFlags(itemFlags);
    auto messageItem = new QStandardItem(s.message);
    messageItem->setFlags(itemFlags);
    QList<QStandardItem*> rc;
    rc << nameItem << branchItem << messageItem;
    return rc;
}

// -----------  StashModel
class StashModel : public QStandardItemModel
{
public:
    explicit StashModel(QObject *parent = nullptr);

    void setStashes(const QList<Stash> &stashes);
    const Stash &at(int i) { return m_stashes.at(i); }

private:
    QList<Stash> m_stashes;
};

StashModel::StashModel(QObject *parent) :
    QStandardItemModel(0, ColumnCount, parent)
{
    QStringList headers;
    headers << StashDialog::tr("Name") << StashDialog::tr("Branch") << StashDialog::tr("Message");
    setHorizontalHeaderLabels(headers);
}

void StashModel::setStashes(const QList<Stash> &stashes)
{
    m_stashes = stashes;
    if (const int rows = rowCount())
        removeRows(0, rows);
    for (const Stash &s : stashes)
        appendRow(stashModelRowItems(s));
}

// ---------- StashDialog
StashDialog::StashDialog(QWidget *parent) : QDialog(parent),
    m_model(new StashModel),
    m_proxyModel(new QSortFilterProxyModel),
    m_deleteAllButton(new QPushButton(tr("Delete &All..."))),
    m_deleteSelectionButton(new QPushButton(tr("&Delete..."))),
    m_showCurrentButton(new QPushButton(tr("&Show"))),
    m_restoreCurrentButton(new QPushButton(tr("R&estore..."))),
    //: Restore a git stash to new branch to be created
    m_restoreCurrentInBranchButton(new QPushButton(tr("Restore to &Branch..."))),
    m_refreshButton(new QPushButton(tr("Re&fresh")))
{
    setAttribute(Qt::WA_DeleteOnClose, true);  // Do not update unnecessarily
    setWindowTitle(tr("Stashes"));

    resize(599, 485);

    m_repositoryLabel = new QLabel(this);

    auto filterLineEdit = new FancyLineEdit(this);
    filterLineEdit->setFiltering(true);

    auto buttonBox = new QDialogButtonBox(this);
    buttonBox->setOrientation(Qt::Vertical);
    buttonBox->setStandardButtons(QDialogButtonBox::Close);

    // Buttons
    buttonBox->addButton(m_showCurrentButton, QDialogButtonBox::ActionRole);
    connect(m_showCurrentButton, &QPushButton::clicked,
            this, &StashDialog::showCurrent);
    buttonBox->addButton(m_refreshButton, QDialogButtonBox::ActionRole);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &StashDialog::forceRefresh);
    buttonBox->addButton(m_restoreCurrentButton, QDialogButtonBox::ActionRole);
    connect(m_restoreCurrentButton, &QPushButton::clicked,
            this, &StashDialog::restoreCurrent);
    buttonBox->addButton(m_restoreCurrentInBranchButton, QDialogButtonBox::ActionRole);
    connect(m_restoreCurrentInBranchButton, &QPushButton::clicked,
            this, &StashDialog::restoreCurrentInBranch);
    buttonBox->addButton(m_deleteSelectionButton, QDialogButtonBox::ActionRole);
    connect(m_deleteSelectionButton, &QPushButton::clicked,
            this, &StashDialog::deleteSelection);
    buttonBox->addButton(m_deleteAllButton, QDialogButtonBox::ActionRole);
    connect(m_deleteAllButton, &QPushButton::clicked,
            this, &StashDialog::deleteAll);

    // Models
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterKeyColumn(-1);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_stashView = new TreeView(this);
    m_stashView->setActivationMode(Utils::DoubleClickActivation);
    m_stashView->setModel(m_proxyModel);
    m_stashView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_stashView->setAllColumnsShowFocus(true);
    m_stashView->setUniformRowHeights(true);
    m_stashView->setFocus();

    using namespace Layouting;
    Row {
        Column {
            m_repositoryLabel,
            filterLineEdit,
            m_stashView
        },
        buttonBox
    }.attachTo(this);

    connect(filterLineEdit, &Utils::FancyLineEdit::filterChanged,
            m_proxyModel, &QSortFilterProxyModel::setFilterFixedString);
    connect(m_stashView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &StashDialog::enableButtons);
    connect(m_stashView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &StashDialog::enableButtons);
    connect(m_stashView, &Utils::TreeView::activated,
            this, &StashDialog::showCurrent);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

StashDialog::~StashDialog() = default;

void StashDialog::refresh(const FilePath &repository, bool force)
{
    if (m_repository == repository && !force)
        return;
    // Refresh
    m_repository = repository;
    m_repositoryLabel->setText(GitPlugin::msgRepositoryLabel(repository));
    if (m_repository.isEmpty()) {
        m_model->setStashes(QList<Stash>());
    } else {
        QList<Stash> stashes;
        GitClient::instance()->synchronousStashList(m_repository, &stashes);
        m_model->setStashes(stashes);
        if (!stashes.isEmpty()) {
            for (int c = 0; c < ColumnCount; c++)
                m_stashView->resizeColumnToContents(c);
        }
    }
    enableButtons();
}

void StashDialog::deleteAll()
{
    const QString title = tr("Delete Stashes");
    if (!ask(title, tr("Do you want to delete all stashes?")))
        return;
    QString errorMessage;
    if (GitClient::instance()->synchronousStashRemove(m_repository, QString(), &errorMessage))
        refresh(m_repository, true);
    else
        warning(title, errorMessage);
}

void StashDialog::deleteSelection()
{
    const QList<int> rows = selectedRows();
    QTC_ASSERT(!rows.isEmpty(), return);
    const QString title = tr("Delete Stashes");
    if (!ask(title, tr("Do you want to delete %n stash(es)?", nullptr, rows.size())))
        return;
    QString errorMessage;
    QStringList errors;
    // Delete in reverse order as stashes rotate
    for (int r = rows.size() - 1; r >= 0; r--)
        if (!GitClient::instance()->synchronousStashRemove(m_repository, m_model->at(rows.at(r)).name, &errorMessage))
            errors.push_back(errorMessage);
    refresh(m_repository, true);
    if (!errors.isEmpty())
        warning(title, errors.join('\n'));
}

void StashDialog::showCurrent()
{
    const int index = currentRow();
    QTC_ASSERT(index >= 0, return);
    GitClient::instance()->show(m_repository.toString(), QString(m_model->at(index).name));
}

// Suggest Branch name to restore 'stash@{0}' -> 'stash0-date'
static inline QString stashRestoreDefaultBranch(QString stash)
{
    stash.remove('{');
    stash.remove('}');
    stash.remove('@');
    stash += '-';
    stash += QDateTime::currentDateTime().toString("yyMMddhhmmss");
    return stash;
}

// Return next stash id 'stash@{0}' -> 'stash@{1}'
static inline QString nextStash(const QString &stash)
{
    const int openingBracePos = stash.indexOf('{');
    if (openingBracePos == -1)
        return QString();
    const int closingBracePos = stash.indexOf('}', openingBracePos + 2);
    if (closingBracePos == -1)
        return QString();
    bool ok;
    const int n = stash.mid(openingBracePos + 1, closingBracePos - openingBracePos - 1).toInt(&ok);
    if (!ok)
        return QString();
    QString rc =  stash.left(openingBracePos + 1);
    rc += QString::number(n + 1);
    rc += '}';
    return rc;
}

StashDialog::ModifiedRepositoryAction StashDialog::promptModifiedRepository(const QString &stash)
{
    QMessageBox box(QMessageBox::Question,
                    tr("Repository Modified"),
                    tr("%1 cannot be restored since the repository is modified.\n"
                       "You can choose between stashing the changes or discarding them.").arg(stash),
                    QMessageBox::Cancel, this);
    QPushButton *stashButton = box.addButton(tr("Stash"), QMessageBox::AcceptRole);
    QPushButton *discardButton = box.addButton(tr("Discard"), QMessageBox::AcceptRole);
    box.exec();
    const QAbstractButton *clickedButton = box.clickedButton();
    if (clickedButton == stashButton)
        return ModifiedRepositoryStash;
    if (clickedButton == discardButton)
        return ModifiedRepositoryDiscard;
    return ModifiedRepositoryCancel;
}

// Prompt for restore: Make sure repository is unmodified,
// prompt for a branch if desired or just ask to restore.
// Note that the stash to be restored changes if the user
// chooses to stash away modified repository.
bool StashDialog::promptForRestore(QString *stash,
                                   QString *branch /* = 0*/,
                                   QString *errorMessage)
{
    const QString stashIn = *stash;
    bool modifiedPromptShown = false;
    switch (GitClient::instance()->gitStatus(
                m_repository, StatusMode(NoUntracked | NoSubmodules), nullptr, errorMessage)) {
    case GitClient::StatusFailed:
        return false;
    case GitClient::StatusChanged: {
            switch (promptModifiedRepository(*stash)) {
            case ModifiedRepositoryCancel:
                return false;
            case ModifiedRepositoryStash:
                if (GitClient::instance()->synchronousStash(
                            m_repository, QString(), GitClient::StashPromptDescription).isEmpty()) {
                    return false;
                }
                *stash = nextStash(*stash); // Our stash id to be restored changed
                QTC_ASSERT(!stash->isEmpty(), return false);
                break;
            case ModifiedRepositoryDiscard:
                if (!GitClient::instance()->synchronousReset(m_repository))
                    return false;
                break;
            }
        modifiedPromptShown = true;
    }
        break;
    case GitClient::StatusUnchanged:
        break;
    }
    // Prompt for branch or just ask.
    if (branch) {
        *branch = stashRestoreDefaultBranch(*stash);
        if (!inputText(this, tr("Restore Stash to Branch"), tr("Branch:"), branch)
            || branch->isEmpty())
            return false;
    } else {
        if (!modifiedPromptShown && !ask(tr("Stash Restore"), tr("Would you like to restore %1?").arg(stashIn)))
            return false;
    }
    return true;
}

static inline QString msgRestoreFailedTitle(const QString &stash)
{
    return StashDialog::tr("Error restoring %1").arg(stash);
}

void StashDialog::restoreCurrent()
{
    const int index = currentRow();
    QTC_ASSERT(index >= 0, return);
    QString errorMessage;
    QString name = m_model->at(index).name;
    // Make sure repository is not modified, restore. The command will
    // output to window on success.
    if (promptForRestore(&name, nullptr, &errorMessage)
            && GitClient::instance()->synchronousStashRestore(m_repository, name)) {
        refresh(m_repository, true); // Might have stashed away local changes.
    } else if (!errorMessage.isEmpty()) {
        warning(msgRestoreFailedTitle(name), errorMessage);
    }
}

void StashDialog::restoreCurrentInBranch()
{
    const int index = currentRow();
    QTC_ASSERT(index >= 0, return);
    QString errorMessage;
    QString branch;
    QString name = m_model->at(index).name;
    if (promptForRestore(&name, &branch, &errorMessage)
            && GitClient::instance()->synchronousStashRestore(m_repository, name, false, branch)) {
        refresh(m_repository, true); // git deletes the stash, unfortunately.
    } else if (!errorMessage.isEmpty()) {
        warning(msgRestoreFailedTitle(name), errorMessage);
    }
}

int StashDialog::currentRow() const
{
    const QModelIndex proxyIndex = m_stashView->currentIndex();
    if (proxyIndex.isValid()) {
        const QModelIndex index = m_proxyModel->mapToSource(proxyIndex);
        if (index.isValid())
            return index.row();
    }
    return -1;
}

QList<int> StashDialog::selectedRows() const
{
    QList<int> rc;
    const QModelIndexList rows = m_stashView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : rows) {
        const QModelIndex index = m_proxyModel->mapToSource(proxyIndex);
        if (index.isValid())
            rc.push_back(index.row());
    }
    Utils::sort(rc);
    return rc;
}

void StashDialog::forceRefresh()
{
    refresh(m_repository, true);
}

void StashDialog::enableButtons()
{
    const bool hasRepository = !m_repository.isEmpty();
    const bool hasStashes = hasRepository && m_model->rowCount();
    const bool hasCurrentRow = hasRepository && hasStashes && currentRow() >= 0;
    m_deleteAllButton->setEnabled(hasStashes);
    m_showCurrentButton->setEnabled(hasCurrentRow);
    m_restoreCurrentButton->setEnabled(hasCurrentRow);
    m_restoreCurrentInBranchButton->setEnabled(hasCurrentRow);
    const bool hasSelection = !m_stashView->selectionModel()->selectedRows().isEmpty();
    m_deleteSelectionButton->setEnabled(hasSelection);
    m_refreshButton->setEnabled(hasRepository);
}

void StashDialog::warning(const QString &title, const QString &what, const QString &details)
{
    QMessageBox msgBox(QMessageBox::Warning, title, what, QMessageBox::Ok, this);
    if (!details.isEmpty())
        msgBox.setDetailedText(details);
    msgBox.exec();
}

bool StashDialog::ask(const QString &title, const QString &what, bool defaultButton)
{
    return QMessageBox::question(
                this, title, what, QMessageBox::Yes | QMessageBox::No,
                defaultButton ? QMessageBox::Yes : QMessageBox::No) == QMessageBox::Yes;
}

} // namespace Internal
} // namespace Git
