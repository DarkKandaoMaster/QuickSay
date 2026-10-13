// 版本：2.1.1
// 更新内容：
// 1. 程序体积从 24.6 MB 减小到 13.3 MB。

#include "app/quicksayapp.h"
#include "core/files.h"
#include "core/settings.h"
#include "platform/startup.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    bool autostarted = false; // 注册表 Run 项和计划任务都会带上 --autostart
    for (int i = 1; i < argc; ++i) {
        // 提权去建计划任务的那份实例：建完就退出，退出码告诉等着它的实例成没成。
        // 必须在单实例检测之前处理，否则会被当成第二个实例拦下
        if (QString(argv[i]) == "--create-admin-task") return AutoStart::createAdminTask() ? 0 : 1;
        if (QString(argv[i]) == "--autostart") autostarted = true;
    }

    // 必须在 QApplication 之前：后启动的实例要在什么 Qt 对象都还没建的时候就退出
    if (SingleInstance::notifyIfRunning()) return 0;

    // 2.1.0 以前的数据文件搬进 data/。要赶在下面读“以管理员权限启动”之前
    DataFile::migrateLegacyFiles();

    // “以管理员权限启动”：手动启动时自己提权重开一份。放在单实例检测之后——已经在跑时双击 exe 只是想叫出窗口，不该弹 UAC。
    // 开机自启的那份绝不走这里：要么由计划任务直接以管理员权限起来，要么宁可这次以普通权限跑，也不在开机时弹 UAC
    if (Settings::readRunAsAdminFromFile() && !Elevation::isProcessElevated() && !autostarted) {
        SingleInstance::release(); // 先让出互斥体，否则提权的那份会被单实例检测拦下
        if (Elevation::relaunchElevated()) return 0;
        SingleInstance::notifyIfRunning(); // 用户拒绝了 UAC：抢回互斥体，以普通权限接着跑
    }

    QApplication app(argc, argv);
    QuickSayApp quickSay(autostarted);
    return app.exec();
}
