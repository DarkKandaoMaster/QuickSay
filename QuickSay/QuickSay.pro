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

LIBS += -ltaskschd -lole32 -loleaut32 -ladvapi32 -lshell32 -lwtsapi32 -luuid -lgdi32 -lwinhttp #以管理员权限开机自启要用到计划任务、SID和UAC相关库；托盘原生右键菜单的图标位图要用gdi32的CreateDIBSection；高级输入停止机制用wtsapi32监听锁屏、注销和会话断开；备份导入后通过Explorer重启需要Shell COM接口的GUID；检查更新用winhttp向GitHub Releases API发请求

include(.\QHotkey-1.5.0\qhotkey.pri)
RC_ICONS += icons/icon.ico #哈哈哈！终于让这句代码运行成功了！一切造物的工都已完毕！说下我踩到的坑：1.这个ico文件不能是中文；2.是+=，不是=；3.一定要确保这个ico文件是真的ico文件呀，也就是说：1.如果你和我一样看图软件用的是ImageGlass，那么这个ico文件是打得开的；2.去查看ico文件的网站上，这个文件是能打得开的。woc我一开始是在一个破网站上转ico文件，结果就因为这个ico文件根本打不开，所以试了好久好久...
CONFIG -= debug_and_release #写上这句代码，再取消影子构建，运行程序的时候就不会生成debug文件夹和release文件夹了
