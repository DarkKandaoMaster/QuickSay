QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

include(.\QHotkey-1.5.0\qhotkey.pri)
DEFINES += QAPPLICATION_CLASS=QApplication #告诉SingleApplication继承自QApplication而不是默认的QCoreApplication，于是SingleApplication就会继承QApplication的所有方法
include(.\SingleApplication-3.5.3\singleapplication.pri)
RC_ICONS += icons/icon.ico #哈哈哈！终于让这句代码运行成功了！一切造物的工都已完毕！说下我踩到的坑：1.这个ico文件不能是中文；2.是+=，不是=；3.一定要确保这个ico文件是真的ico文件呀，也就是说：1.如果你和我一样看图软件用的是ImageGlass，那么这个ico文件是打得开的；2.去查看ico文件的网站上，这个文件是能打得开的。woc我一开始是在一个破网站上转ico文件，结果就因为这个ico文件根本打不开，所以试了好久好久...
CONFIG -= debug_and_release #写上这句代码，再取消影子构建，运行程序的时候就不会生成debug文件夹和release文件夹了
