#ifndef DIRFILETREE_H
#define DIRFILETREE_H

#include <QDomDocument>
#include <QHash>
#include <QIcon>
#include <QTreeWidget>
#include <QHeaderView>

namespace ShowListing{
class DirFileTree : public QTreeWidget
{
    Q_OBJECT

public:
    static const int RootType = QTreeWidgetItem::UserType;
    static const int DirType = QTreeWidgetItem::UserType + 1;
    static const int FileType = QTreeWidgetItem::UserType + 2;

public:
    DirFileTree(QWidget *parent = 0);

    QIcon catalogIcon;
    QIcon folderIcon;
    QIcon fileIcon;

    QPixmap catalogPixmap;

    Q_PROPERTY(QString generator READ generator WRITE setGenerator)
    Q_PROPERTY(QString base READ base WRITE setBase)

private:
    QString _generator;
    QString generator() const { return _generator; }
    void setGenerator(QString rhsgenerator) { _generator = rhsgenerator; }

    QString _base;
    QString base() const { return _base; }
    void setBase(QString rhsbase) { _base = rhsbase; }

};
}

#endif
