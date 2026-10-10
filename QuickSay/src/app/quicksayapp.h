#pragma once

#include "core/groupmodel.h"
#include "hotkey/globalhotkey.h"

#include <QObject>
#include <QTimer>

#include <memory>

class AppNativeEventFilter;
class MainWindow;
class QSystemTrayIcon;
class QWinEventNotifier;
class SettingsWindow;

// 程序本体：在 QApplication 之后创建，把各个窗口、托盘、全局快捷键和系统集成接起来。
class QuickSayApp : public QObject {
  public:
    // autostarted：带 --autostart 启动（注册表 Run 项或计划任务），只进托盘、不弹主窗口
    explicit QuickSayApp(bool autostarted);
    ~QuickSayApp() override;

    // 以管理员权限重启自己。优先用已授权的计划任务（不再弹 UAC），否则 runas。
    // 成功时已经调用了 quit；用户拒绝 UAC 时恢复原状并返回 false。
    bool restartElevated();

    // 导入备份后重启：经由 Explorer 以普通权限启动新实例，再让它按导入的设置自行提权。无论成败本进程都退出。
    void restartAfterImport(QWidget *errorDialogParent);

    // 重装两个低级键盘钩子。主线程卡久了（等 UAC、COM 建计划任务）钩子会被系统悄悄摘掉，卡完马上调一次，不用等看门狗
    static void reinstallHooks();

  private:
    void setHotkeysRegistered(bool registered);
    // “把选中的文字存为新短语”快捷键：复制前台程序的选区，存进主窗口当前分组末尾
    void captureSelection();
    void releaseSingleInstance();
    void createTrayIcon();

    GroupModel groups_;
    HotkeyRegistry hotkeys_;
    std::unique_ptr<MainWindow> mainWindow_;
    std::unique_ptr<SettingsWindow> settingsWindow_;
    std::unique_ptr<AppNativeEventFilter> nativeFilter_;
    QWinEventNotifier *instanceNotifier_ = nullptr;
    QSystemTrayIcon *trayIcon_ = nullptr;
    QTimer hookWatchdog_;
};
