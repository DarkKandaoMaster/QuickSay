#pragma once

#include <QString>
#include <QtGlobal>

#include <functional>

struct OutputAction;

// SendInput 相关。所有函数都按 SendInput 实际插入的事件数判断成败；
// 只发出一部分时会补发对应的抬起事件，避免按键卡在按下状态。
namespace KeyInjector {

bool sendCtrlV();
bool sendCtrlC(); // “把选中的文字存为新短语”用来复制前台程序里的选区
bool sendCombo(const OutputAction &action);

// 抬起用户此刻按着的修饰键（通常就是触发短语的快捷键里的那个）。
// 只抬起、绝不在输出结束后按回去：输出期间用户可能已经松手，再注入一次按下会让修饰键永久卡住。
bool releasePressedModifiers();

// 钩子吞掉 Alt+←、Win+← 里的主键后，补一对无用途的按键（VK 0xE8），
// 免得松开 Alt/Win 时前台程序激活菜单栏或弹出开始菜单。AutoHotkey 的 #MenuMaskKey 也是这么做的。
void sendMenuMaskKey();
// 用户此刻是不是按着 Alt 或 Win。是的话抬起修饰键之前要先补一次 sendMenuMaskKey()：
// 快捷键的主键被 RegisterHotKey 吃掉了，前台程序只看到 Alt 按下又抬起，会去激活菜单栏
bool isAltOrWinPressed();

// 以 CF_HDROP 格式把一个文件放进剪贴板
bool setClipboardFile(const QString &filePath);

// 低级键盘钩子用：这次按键是不是本程序 SendInput 出来的（KBDLLHOOKSTRUCT 的 flags、dwExtraInfo）。
// 自家注入的键都在 dwExtraInfo 里带了标记，钩子见到就放行，免得模拟出来的方向键、回车、Ctrl+V 被当成用户输入；
// AutoHotkey 之类别人注入的键不受影响，照常当成用户按键处理。
bool isOwnInjection(quint32 flags, quintptr extraInfo);

} // namespace KeyInjector

// “把选中的文字存为新短语”：模拟 Ctrl+C 复制前台程序里选中的文字，等剪贴板真的变了再读出来。
// 和系统复制一样，复制到的文字会留在剪贴板里。
namespace SelectionCapture {

// 结果异步回调：复制到了就是那段文字；前台程序没响应 Ctrl+C（没选中东西、不支持复制）就是空串。
// 上一次还没结束时直接忽略这次请求，不回调
void start(std::function<void(const QString &text)> onFinished);

} // namespace SelectionCapture
