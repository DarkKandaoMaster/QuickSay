#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// 所有数据文件和图标都放在 exe 同目录下。
namespace Paths {

// exe 的完整路径（反斜杠）。不依赖 QCoreApplication，提权建任务的那个实例和 main() 开头都能用。
QString executable();

QString appDir();
QString configFile();
QString phrasesFile();
QString groupsFile();
QString iconDir();
QString icon(const QString &fileName);

} // namespace Paths

// exe 旁边那三份 JSON 数据文件的读写。
namespace DataFile {

// 返回 false 时调用方应当写入默认内容：文件不存在，或者内容不是期望的 JSON 类型。
// 后一种情况先把原文件改名成 <文件名>.broken.<毫秒> 留底，免得接下来的保存把用户数据覆盖成默认值。
bool readObject(const QString &path, QJsonObject &out);
bool readArray(const QString &path, QJsonArray &out);

// 先写临时文件再原子替换，中途失败（断电、磁盘满）不会留下半截文件
bool writeAtomically(const QString &path, const QByteArray &data, QString &error, bool textMode = false);
bool writeJson(const QString &path, const QJsonDocument &doc);

} // namespace DataFile
