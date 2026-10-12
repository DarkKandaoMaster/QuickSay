#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// 图标在 exe 同目录的 icons/ 下，数据文件在 data/ 下。
namespace Paths {

// exe 的完整路径（反斜杠）。不依赖 QCoreApplication，提权建任务的那个实例和 main() 开头都能用。
QString executable();

QString appDir();
QString dataDir();
QString settingsFile();
QString phrasesFile();
QString iconDir();
QString icon(const QString &fileName);

} // namespace Paths

// data/ 下那两份 JSON 数据文件的读写。
namespace DataFile {

// 2.1.0 以前数据文件直接放在 exe 旁边：config.json（设置）、data.json（短语）、tab.json（分组）。
// 搬进 data/ 换成新格式后删掉老文件；data/ 里已经有新文件的就以新文件为准，老文件直接删（上次搬到一半被打断）。
// 新文件没写成功就不删老文件。在 main() 开头、读任何设置之前调用，不需要 QApplication
void migrateLegacyFiles();

// 返回 false 时调用方应当写入默认内容：文件不存在，或者内容不是期望的 JSON 类型。
// 后一种情况先把原文件改名成 <文件名>.broken.<毫秒> 留底，免得接下来的保存把用户数据覆盖成默认值。
bool readObject(const QString &path, QJsonObject &out);
bool readArray(const QString &path, QJsonArray &out);
// 改名成 <文件名>.broken.<毫秒>：内容是合法 JSON、但结构不对的文件也照此留底
void setAsideBroken(const QString &path);

// 先写临时文件再原子替换，中途失败（断电、磁盘满）不会留下半截文件。所在目录不存在时先建出来
bool writeAtomically(const QString &path, const QByteArray &data, QString &error, bool textMode = false);
bool writeJson(const QString &path, const QJsonDocument &doc);

} // namespace DataFile
