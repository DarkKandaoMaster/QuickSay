#pragma once

#include <QString>

// 在鼠标旁边显示一行不抢焦点、点不中的小提示，过一会儿自己消失。
// 全程只有一个提示框：上一个还没消失时再调，直接换掉内容并重新计时，不会像 Windows 通知那样排队。
void showCursorHint(const QString &text);
