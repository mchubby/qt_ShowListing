#ifndef LOADPATHWORKER_H
#define LOADPATHWORKER_H

#include <QObject>
#include <QRunnable>
#include <QReadWriteLock>
#include <QTreeWidgetItem>
#include <QIODevice>

namespace ShowListing{class DirFileTree;class AdcListReader;}

class LoadPathWorker : public QObject, public QRunnable
{
    Q_OBJECT

public:
    LoadPathWorker(QIODevice *inputIo, ShowListing::DirFileTree *treewidget, qint64 *pWrittenTracker, QReadWriteLock *lock, QTreeWidgetItem **ppInsertedItem);
public:
    void run();
    void postCancelRequest();

private:
    QIODevice *io;
    ShowListing::DirFileTree *tree;
    qint64 *pWritten;
    QReadWriteLock *lock;
    QTreeWidgetItem **ppInsertedItem;

    ShowListing::AdcListReader *runningReader;

private slots:
    void slotBroadcastProgressReceived(qint64 pos);

signals:
    void signalWorkFinished(int status, const QString& message);
};

#endif // LOADPATHWORKER_H
