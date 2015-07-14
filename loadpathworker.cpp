#include "loadpathworker.h"

#include "adclistreader.h"

LoadPathWorker::LoadPathWorker(QIODevice *inputIo,
                               ShowListing::DirFileTree *treewidget,
                               qint64 *pWrittenTracker,
                               QReadWriteLock *lock,
                               QTreeWidgetItem **ppInsertedItem)
    : runningReader(0)
{
    io = inputIo;
    tree = treewidget;
    pWritten = pWrittenTracker;
    this->lock = lock;
    this->ppInsertedItem = ppInsertedItem;
}

void LoadPathWorker::run()
{
    ShowListing::AdcListReader reader(tree);
    QString message;

    QObject::connect(&reader, SIGNAL(broadcastProgress(qint64)),
                     this, SLOT(slotBroadcastProgressReceived(qint64)));

    runningReader = &reader;
    int rc = reader.read(io, ppInsertedItem);
    runningReader = 0;
    if (rc != 0) {
        message = reader.errorString();
    }
    emit signalWorkFinished(rc, message);
}

void LoadPathWorker::slotBroadcastProgressReceived(qint64 pos)
{
    lock->lockForWrite();
    *pWritten = pos;
    lock->unlock();
}

void LoadPathWorker::postCancelRequest()
{
    if (runningReader) {
        runningReader->cancelProcessing();
    }
}
