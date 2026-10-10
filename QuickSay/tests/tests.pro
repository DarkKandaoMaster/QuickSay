# 不碰系统的逻辑的回归测试（标签解析、版本比较、备份往返、设置迁移、分组/角标/方向键浏览）。和主程序分开编译，不影响 QuickSay.pro。
# 测试程序会在自己旁边写 config.json（备份要用一份完整的默认设置），已被本目录的 .gitignore 忽略。
# 和 QuickSay.pro 一样必须保持 CRLF：LF 换行加中文注释时 qmake 会吞掉下面的 QT += 那行。
QT += core gui widgets testlib
CONFIG += c++17 console testcase
CONFIG -= debug_and_release app_bundle

TARGET = quicksay_tests
INCLUDEPATH += ../src
OBJECTS_DIR = build/obj
MOC_DIR = build/moc

SOURCES += \
    tst_core.cpp \
    tst_ui.cpp \
    ../src/core/backup.cpp \
    ../src/core/files.cpp \
    ../src/core/groupmodel.cpp \
    ../src/core/phrase.cpp \
    ../src/core/settings.cpp \
    ../src/output/outputparser.cpp \
    ../src/ui/phraselist.cpp

HEADERS += \
    ../src/core/groupmodel.h
