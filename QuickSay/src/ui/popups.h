#pragma once

#include <QString>

#include <functional>

// 在鼠标旁边显示一行不抢焦点、点不中的小提示，过一会儿自己消失。
// 全程只有一个提示框：上一个还没消失时再调，直接换掉内容并重新计时，不会像 Windows 通知那样排队。
void showCursorHint(const QString &text);

class QPoint;

// 短语右键“选取文字”：在鼠标旁边弹出一个显示短语全文的小窗口，用鼠标拖选一段，松开鼠标立刻关闭并把选中的部分交给 onPicked。
// 用 Qt::Popup 而不是普通窗口：和右键菜单一样不抢前台，选完直接输出到原来的程序；点窗口外面就取消。
namespace SelectTextPopup {

void show(const QString &text, const QPoint &globalPos, std::function<void(const QString &picked)> onPicked);

} // namespace SelectTextPopup
