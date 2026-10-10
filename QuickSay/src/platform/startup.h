#pragma once

#include <QString>

// 跨完整性级别的单实例检测：命名互斥体占位，命名事件通知已在运行的实例显示主窗口。
// 不用 QSharedMemory/QLocalServer：管理员实例建出的高完整性对象，中完整性实例打不开，检测会失效。
namespace SingleInstance {

// 抢互斥体。已经有实例在跑时通知它显示主窗口并返回 true（调用方直接退出）。必须在 QApplication 之前调用。
bool notifyIfRunning();

// 放掉互斥体和事件。只在重启自己之前用：不让出来，新实例会被当成“已有实例”而退出。
void release();

// 已在运行的实例用 QWinEventNotifier 监听的事件（手动重置，处理完要 ResetEvent）
void *showWindowEvent();

// 用已授权的计划任务以 --autostart 拉起新实例前，预先把“显示窗口”事件置位，让新实例进入事件循环后显示主窗口。
// 返回的句柄要等新实例抢到互斥体后再关，见 waitForNewInstance。
void *presignalShowWindow();
void waitForNewInstance();

} // namespace SingleInstance

namespace Elevation {

bool isProcessElevated();

// 用 runas 重新启动一份提权的自己。用户在 UAC 上点“否”返回 false。
bool relaunchElevated();

// 用 runas 拉起带 --create-admin-task 的提权实例去建计划任务，等它退出并按退出码判断成败。已提权时直接建。
bool createAdminTaskWithUac();

// 让桌面 Explorer 启动一个不带参数的新实例。即使当前是管理员进程，新实例也先以普通权限启动，再按导入后的设置自行提权。
bool launchViaExplorer(QString &error);

} // namespace Elevation

// 开机自启有两条路，必须互斥（同时存在会开机启动两个实例）：
//   普通权限：注册表 HKCU\...\Run 的 QuickSay 键，值为 "exe路径" --autostart
//   管理员权限：计划任务「QuickSay 管理员自启」，登录时以最高权限启动且不弹 UAC
// 由 Settings::autoStart() / runAsAdmin() 两个开关决定走哪条：两个都开才走计划任务。
namespace AutoStart {

// 把系统里的自启状态调成和设置一致。quiet=true 时绝不弹 UAC（启动时用）：
// 需要建任务却没提权就先退回注册表 Run 项，留到下次以管理员身份运行时再补。失败返回 false。
bool apply(bool quiet);

// 系统里自启的真实状态（用户可能绕过 QuickSay 自己删了任务或注册表项）
bool isEnabledInSystem();

// 下面三个直接操作计划任务。任务的 SDDL 给了当前用户完全控制，所以删除不需要提权。
bool queryAdminTask(QString *exePath);
bool deleteAdminTask();
bool createAdminTask(); // 必须在提权进程里调用

// 用已授权的计划任务立即以管理员权限启动一份新实例，不再弹 UAC。没有任务或启动失败返回 false。
bool runAdminTaskNow();

} // namespace AutoStart
