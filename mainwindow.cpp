#include <QApplication>
#include <QtGui>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QIODevice>
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <QProgressDialog>

#include "mainwindow.h"

#include "dirfiletree.h"
#include "adclistreader.h"

#include "loadpathworker.h"

#include "qualz4file.h"

using ShowListing::DirFileTree;
using ShowListing::AdcListReader;

MainWindow::MainWindow(QApplication &application, QWidget *parent) : QMainWindow(parent)
{
    app = &application;
    dirFileTree = new DirFileTree;
    setCentralWidget(dirFileTree);

    createActions();
    createMenus();
    setAcceptDrops(true);

    statusBar()->showMessage(tr("Ready"));

    setWindowTitle(tr("ShowListing"));

    QSettings settings("ShowListing", "ShowListing 1");
    if (settings.contains("windowState") || settings.contains("geometry")) {
        restoreState(settings.value("windowState").toByteArray());
        restoreGeometry(settings.value("geometry").toByteArray());
        QSize size = settings.value("size").toSize();
        if (size.width() < 640 || size.height() < 480) {
            size.scale(640, 640, Qt::KeepAspectRatio);
        }
        resize(size);
    }
    else {
        resize(1024, 768);
    }
    lastOpenPath = QDir::currentPath();
}

void MainWindow::open()
{
    QString path =
            QFileDialog::getOpenFileName(this, tr("ShowListing - Open ADC FileListing"),
                                         lastOpenPath,
#if defined(WITH_BZIP2)
                                         tr("Supported FileListing (*.xml *.xml.bz2 *.xmlz4)")
#else
                                         tr("Supported FileListing (*.xml *.xmlz4)")
#endif
                                         + ";;" + tr("Uncompressed ADC FileListing (*.xml)")
#if defined(WITH_BZIP2)
                                         + ";;" + tr("Bzip2 (DC++) FileListing (*.xml.bz2)")
#endif
                                         + ";;" + tr("LZ4-compressed ADC FileListing (*.xmlz4)")
    );

    if (!path.isEmpty())
        openPath(path);
}

void MainWindow::openPath(const QString& fileName)
{
    QFileInfo fi(fileName);
    lastOpenPath = fi.dir().absolutePath();
    lastOpenFilePath = fileName;

    if (fi.suffix().toLower() == "xmlz4") {
        source = new QuaLz4File(fileName);
        if (!source->open(QFile::ReadOnly)) {
            QMessageBox::warning(this, tr("ShowListing - Cannot open file list"),
                                 tr("Cannot open or read file %1.\nThe compressed bytestream may be corrupt.")
                                 .arg(fileName));
            delete source;
            source = 0;
            return;
        }
    } else {
        source = new QFile(fileName);
        if (!source->open(QFile::ReadOnly | QFile::Text)) {
            QMessageBox::warning(this, tr("ShowListing - Cannot open file list"),
                                 tr("Cannot open or read file %1:\n%2.")
                                 .arg(fileName)
                                 .arg(source->errorString()));
            delete source;
            source = 0;
            return;
        }
    }

#if 0
    progressValue = 0;
    insertedRow = 0;
    listReader = new ShowListing::AdcListReader(dirFileTree);
    watcher = new QFutureWatcher<void>();

    progress = new QProgressDialog(tr("Processing %1...").arg(QDir::toNativeSeparators(fileName)),
                                                    QString(), 0, fi.size(), this);
    progress->setWindowModality(Qt::WindowModal);
    progress->show();

    QFuture<void> future = QtConcurrent::run(listReader, &ShowListing::AdcListReader::read, source, &insertedRow);

    QObject::connect(progress, SIGNAL(canceled()),
                   this, SLOT(slotOpenPathCancelled()));
    QObject::connect(listReader, SIGNAL(broadcastProgress(qint64)),
                   this, SLOT(slotBroadcastProgressReceived(qint64)));
    QObject::connect(watcher, SIGNAL(finished()),
                     this, SLOT(slotFutureWatchNotify()));
    watcher->setFuture(future);
    timer.start(500);
#else
    //FIXME: temporarily do everything ins GUI thread until a proper way is implemented
    progress = new QProgressDialog(QString(), QString(), 0, 0, this);
    watcher = new QFutureWatcher<void>();
    listReader = new ShowListing::AdcListReader(dirFileTree);
    listReader->read(source, &insertedRow);
    slotFutureWatchNotify();
#endif
}

