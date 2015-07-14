#ifndef ADCLISTREADER_H
#define ADCLISTREADER_H

#include <QIcon>
#include <QXmlStreamReader>

QT_BEGIN_NAMESPACE
class QTreeWidgetItem;
QT_END_NAMESPACE

#include "dirfiletree.h"

namespace ShowListing{
//! [0]
class AdcListReader : public QObject
{
    Q_OBJECT

public:
//! [1]
    AdcListReader(ShowListing::DirFileTree *treeWidget);
//! [1]

    int read(QIODevice *device, QTreeWidgetItem **);

    bool hasError() const;
    QString errorString() const;
    QString errorString(QString) const;
    void cancelProcessing();

signals:
    void broadcastProgress(qint64 curPos);

private:
//! [2]
    void readAdcList(QTreeWidgetItem *item);
    void readDirectory(QTreeWidgetItem *item);
    void readFile(QTreeWidgetItem *item);

    QTreeWidgetItem *createChildItem(QTreeWidgetItem *item, int Type = 0);

    QXmlStreamReader xml;
    ShowListing::DirFileTree *treeWidget;
    bool cancelRequested;
//! [2]

};
//! [0]
}

#endif
