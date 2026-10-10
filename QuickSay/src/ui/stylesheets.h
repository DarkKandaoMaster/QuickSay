#pragma once

namespace StyleSheets {

// 仿 Windows 11 的主样式。不挂在 QApplication 上（设置窗口要原生控件外观），
// 而是挂给需要它的窗口：主窗口、添加/修改窗口、高级输入帮助、分组面板和几个没有父对象的右键菜单。
extern const char *const kApp;

// 分组面板在 kApp 之后追加的部分
extern const char *const kGroupPanel;

// 设置窗口专用。只写底色，控件外观全部交给 Qt 原生样式；所有选择器都以 #settingsWindow 开头，和 kApp 互不影响。
extern const char *const kSettings;

} // namespace StyleSheets
