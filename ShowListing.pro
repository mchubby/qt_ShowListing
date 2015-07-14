HEADERS       = mainwindow.h \
    dirfiletree.h \
    adclistreader.h \
    util.h \
    qualz4file.h \
    lz4.h \
    loadpathworker.h
SOURCES       = main.cpp \
                mainwindow.cpp \
    dirfiletree.cpp \
    adclistreader.cpp \
    qualz4file.cpp \
    lz4.c \
    loadpathworker.cpp

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
QT           += xml

OTHER_FILES +=

RESOURCES += \
    res.qrc


