#pragma once

#include <QString>
#include <QVector>

// 高级输入：短语正文里的 <Enter>、<press Ctrl+C>、<sleep 1.5s>、<img 路径> 这类标签。
// 解析失败的标签当普通文字原样输出；\< 转义成字面的 <。
struct OutputAction {
    enum class Type {
        Text, // 经剪贴板粘贴的一段文字
        Press, // 模拟按键/组合键
        Sleep,
        File, // <img>/<file>：以 CF_HDROP 放进剪贴板再粘贴
    };

    Type type = Type::Text;
    QString text;
    QVector<unsigned short> modifiers; // 虚拟键码
    unsigned short key = 0;
    int sleepMs = 0;
    QString filePath;
};

QVector<OutputAction> parseOutputActions(const QString &text);
