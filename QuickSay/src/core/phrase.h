#pragma once

#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <Qt>

class QListWidgetItem;

// 短语直接存在 QListWidgetItem 的这几个数据角色里，列表本身就是数据模型。
// 改动任何一个角色都要同步 phrasestore.cpp 里的读写。
namespace PhraseRole {
enum : int {
    Text = Qt::UserRole, // 短语正文         data.json: text
    Remark, // 备注，非空时代替正文显示 data.json: remark
    Group, // 所属分组名        data.json: tab
    Hotkey, // 短语快捷键        data.json: hotkey
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

// data.json 的读写。短语本身存在 QListWidget 的条目里，见 phraseitem.h。
namespace PhraseStore {

// data.json 不存在时写入新手教程那几条默认短语
void load(QListWidget &list);
void save(QListWidget &list);

// 只读导出，供备份使用
QJsonArray toJson(const QListWidget &list);

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
