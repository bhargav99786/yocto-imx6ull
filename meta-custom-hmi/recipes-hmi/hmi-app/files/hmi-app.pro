QT += core gui widgets network

CONFIG += c++11

TARGET = hmi-app
TEMPLATE = app

SOURCES += \
    main.cpp \
    loginwindow.cpp \
    mainwindow.cpp \
    touchcanvas.cpp \
    numpaddialog.cpp

HEADERS += \
    loginwindow.h \
    mainwindow.h \
    touchcanvas.h \
    numpaddialog.h

target.path = /usr/bin
INSTALLS += target
