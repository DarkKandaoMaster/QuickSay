#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class QListWidget;
class GroupModel;

// 单文件备份：可读的文件头 + Base64 正文 + SHA-256 校验。
// 只收录设置、分组和短语本身；<img>/<file> 后面的路径只是正文里的普通字符，外部文件不会进备份。
namespace Backup {

// parse 出来的一律是当前格式，老格式的备份在 parse 里就升级好了
struct Data {
    QJsonObject settings; // settings.json
    QJsonArray groups; // phrases.json 里的 "groups"，短语嵌在分组里
    QString appVersion; // 导出时的 QuickSay 版本
    QDateTime exportedAtUtc;
};

QByteArray create(const QListWidget &phrases, const GroupModel &groups);

// 完整校验文件头、校验和、JSON 结构和各字段；只读，不碰现有数据
bool parse(const QByteArray &fileData, Data &backup, QString &error);

// 仍在任意屏幕上的窗口坐标保持原样，整个落到屏幕外的移到主屏幕中央
void moveWindowsOntoScreens(QJsonObject &settings);

// 两份新文件全部写好后再替换；任一步失败就把已经动过的文件全部回滚
bool replaceDataFiles(const Data &backup, QString &error);

} // namespace Backup
