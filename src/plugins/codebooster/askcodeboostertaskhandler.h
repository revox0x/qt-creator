#ifndef ASKCODEBOOSTERTASKHANDLER_H
#define ASKCODEBOOSTERTASKHANDLER_H

#include <projectexplorer/itaskhandler.h>

namespace CodeBooster::Internal {

class AskCodeBoosterTaskHandler : public ProjectExplorer::ITaskHandler
{
    Q_OBJECT
public:
    bool canHandle(const ProjectExplorer::Task &task) const override;
    void handle(const ProjectExplorer::Tasks &tasks) override;
    QAction *createAction(QObject *parent) const override;

signals:
    void askCompileError(const QString &sysMsg, const QString &userMsg);


private:
    static QString sysMessage;
};


}


#endif // ASKCODEBOOSTERTASKHANDLER_H
