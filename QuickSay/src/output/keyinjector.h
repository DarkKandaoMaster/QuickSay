#pragma once

#include <QString>
#include <QtGlobal>

struct OutputAction;

// SendInput 相关。所有函数都按 SendInput 实际插入的事件数判断成败；
// 只发出一部分时会补发对应的抬起事件，避免按键卡在按下状态。
namespace KeyInjector {

bool sendCtrlV();
bool sendCombo(const OutputAction &action);

// 抬起用户此刻按着的修饰键（通常就是触发短语的快捷键里的那个）。
// 只抬起、绝不在输出结束后按回去：输出期间用户可能已经松手，再注入一次按下会让修饰键永久卡住。
bool releasePressedModifiers();

// 钩子吞掉 Alt+←、Win+← 里的主键后，补一对无用途的按键（VK 0xE8），
// 免得松开 Alt/Win 时前台程序激活菜单栏或弹出开始菜单。AutoHotkey 的 #MenuMaskKey 也是这么做的。
void sendMenuMaskKey();

// 以 CF_HDROP 格式把一个文件放进剪贴板
bool setClipboardFile(const QString &filePath);

// 低级键盘钩子用：这次按键是不是本程序 SendInput 出来的（KBDLLHOOKSTRUCT 的 flags、dwExtraInfo）。
// 自家注入的键都在 dwExtraInfo 里带了标记，钩子见到就放行，免得模拟出来的方向键、回车、Ctrl+V 被当成用户输入；
// AutoHotkey 之类别人注入的键不受影响，照常当成用户按键处理。
bool isOwnInjection(quint32 flags, quintptr extraInfo);

} // namespace KeyInjector
