#pragma once

class MainWindow;

// 用记事本批量编辑一个分组下全部短语的正文（备注和快捷键原样保留，按顺序对应）。
// 记事本是异步进程，开着期间锁住 QuickSay 的所有窗口、暂停短语输出和呼出快捷键、键盘钩子放行一切按键。
namespace BatchEdit {

bool isActive();
void start(MainWindow *mainWindow, int groupIndex);

} // namespace BatchEdit
