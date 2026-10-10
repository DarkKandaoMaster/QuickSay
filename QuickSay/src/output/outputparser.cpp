#include "output/outputparser.h"

#include <QFileInfo>
#include <QStringList>

#include <climits>

#include <windows.h>

namespace {

// 复制路径时常被带进来的不可见方向控制字符
QString cleanPath(QString path) {
    const ushort hiddenChars[] = {0x200E, 0x200F, 0x202A, 0x202B, 0x202C, 0x202D, 0x202E, 0x2066, 0x2067, 0x2068, 0x2069};
    for (ushort code : hiddenChars)
        path.remove(QChar(code));
    return path.trimmed();
}

bool virtualKeyFromName(const QString &name, WORD &vk) {
    const QString key = name.trimmed().toLower();
    struct Named {
        const char *name;
        WORD vk;
    };
    static const Named namedKeys[] = {
        {"enter", VK_RETURN},
        {"return", VK_RETURN},
        {"tab", VK_TAB},
        {"space", VK_SPACE},
        {"esc", VK_ESCAPE},
        {"escape", VK_ESCAPE},
        {"backspace", VK_BACK},
        {"left", VK_LEFT},
        {"right", VK_RIGHT},
        {"up", VK_UP},
        {"down", VK_DOWN},
        {"ins", VK_INSERT},
        {"insert", VK_INSERT},
    };
    for (const Named &named : namedKeys) {
        if (key == QLatin1String(named.name)) {
            vk = named.vk;
            return true;
        }
    }
    if (key.size() == 1) {
        const ushort c = key.at(0).toUpper().unicode();
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            vk = static_cast<WORD>(c);
            return true;
        }
    }
    if (key.size() >= 2 && key.at(0) == 'f') {
        bool ok = false;
        const int n = key.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 24) {
            vk = static_cast<WORD>(VK_F1 + n - 1);
            return true;
        }
    }
    return false;
}

// "Ctrl+Shift+A"：修饰键各最多一次、顺序随意，主键最多一个（也可以只有修饰键）
bool parseCombo(const QString &combo, OutputAction &action) {
    const QStringList parts = combo.split('+', Qt::KeepEmptyParts);
    if (parts.isEmpty()) return false;

    bool ctrl = false, alt = false, shift = false, meta = false, hasPrimary = false;
    WORD primary = 0;
    auto takeOnce = [](bool &flag) {
        if (flag) return false;
        flag = true;
        return true;
    };
    for (const QString &rawPart : parts) {
        const QString part = rawPart.trimmed();
        if (part.isEmpty()) return false;
        const QString lower = part.toLower();
        if (lower == "ctrl" || lower == "control") {
            if (!takeOnce(ctrl)) return false;
        } else if (lower == "alt") {
            if (!takeOnce(alt)) return false;
        } else if (lower == "shift") {
            if (!takeOnce(shift)) return false;
        } else if (lower == "meta" || lower == "win" || lower == "windows") {
            if (!takeOnce(meta)) return false;
        } else {
            if (hasPrimary || !virtualKeyFromName(part, primary)) return false;
            hasPrimary = true;
        }
    }
    if (!hasPrimary && !ctrl && !alt && !shift && !meta) return false;

    action.type = OutputAction::Type::Press;
    if (ctrl) action.modifiers.append(VK_LCONTROL);
    if (alt) action.modifiers.append(VK_LMENU);
    if (shift) action.modifiers.append(VK_LSHIFT);
    if (meta) action.modifiers.append(VK_LWIN);
    action.key = primary;
    return true;
}

