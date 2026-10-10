#pragma once

class MainWindow;

// 用记事本批量编辑一个分组下全部短语的正文，短语之间一行 -----，可以随便增删、改顺序。
// 应用时按正文原样对回原来的短语：正文没改的保留备注和快捷键，改过的当新短语。
// 批量添加：打开一个空文件，每个非空行一条，保存关掉后加在分组末尾，不碰原有短语。
// 编辑期间锁住 QuickSay 的所有窗口、暂停短语输出和呼出快捷键、托盘不响应、键盘钩子放行一切按键，直到记事本关掉这个文件。
namespace BatchEdit {

bool isActive();
void start(MainWindow *mainWindow, int groupIndex);
void startAdd(MainWindow *mainWindow, int groupIndex);

} // namespace BatchEdit
