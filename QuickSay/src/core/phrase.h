#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <Qt>

class QListWidgetItem;

// 短语直接存在 QListWidgetItem 的这几个数据角色里，列表本身就是数据模型。
// 改动任何一个角色都要同步 phrase.cpp 里 PhraseStore 的读写。
namespace PhraseRole {
enum : int {
    Text = Qt::UserRole, // 短语正文         phrases.json: text
    Remark, // 备注，非空时代替正文显示 phrases.json: remark
    Group, // 所属分组名        phrases.json: 短语所在分组的 group
    Hotkey, // 短语快捷键        phrases.json: hotkey
    Badge, // 角标字符，运行期由 PhraseList::applyFilter 算出，不落盘
};
}

namespace PhraseItem {

QListWidgetItem *create(const QString &text, const QString &remark, const QString &group, const QString &hotkey);

QString text(const QListWidgetItem *item);
QString remark(const QListWidgetItem *item);
QString group(const QListWidgetItem *item);
QString hotkey(const QListWidgetItem *item);

void setText(QListWidgetItem *item, const QString &value);
void setRemark(QListWidgetItem *item, const QString &value);
void setGroup(QListWidgetItem *item, const QString &value);
void setHotkey(QListWidgetItem *item, const QString &value);

// 按正文/备注刷新显示文字、颜色和悬停提示
void refreshDisplay(QListWidgetItem *item);

} // namespace PhraseItem

class QListWidget;
class GroupModel;

// data/phrases.json 的读写，分组和短语存在一起：
// {"groups": [{"group": 分组名, "item_height": 行高, "columns": 列数, "phrases": [{"text", "remark", "hotkey"}, ...]}, ...]}
// 磁盘上短语嵌在分组里，内存里摊平成一个列表，每条短语用 PhraseRole::Group 记分组名。
namespace PhraseStore {

// 文件不存在时写入默认的三个分组和新手教程那几条短语；一个分组都没有时补一个，界面才转得起来
void load(QListWidget &list, GroupModel &groups);
void save(const QListWidget &list, const GroupModel &groups);

// 上面 "groups" 数组的内容，备份也用它
QJsonArray toJson(const QListWidget &list, const GroupModel &groups);
// 整份 data/phrases.json：groups 加上写文件的 QuickSay 版本号（"version"），以后改格式时按它判断是哪一版的数据
QJsonObject fileContent(const QJsonArray &groups);

// 2.1.0 以前分组（tab.json）和短语（data.json）分开存，短语用 "tab" 字段记分组名。合并成 toJson 的格式：
// 分组表里没有的分组名按出现顺序补成新分组，一条短语都不丢
QJsonArray fromLegacy(const QJsonArray &groups, const QJsonArray &phrases, int defaultItemHeight);

} // namespace PhraseStore

// 批量编辑短语用的纯文本处理：拼成一个文本文件、再切回来、和原来的短语按正文对上。不碰界面，方便测试。
namespace PhraseBatch {

// 从 5 个 - 起，只要有哪条短语里某一行正好就是它，就再加一个 -
QString chooseSeparator(const QStringList &texts);
QString join(const QStringList &texts, const QString &separator);
// 整行正好等于分隔线才切；空段和只有空白的段丢掉
QStringList split(const QString &text, const QString &separator);
// 批量添加用：每个非空行一条，行首行尾的空白原样保留；只有空白的行丢掉
QStringList splitLines(const QString &text);
// 每条新正文对应的旧下标，-1 是新增；正文相同的按出现顺序一对一认领，没被认领的旧短语就是删掉或改过正文的
QVector<int> matchByText(const QStringList &oldTexts, const QStringList &newTexts);

} // namespace PhraseBatch
