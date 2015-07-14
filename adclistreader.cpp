#include <QtGui>
#include <QDir>

#include "adclistreader.h"
#include "util.h"

using ShowListing::AdcListReader;

namespace {
class Sleep : public QThread
{
public:
    static void msleep(int ms)
    {
        QThread::msleep(ms);
    }
};
}

static const QString sDIRECTORY = "Directory";
static const QString sFILE = "File";
static const QString sName = "Name";
static const QString sSize = "Size";
static const QString sDate = "Date";

namespace {

inline Qt::GlobalColor getColorFromSize(const qulonglong& val)
{
    if(val >= (1ULL << 40)) {
        return Qt::darkRed;
    }
    else if(val >= (500 * 1ULL << 30)) {
        return Qt::darkMagenta;
    }
    else if(val >= (100 * 1ULL << 30)) {  // 2^30 = 1 GiB
        return Qt::darkGreen;
    }
    else if(val >= (5 * 1ULL << 30)) {
        return Qt::black;
    }
    return Qt::darkGray;
}

}


//! [0]
AdcListReader::AdcListReader(ShowListing::DirFileTree *treeWidget)
    : treeWidget(treeWidget), cancelRequested(false)
{
}
//! [0]

//! [1]
int AdcListReader::read(QIODevice *device, QTreeWidgetItem **retVal)
{
    xml.setDevice(device);
    if (xml.readNextStartElement()) {
        QXmlStreamAttributes attr(xml.attributes());
        if (xml.name() == "FileListing" && attr.value("Version") == "1") {
            // Read extra metadata
            treeWidget->setProperty("generator", attr.value("Generator").toString().trimmed());
            treeWidget->setProperty("base", attr.value("Base").toString().trimmed());

            QTreeWidgetItem *topLevelNode = createChildItem(0, ShowListing::DirFileTree::RootType);
            topLevelNode->setBackgroundColor(0, QColor(240, 240, 255));
            topLevelNode->setTextColor(0, Qt::darkMagenta);
            topLevelNode->setText(0, attr.value("GeneratedDate").toString().trimmed() != "" ? QString("Date=%1").arg(attr.value("GeneratedDate").toString().trimmed()) : "");
            topLevelNode->setIcon(0, treeWidget->catalogIcon);
            topLevelNode->setData(1, Qt::UserRole, 0);

            readAdcList(topLevelNode);

            qulonglong totalSize = topLevelNode->data(1, Qt::UserRole).toLongLong();
            topLevelNode->setText(1, QString("[ %1 ]").arg(humanizeBigNums(totalSize, 2)));
            topLevelNode->setTextColor(1, getColorFromSize(totalSize));
            if (retVal) {
                *retVal = topLevelNode;
            }
        }
        else {
            xml.raiseError(QObject::tr("The file is not an ADC FileListing version 1 XML file. Found root element: %1")
                       .arg(xml.name().toString()));
        }
    }
    return xml.hasError();
}
//! [1]

bool AdcListReader::hasError() const
{
    return xml.hasError();
}

//! [2]
QString AdcListReader::errorString() const
{
    return QObject::tr("At Line[%1]:Column[%2],\n%3")
            .arg(xml.lineNumber())
            .arg(xml.columnNumber())
            .arg(xml.errorString());
}
//! [2]

//! [2]
QString AdcListReader::errorString(QString customError) const
{
    return QObject::tr("At Line[%1]:Column[%2],\n%3")
            .arg(xml.lineNumber())
            .arg(xml.columnNumber())
            .arg(customError);
}
//! [2]

void AdcListReader::cancelProcessing()
{
    cancelRequested = true;
}



//! [3]
void AdcListReader::readAdcList(QTreeWidgetItem *rootItem)
{
    while (!cancelRequested && !xml.hasError() && xml.readNextStartElement())
    {
        if (xml.name() == sDIRECTORY)
            readDirectory(rootItem);
        else if (xml.name() == sFILE)
            readFile(rootItem);
        else
            xml.skipCurrentElement();
    }
    if (cancelRequested) {
        xml.raiseError(QObject::tr("Cancel requested by user."));
    }
}
//! [3]

