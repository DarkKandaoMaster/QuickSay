#include "core/phrase.h"

#include "core/files.h"
#include "core/groupmodel.h"
#include "core/settings.h"
#include "core/version.h"

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

void load(QListWidget &list, GroupModel &groups) {
    const QString path = Paths::phrasesFile();
    QJsonObject root;
    if (DataFile::readObject(path, root)) {
        if (root.value("groups").isArray()) {
            list.clear();
            QVector<Group> loaded;
            QSet<QString> names;
            for (const QJsonValue &groupValue : root.value("groups").toArray()) {
                if (!groupValue.isObject()) continue;
                const QJsonObject group = groupValue.toObject();
                const QString name = group.value("group").toString();
                // 手改出来的同名分组只留第一个，短语都归到它名下
                if (!names.contains(name)) {
                    names.insert(name);
                    const int itemHeight = group.value("item_height").toInt();
                    loaded.append({name, itemHeight > 0 ? itemHeight : Settings::defaultItemHeight(), qMax(1, group.value("columns").toInt(1))});
                }
                for (const QJsonValue &phraseValue : group.value("phrases").toArray()) {
                    if (!phraseValue.isObject()) continue;
                    const QJsonObject phrase = phraseValue.toObject();
                    list.addItem(PhraseItem::create(phrase.value("text").toString(), phrase.value("remark").toString(), name, phrase.value("hotkey").toString()));
                }
            }
            if (loaded.isEmpty()) loaded.append({kDefaultGroup, Settings::defaultItemHeight(), 1});
            groups.reset(loaded);
            return;
        }
        DataFile::setAsideBroken(path); // 是 JSON 对象却没有分组数组，多半是手改坏了，同样留底
    }

    const int defaultHeight = Settings::defaultItemHeight();
    groups.reset({{kDefaultGroup, defaultHeight, 1}, {"此处可右键点击", defaultHeight, 1}});
    list.clear();
    list.addItem(PhraseItem::create("点一下短语，就会输入到光标处~", "", kDefaultGroup, ""));
    save(list, groups);
}

void save(const QListWidget &list, const GroupModel &groups) {
    if (Settings::writesFrozen()) return;
    DataFile::writeJson(Paths::phrasesFile(), QJsonDocument(fileContent(toJson(list, groups))));
}

QJsonObject fileContent(const QJsonArray &groups) {
    QJsonObject root;
    root["version"] = kAppVersion;
    root["groups"] = groups;
    return root;
}

QJsonArray toJson(const QListWidget &list, const GroupModel &groups) {
    QStringList names;
    for (int i = 0; i < groups.count(); ++i)
        names.append(groups.name(i));
    QVector<QJsonArray> phrases(names.size());
    for (int i = 0; i < list.count(); ++i) {
        const QListWidgetItem *item = list.item(i);
        // 按理每条短语的分组都在分组表里；万一不在（哪条代码路径漏改了分组名），宁可多出一个分组也不丢短语
        int index = names.indexOf(PhraseItem::group(item));
        if (index < 0) {
            names.append(PhraseItem::group(item));
            phrases.append(QJsonArray());
            index = names.size() - 1;
        }
        QJsonObject phrase;
        phrase["text"] = PhraseItem::text(item);
        phrase["remark"] = PhraseItem::remark(item);
        phrase["hotkey"] = PhraseItem::hotkey(item);
        phrases[index].append(phrase);
    }
    QJsonArray result;
    for (int i = 0; i < names.size(); ++i) {
        QJsonObject group;
        group["group"] = names[i];
        group["item_height"] = groups.itemHeight(i); // 补出来的分组下标越界，取默认行高、1 列
        group["columns"] = groups.columns(i);
        group["phrases"] = phrases[i];
        result.append(group);
    }
    return result;
}

QJsonArray fromLegacy(const QJsonArray &groups, const QJsonArray &phrases, int defaultItemHeight) {
    QStringList names;
    QVector<QJsonObject> merged;
    QVector<QJsonArray> mergedPhrases;
    auto groupIndex = [&](const QString &name) {
        int index = names.indexOf(name);
        if (index >= 0) return index;
        names.append(name);
        QJsonObject group;
        group["group"] = name;
        group["item_height"] = defaultItemHeight;
        group["columns"] = 1;
        merged.append(group);
        mergedPhrases.append(QJsonArray());
        return int(names.size()) - 1;
    };
    for (const QJsonValue &value : groups) {
        if (!value.isObject()) continue;
        const QJsonObject old = value.toObject();
        if (names.contains(old.value("tab").toString())) continue;
        QJsonObject &group = merged[groupIndex(old.value("tab").toString())];
        if (old.value("item_height").toInt() > 0) group["item_height"] = old.value("item_height").toInt();
        group["columns"] = qMax(1, old.value("columns").toInt(1)); // 很老的版本没有 columns 字段
    }
    for (const QJsonValue &value : phrases) {
        if (!value.isObject()) continue;
        const QJsonObject old = value.toObject();
        const QString name = old.value("tab").toString();
        QJsonObject phrase;
        phrase["text"] = old.value("text").toString();
        phrase["remark"] = old.value("remark").toString();
        phrase["hotkey"] = old.value("hotkey").toString();
        mergedPhrases[groupIndex(name.isEmpty() ? kDefaultGroup : name)].append(phrase); // 很老的版本没有分组
    }
    QJsonArray result;
    for (int i = 0; i < merged.size(); ++i) {
        merged[i]["phrases"] = mergedPhrases[i];
        result.append(merged[i]);
    }
    return result;
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
