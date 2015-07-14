#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStatusBar>
#include <QReadWriteLock>
#include <QTimer>
#include <QFutureWatcher>

namespace ShowListing{
class DirFileTree;
class AdcListReader;
}
QT_BEGIN_NAMESPACE
class QProgressDialog;
class QTreeWidgetItem;
QT_END_NAMESPACE


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QApplication &application, QWidget *parent = 0);
    void openPath(const QString& fileName);

public slots:
    void open();
    void onExport();
    void about();

private slots:
    void slotFutureWatchNotify();
    void slotBroadcastProgressReceived(qint64 pos);
    void slotOpenPathCancelled();
    void slotTimer();

protected:
    virtual void closeEvent(QCloseEvent *);
    virtual void dragEnterEvent(QDragEnterEvent* event);
    virtual void dragMoveEvent(QDragMoveEvent* event);
    virtual void dragLeaveEvent(QDragLeaveEvent* event);
    virtual void dropEvent(QDropEvent* event);

private:
    void createActions();
    void createMenus();

    ShowListing::DirFileTree *dirFileTree;

    QApplication *app;
    QMenu *fileMenu;
    QMenu *helpMenu;
    QAction *openAct;
    QAction *exportAct;
    QAction *exitAct;
    QAction *aboutAct;

    QString lastOpenPath;
    QString lastOpenFilePath;

    QTimer timer;
    QProgressDialog *progress;
    QReadWriteLock lock_progressValue;
    qint64 progressValue;
    ShowListing::AdcListReader *listReader;
    QIODevice *source;
    QTreeWidgetItem *insertedRow;
    QFutureWatcher<void> *watcher;
};

#endif
