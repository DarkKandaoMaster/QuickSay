#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>

// 更新内容写在 src/main.cpp 开头，发版时那里的版本号和这里一起改
inline const QString kAppVersion = QStringLiteral("2.1.1");

// 只在备份文件结构发生不兼容变化时才改，不跟着程序版本走。2.1.0 起是 2.0.0（英文设置键名、短语嵌在分组里），
// 1.0.0 的老备份仍能导入，见 backup.cpp
inline const QString kBackupFormatVersion = QStringLiteral("2.0.0");

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