void MainWindow::slotFutureWatchNotify()
{
    qDebug("call slotFutureWatchNotify()\n");
    timer.stop();
    Q_ASSERT_X(listReader != 0, "slotFutureWatchNotify()", "use-after-free listReader");
    Q_ASSERT_X(source != 0, "slotFutureWatchNotify()", "use-after-free source");
    if (!listReader->hasError() && insertedRow) {
        progress->reset();

        insertedRow->setData(0, Qt::UserRole, QString("%1").arg(QDir::toNativeSeparators(lastOpenFilePath)));
        insertedRow->setText(0, QString("[%1] %2").arg(lastOpenFilePath).arg(insertedRow->text(0)));
        insertedRow->setExpanded(true);
        // modify UI in GUI thread.
        dirFileTree->setUpdatesEnabled(false);
        dirFileTree->addTopLevelItem(insertedRow);
        dirFileTree->header()->resizeSections(QHeaderView::ResizeToContents);
        dirFileTree->setUpdatesEnabled(true);

        statusBar()->showMessage(tr("File loaded:%1:").arg(dirFileTree->property("base").toString()));
    } else {
        if (insertedRow) {
            dirFileTree->setUpdatesEnabled(false);
            dirFileTree->addTopLevelItem(insertedRow);
            delete insertedRow;
            dirFileTree->setUpdatesEnabled(true);
        }
        QMessageBox::warning(this, tr("ShowListing - Failed to open file list"),
                             tr("Cannot open file list %1, maybe the file is corrupt.\n\n%2")
                             .arg(lastOpenFilePath)
                             .arg(listReader->errorString()));
        progress->reset();
    }
    delete watcher;
    watcher = 0;
    delete progress;
    progress = 0;
    delete listReader;
    listReader = 0;
    delete source;
    source = 0;
}

void MainWindow::slotBroadcastProgressReceived(qint64 pos)
{
    lock_progressValue.lockForWrite();
    progressValue = pos;
    lock_progressValue.unlock();
    if (progress) {
        progress->setValue(pos);
    }
}

void MainWindow::slotOpenPathCancelled()
{
    if (listReader) {
        listReader->cancelProcessing();
    }
}

void MainWindow::slotTimer()
{
    if (progress != 0)
    {
        lock_progressValue.lockForRead();
        progress->setValue(progressValue);
        lock_progressValue.unlock();
    }
}


void MainWindow::onExport()
{
    QString fileName =
            QFileDialog::getSaveFileName(this, tr("ShowListing - Export Filelisting"),
                                         lastOpenPath,
                                         tr("Text Files (*.txt)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("ShowListing"),
                             tr("Cannot write file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
        return;
    }

    //if (dirFileTree->write(&file))
        //statusBar()->showMessage(tr("File saved"), 2000);
}

void MainWindow::about()
{
   QMessageBox::about(this, tr("About ShowListing"),
                      tr("This program allows you to browse a hierarchized file listing, as defined by the <b><a href=\"http://adc.sourceforge.net/ADC.html\">Advanced Direct Connect protocol</a></b>.<br>"
                         "Data is presented in a tree widget and shows cumulated folder sizes, to quickly visualize repartition of volume."));
}

void MainWindow::createActions()
{
    openAct = new QAction(tr("&Open FileList..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    exportAct = new QAction(tr("E&xport..."), this);
    exportAct->setShortcuts(QKeySequence::SaveAs);
    connect(exportAct, SIGNAL(triggered()), this, SLOT(onExport()));

    exitAct = new QAction(tr("&Quit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

    aboutAct = new QAction(tr("&About"), this);
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAct);
#if 0
    fileMenu->addAction(exportAct);
#endif
    fileMenu->addAction(exitAct);

    menuBar()->addSeparator();

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAct);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings("ShowListing", "ShowListing 1");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    if ( !isMaximized() ) {
            settings.setValue( "size", size() );
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
     event->acceptProposedAction();
}

void MainWindow::dragMoveEvent(QDragMoveEvent* event)
{
     event->acceptProposedAction();
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent* event)
{
     event->accept();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if(event->mimeData()->hasUrls()) {
        QList<QUrl> urllist(event->mimeData()->urls());
        for (int i = 0; i < urllist.size() && i < 5; ++i)
             openPath(urllist.at(i).toLocalFile());
    }
}
