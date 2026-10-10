#pragma once

#include <QString>
#include <QtGui/qwindowdefs.h>

class QWidget;

// 主窗口和分组面板都带 WS_EX_NOACTIVATE：能点，但点了不抢前台，短语才能粘进原来的前台程序。
namespace WindowStyle {

void addNoActivate(WId window);
// 切换样式并通知系统重算边框（进出搜索模式用）
void setNoActivate(WId window, bool enabled);

// 窗口中心点处最前面的顶层窗口是不是它自己（取不到窗口矩形时当作在最前）
bool isFrontmost(QWidget *widget);

// 显示（不激活）并按是否置顶调整 Z 序。insertAfter 为 0 时用 HWND_TOP。
void showNoActivate(WId window, bool topmost, WId insertAfter = 0);

// 呼出时禁用 DWM 过渡动画、收起时恢复：窗口一按就出现，收起时仍有淡出动画
void setTransitionsDisabled(WId window, bool disabled);

} // namespace WindowStyle

// 托盘右键菜单用 Win32 TrackPopupMenu 而不是 QMenu：外观由系统绘制（和 TrafficMonitor 一致），也不受 QuickSay 的 QSS 影响。
namespace TrayMenu {

enum class Choice {
    None,
    Settings,
    Quit,
};

// 阻塞到用户选完
Choice exec();

// 菜单弹出期间为 true。原生菜单不是 Qt 弹出控件，activePopupWidget() 看不到它，键盘钩子要靠这个放行。
bool isOpen();

} // namespace TrayMenu
