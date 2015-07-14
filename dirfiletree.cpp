#include <QtGui>
#include <QHeaderView>

#include "dirfiletree.h"

namespace ShowListing{
DirFileTree::DirFileTree(QWidget *parent)
    : QTreeWidget(parent)
{
    QStringList labels;
    labels << tr("Folder") << tr("Size");

    QFont curFont(font());
    curFont.setPointSizeF(8.5);
    setFont(curFont);
    QFont hedFont(header()->font());
    hedFont.setPointSizeF(6);
    header()->setFont(hedFont);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    header()->setResizeMode(QHeaderView::Interactive);
#else
    header()->setSectionResizeMode(QHeaderView::Interactive);
#endif
    setHeaderLabels(labels);

    catalogPixmap = QPixmap("://ozturk_developerkit_paste.png");
    catalogIcon.addPixmap(catalogPixmap);
    folderIcon.addPixmap(style()->standardPixmap(QStyle::SP_DirClosedIcon),
                         QIcon::Normal, QIcon::Off);
    folderIcon.addPixmap(style()->standardPixmap(QStyle::SP_DirOpenIcon),
                         QIcon::Normal, QIcon::On);
    fileIcon.addPixmap(style()->standardPixmap(QStyle::SP_FileIcon));
}


}