void AdcListReader::readDirectory(QTreeWidgetItem *item)
{
    QString pathParent = item ? item->data(0, Qt::UserRole).toString() : QDir::separator();
    QString filename = xml.attributes().value(sName).toString().trimmed();
    bool isConvOk = false;
    if (filename == "") {
        xml.raiseError(errorString(QObject::tr("Invalid Entry: <%1> has a missing or empty %2= attribute.")
                                   .arg(sDIRECTORY).arg(sName)));
        return;
    }

    QString filedate = xml.attributes().value(sDate).toString().trimmed();

    QTreeWidgetItem *folder = createChildItem(item, ShowListing::DirFileTree::DirType);
    folder->setIcon(0, treeWidget->folderIcon);
    folder->setData(0, Qt::UserRole, pathParent + QDir::separator() + filename);
    folder->setText(0, filename);
    folder->setData(1, Qt::UserRole, 0);

    if (xml.attributes().value("Incomplete").toString().trimmed() == "1") {
        folder->setTextColor(0, QColor(Qt::blue));
    }

    qulonglong filedateparsed = filedate.toLongLong(&isConvOk);
    if (isConvOk) {
        folder->setData(2, Qt::UserRole, filedateparsed);
    }


    while (!cancelRequested && !xml.hasError() && xml.readNextStartElement()) {
        // When a directory has been processed, notify listeners
        emit broadcastProgress(xml.device()->pos());
        if (xml.name() == sDIRECTORY)
            readDirectory(folder);
        else if (xml.name() == sFILE)
            readFile(folder);
        else
            xml.skipCurrentElement();
    }
    if (cancelRequested) {
        return;
    }


    qulonglong folderSize = folder->data(1, Qt::UserRole).toLongLong();
    folder->setText(1, QString(">> %1").arg(humanizeBigNums(folderSize, 2)));
    folder->setTextColor(1, getColorFromSize(folderSize));
    qulonglong parentsize = folderSize + item->data(1, Qt::UserRole).toLongLong();
    item->setData(1, Qt::UserRole, parentsize);
}

void AdcListReader::readFile(QTreeWidgetItem *item)
{
    QString pathParent = item ? item->data(0, Qt::UserRole).toString() : QDir::separator();
    QString filename = xml.attributes().value(sName).toString().trimmed();
    bool isConvOk = false;

    if (filename == "") {
        xml.raiseError(errorString(QObject::tr("Invalid Entry: <%1> has a missing or empty %2= attribute.")
                       .arg(sFILE).arg(sName)));
        return;
    }

    QString filesize = xml.attributes().value(sSize).toString().simplified();
    qulonglong filesizeparsed = filesize.toLongLong(&isConvOk);

    QTreeWidgetItem *file = createChildItem(item, ShowListing::DirFileTree::FileType);
    file->setIcon(0, treeWidget->fileIcon);
    file->setData(0, Qt::UserRole, pathParent + QDir::separator() + filename);
    file->setText(0, filename);
    if (filesize == "" || !isConvOk) {
//Do not mandate a file size, particular items (such as symlinks) may have this value omitted
//        xml.raiseError(errorString(QObject::tr("Invalid Entry: <%1> has a missing or empty %2= attribute, or is not a valid number.")
//                       .arg(sFILE).arg(sSize)));
//        return;
    } else {
        file->setData(1, Qt::UserRole, filesizeparsed);
        file->setText(1, humanizeBigNums(filesizeparsed, 2));
    }

    qulonglong parentsize = filesizeparsed + item->data(1, Qt::UserRole).toLongLong();
    item->setData(1, Qt::UserRole, parentsize);

    while (!cancelRequested && xml.readNextStartElement()) {
        if (xml.name() == sDIRECTORY || xml.name() == sFILE) {
            xml.raiseError(errorString(QObject::tr("Invalid Entry <%1 Name=\"%2\">: has unexpected child element <%3>.")
                           .arg(sFILE)
                           .arg(filename)
                           .arg(xml.name().toString())));
            return;
        }
        else
            xml.skipCurrentElement();
    }
}

QTreeWidgetItem *AdcListReader::createChildItem(QTreeWidgetItem *item, int Type)
{
    return new QTreeWidgetItem(item, Type);
}
