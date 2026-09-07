QT += core gui widgets network

CONFIG += c++11

TARGET = hmi-app
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    touchcanvas.cpp

HEADERS += \
    mainwindow.h \
    touchcanvas.h

target.path = /usr/bin
INSTALLS += target
