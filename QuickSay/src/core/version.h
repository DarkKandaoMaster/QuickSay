#pragma once

// 版本：2.0.2
// 更新内容：
// 1. 快捷键支持小键盘按键，并区分主键盘方向键与小键盘方向键（小键盘上的键显示为 Num+…，比如 Ctrl+Num+4、Num+Left；小键盘回车仍显示为 Enter，主键盘回车显示为 Return）。
// 2. 行为变化：旧数据里的 Ctrl+Left、Ins 这类快捷键，以前主键盘和小键盘都能触发，升级后只认主键盘那一个；需要用小键盘触发的请重新录入。

#include <QRegularExpression>
#include <QString>
#include <QStringList>

inline const QString kAppVersion = QStringLiteral("2.0.2");

// 只在备份文件结构发生不兼容变化时才改，不跟着程序版本走。
inline const QString kBackupFormatVersion = QStringLiteral("1.0.0");

// 按最多四段数字比较版本号；任一方不是纯数字点分格式时 known 置为 false 并返回 0。
inline int compareVersions(const QString &left, const QString &right, bool &known) {
    static const QRegularExpression pattern("^\\d+(?:\\.\\d+){1,3}$");
    if (!pattern.match(left).hasMatch() || !pattern.match(right).hasMatch()) {
        known = false;
        return 0;
    }
    known = true;
    const QStringList a = left.split('.');
    const QStringList b = right.split('.');
    const int count = qMax(a.size(), b.size());
    for (int i = 0; i < count; ++i) {
        const int av = i < a.size() ? a[i].toInt() : 0;
        const int bv = i < b.size() ? b[i].toInt() : 0;
        if (av != bv) return av > bv ? 1 : -1;
    }
    return 0;
}
