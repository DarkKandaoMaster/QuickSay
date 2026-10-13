QT += core gui widgets
CONFIG += c++17

# 影子构建：Debug 编到 build/debug，Release 编到 build/release，各有各的 data/。
# 一个构建目录只放一种配置，不要 qmake 再套一层 debug/、release/ 子目录，exe 直接落在构建目录里
CONFIG -= debug_and_release

INCLUDEPATH += src
OBJECTS_DIR = obj
MOC_DIR = moc
RCC_DIR = rcc
UI_DIR = ui

SOURCES += \
    src/app/quicksayapp.cpp \
    src/app/updatechecker.cpp \
    src/core/backup.cpp \
    src/core/files.cpp \
    src/core/groupmodel.cpp \
    src/core/phrase.cpp \
    src/core/settings.cpp \
    src/hotkey/globalhotkey.cpp \
    src/hotkey/hotkeyinput.cpp \
    src/main.cpp \
    src/output/keyinjector.cpp \
    src/output/outputparser.cpp \
    src/output/outputrunner.cpp \
    src/platform/nativeui.cpp \
    src/platform/startup.cpp \
    src/ui/batchedit.cpp \
    src/ui/grouppanel.cpp \
    src/ui/mainwindow.cpp \
    src/ui/phraseeditor.cpp \
    src/ui/phraselist.cpp \
    src/ui/popups.cpp \
    src/ui/settingswindow.cpp \
    src/ui/stylesheets.cpp

HEADERS += \
    src/app/quicksayapp.h \
    src/app/updatechecker.h \
    src/core/backup.h \
    src/core/files.h \
    src/core/groupmodel.h \
    src/core/phrase.h \
    src/core/settings.h \
    src/core/version.h \
    src/hotkey/globalhotkey.h \
    src/hotkey/hotkeyinput.h \
    src/output/keyinjector.h \
    src/output/outputparser.h \
    src/output/outputrunner.h \
    src/platform/nativeui.h \
    src/platform/startup.h \
    src/ui/batchedit.h \
    src/ui/grouppanel.h \
    src/ui/mainwindow.h \
    src/ui/phraseeditor.h \
    src/ui/phraselist.h \
    src/ui/popups.h \
    src/ui/settingswindow.h \
    src/ui/stylesheets.h

# 计划任务、SID、UAC：taskschd ole32 oleaut32 advapi32 shell32 uuid；会话通知：wtsapi32；
# 托盘菜单图标：gdi32；检查更新：winhttp；窗口动画、圆角：dwmapi
LIBS += -ltaskschd -lole32 -loleaut32 -ladvapi32 -lshell32 -lwtsapi32 -luuid -lgdi32 -lwinhttp -ldwmapi

include(QHotkey-1.5.0/qhotkey.pri)

# 图标编进 exe，exe 旁边不用再带 icons/
RESOURCES += QuickSay.qrc

# 文件名不能有中文，而且必须是真正的 ico 文件
RC_ICONS += icons/icon.ico