// "500ms"、"1.5s"、"2"（不带单位按秒）。秒数四舍五入到毫秒。
bool parseSleepMs(const QString &text, int &sleepMs) {
    QString duration = text.trimmed();
    bool milliseconds = false;
    if (duration.endsWith("ms", Qt::CaseInsensitive)) {
        milliseconds = true;
        duration.chop(2);
    } else if (duration.endsWith("s", Qt::CaseInsensitive)) {
        duration.chop(1);
    }
    if (duration.isEmpty()) return false;

    bool ok = false;
    if (milliseconds) {
        const int value = duration.toInt(&ok);
        if (!ok || value < 0) return false;
        sleepMs = value;
        return true;
    }
    const double seconds = duration.toDouble(&ok);
    if (!ok || seconds < 0 || seconds > INT_MAX / 1000.0) return false;
    sleepMs = static_cast<int>(seconds * 1000.0 + 0.5);
    return true;
}

bool startsWithKeyword(const QString &text, const QString &keyword) {
    return text.size() > keyword.size() && text.left(keyword.size()).compare(keyword, Qt::CaseInsensitive) == 0 && text.at(keyword.size()).isSpace();
}

bool parseTag(const QString &tag, OutputAction &action) {
    const QString trimmed = tag.trimmed();
    if (trimmed.isEmpty()) return false;

    const QString lower = trimmed.toLower();
    if (lower == "sleep") {
        action.type = OutputAction::Type::Sleep;
        action.sleepMs = 1000;
        return true;
    }

    const QStringList parts = trimmed.split(' ', Qt::SkipEmptyParts);
    if (parts.size() == 2 && parts.at(0).compare("sleep", Qt::CaseInsensitive) == 0) {
        int sleepMs = 0;
        if (!parseSleepMs(parts.at(1), sleepMs)) return false;
        action.type = OutputAction::Type::Sleep;
        action.sleepMs = sleepMs;
        return true;
    }
    if (parts.size() == 2 && parts.at(0).compare("press", Qt::CaseInsensitive) == 0) return parseCombo(parts.at(1), action);

    const bool isImg = startsWithKeyword(trimmed, "img");
    const bool isFile = startsWithKeyword(trimmed, "file");
    if (isImg || isFile) {
        QString path = cleanPath(trimmed.mid(isImg ? 4 : 5));
        if ((path.startsWith('"') && path.endsWith('"')) || (path.startsWith('\'') && path.endsWith('\''))) path = cleanPath(path.mid(1, path.size() - 2));
        const QFileInfo info(path);
        if (!info.isAbsolute() || !info.exists() || !info.isFile()) return false;
        action.type = OutputAction::Type::File;
        action.filePath = info.absoluteFilePath();
        return true;
    }

    // 只有这些单键名能简写成 <Enter> 的形式，组合键必须写 <press ...>
    static const QStringList shorthandKeys = {
        "enter", "return", "tab", "space", "esc", "escape", "backspace",
        "left", "right", "up", "down", "insert", "ins",
        "ctrl", "control", "shift", "alt", "win", "meta", "windows"};
    if (shorthandKeys.contains(lower)) return parseCombo(trimmed, action);
    return false;
}

void appendText(QVector<OutputAction> &actions, const QString &text) {
    if (text.isEmpty()) return;
    OutputAction action;
    action.type = OutputAction::Type::Text;
    action.text = text;
    actions.append(action);
}

} // namespace

QVector<OutputAction> parseOutputActions(const QString &text) {
    QVector<OutputAction> actions;
    QString buffer;
    for (int i = 0; i < text.size();) {
        if (text.at(i) == '\\' && i + 1 < text.size() && text.at(i + 1) == '<') {
            buffer.append('<');
            i += 2;
            continue;
        }
        if (text.at(i) != '<') {
            buffer.append(text.at(i));
            ++i;
            continue;
        }
        const int close = text.indexOf('>', i + 1);
        if (close < 0) {
            buffer.append('<');
            ++i;
            continue;
        }
        OutputAction action;
        if (!parseTag(text.mid(i + 1, close - i - 1), action)) {
            buffer.append(text.mid(i, close - i + 1));
        } else {
            appendText(actions, buffer);
            buffer.clear();
            actions.append(action);
        }
        i = close + 1;
    }
    appendText(actions, buffer);
    return actions;
}
