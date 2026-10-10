#include "core/phrase.h"

#include "core/files.h"
#include "core/settings.h"

#include <QColor>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSet>
#include <QStringList>

namespace {

// 悬停提示按每行约 50 个字符折行，超过 maxLines 行时最后一行换成省略号
QString clampTooltipText(const QString &text, int maxLines) {
    const int maxCharsPerLine = 50;
    QStringList lines;
    for (const QString &raw : text.split('\n')) {
        if (raw.length() <= maxCharsPerLine) {
            lines << raw;
            continue;
        }
        for (int i = 0; i < raw.length(); i += maxCharsPerLine)
            lines << raw.mid(i, maxCharsPerLine);
    }
    if (lines.size() > maxLines) {
        lines = lines.mid(0, maxLines - 1);
        lines << "……";
    }
    return lines.join('\n');
}

} // namespace

namespace PhraseItem {

QListWidgetItem *create(const QString &text, const QString &remark, const QString &group, const QString &hotkey) {
    auto *item = new QListWidgetItem();
    item->setData(PhraseRole::Text, text);
    item->setData(PhraseRole::Remark, remark);
    item->setData(PhraseRole::Group, group);
    item->setData(PhraseRole::Hotkey, hotkey);
    refreshDisplay(item);
    return item;
}

QString text(const QListWidgetItem *item) {
    return item->data(PhraseRole::Text).toString();
}
QString remark(const QListWidgetItem *item) {
    return item->data(PhraseRole::Remark).toString();
}
QString group(const QListWidgetItem *item) {
    return item->data(PhraseRole::Group).toString();
}
QString hotkey(const QListWidgetItem *item) {
    return item->data(PhraseRole::Hotkey).toString();
}

void setText(QListWidgetItem *item, const QString &value) {
    item->setData(PhraseRole::Text, value);
}
void setRemark(QListWidgetItem *item, const QString &value) {
    item->setData(PhraseRole::Remark, value);
}
void setGroup(QListWidgetItem *item, const QString &value) {
    item->setData(PhraseRole::Group, value);
}
void setHotkey(QListWidgetItem *item, const QString &value) {
    item->setData(PhraseRole::Hotkey, value);
}

void refreshDisplay(QListWidgetItem *item) {
    const QString body = text(item);
    const QString note = remark(item);
    if (note.isEmpty()) {
        item->setToolTip(clampTooltipText(body, 30));
        item->setText(body);
        item->setForeground(QColor("#323130"));
        return;
    }
    // 有备注时正文最多 20 行，分隔线占 1 行，给备注留 7 行，总高仍控制在 30 行内
    item->setToolTip(clampTooltipText(body, 20) + "\n────────────────────────\n" + clampTooltipText(note, 7));
    item->setText(note);
    item->setForeground(QColor("#2E7D32"));
}

} // namespace PhraseItem

namespace {

const char *const kDefaultGroup = "短语1";

} // namespace

namespace PhraseStore {

void load(QListWidget &list) {
    QJsonArray phrases;
    if (DataFile::readArray(Paths::phrasesFile(), phrases)) {
        list.clear();
        for (const QJsonValue &value : std::as_const(phrases)) {
            if (!value.isObject()) continue;
            const QJsonObject obj = value.toObject();
            list.addItem(PhraseItem::create(obj["text"].toString(), obj["remark"].toString(), obj["tab"].toString(), obj["hotkey"].toString()));
        }
        return;
    }

    list.addItem(PhraseItem::create("快捷键：按下快捷键（默认Ctrl+Shift+V）呼出QuickSay\n添加短语：右上角加号\n修改/删除：右键短语\n排序：拖动短语", "", kDefaultGroup, ""));
    list.addItem(PhraseItem::create("点击短语：输入对应短语\n上下方向键↑↓：移动光标\n回车键Enter：输入光标处短语", "", kDefaultGroup, ""));
    list.addItem(PhraseItem::create("右键左上角分组：新建/修改/删除分组\n分组排序：拖动分组\n左右方向键←→：切换分组\n鼠标滚轮：也可以切换分组", "", kDefaultGroup, ""));
    list.addItem(PhraseItem::create("感谢使用QuickSay！\n如果觉得好用记得点个Star！\n让我们开始吧！把这些短语都删掉，然后新建一个短语~", "", kDefaultGroup, ""));
    save(list);
}

void save(QListWidget &list) {
    if (Settings::writesFrozen()) return;
    // 兼容很老的版本：那时正文只存在显示文字里，也没有分组
    for (int i = 0; i < list.count(); ++i) {
        QListWidgetItem *item = list.item(i);
        if (PhraseItem::text(item).isEmpty()) PhraseItem::setText(item, item->text());
        if (PhraseItem::group(item).isEmpty()) PhraseItem::setGroup(item, kDefaultGroup);
    }
    DataFile::writeJson(Paths::phrasesFile(), QJsonDocument(toJson(list)));
}

QJsonArray toJson(const QListWidget &list) {
    QJsonArray phrases;
    for (int i = 0; i < list.count(); ++i) {
        const QListWidgetItem *item = list.item(i);
        QJsonObject phrase;
        phrase["text"] = PhraseItem::text(item);
        phrase["remark"] = PhraseItem::remark(item);
        phrase["tab"] = PhraseItem::group(item);
        phrase["hotkey"] = PhraseItem::hotkey(item);
        phrases.append(phrase);
    }
    return phrases;
}

} // namespace PhraseStore

namespace PhraseBatch {

QString chooseSeparator(const QStringList &texts) {
    QSet<int> taken;
    for (const QString &text : texts) {
        for (const QString &line : text.split('\n')) {
            if (line.size() >= 5 && line.count('-') == line.size()) taken.insert(line.size());
        }
    }
    int length = 5;
    while (taken.contains(length))
        ++length;
    return QString(length, '-');
}

QString join(const QStringList &texts, const QString &separator) {
    return texts.join("\n" + separator + "\n");
}

QStringList split(const QString &text, const QString &separator) {
    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    QStringList texts;
    QStringList lines;
    const auto flush = [&]() {
        const QString segment = lines.join('\n');
        if (!segment.trimmed().isEmpty()) texts.append(segment);
        lines.clear();
    };
    for (const QString &line : normalized.split('\n')) {
        if (line == separator) flush();
        else lines.append(line);
    }
    flush();
    return texts;
}

QStringList splitLines(const QString &text) {
    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    QStringList texts;
    for (const QString &line : normalized.split('\n')) {
        if (!line.trimmed().isEmpty()) texts.append(line);
    }
    return texts;
}

QVector<int> matchByText(const QStringList &oldTexts, const QStringList &newTexts) {
    QHash<QString, QList<int>> unclaimed;
    for (int i = 0; i < oldTexts.size(); ++i)
        unclaimed[oldTexts[i]].append(i);
    QVector<int> match;
    match.reserve(newTexts.size());
    for (const QString &text : newTexts) {
        auto it = unclaimed.find(text);
        match.append(it == unclaimed.end() || it->isEmpty() ? -1 : it->takeFirst());
    }
    return match;
}

} // namespace PhraseBatch
