#include "core/files.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

#include <windows.h>

namespace Paths {

QString executable() {
    wchar_t buffer[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return QString::fromWCharArray(buffer);
}

QString appDir() {
    if (QCoreApplication::instance()) return QCoreApplication::applicationDirPath();
    return QFileInfo(executable()).absolutePath();
}

QString configFile() {
    return appDir() + "/config.json";
}

QString phrasesFile() {
    return appDir() + "/data.json";
}

QString groupsFile() {
    return appDir() + "/tab.json";
}

QString iconDir() {
    return appDir() + "/icons";
}

QString icon(const QString &fileName) {
    return iconDir() + "/" + fileName;
}

} // namespace Paths

namespace {

void setAsideBroken(const QString &path) {
    QFile::rename(path, QString("%1.broken.%2").arg(path).arg(QDateTime::currentMSecsSinceEpoch()));
}

// 返回 false 表示文件不存在或已经被当成损坏文件挪开
bool readDocument(const QString &path, QJsonDocument &doc, bool wantArray) {
    QFile file(path);
    if (!file.exists()) return false;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) doc = QJsonDocument::fromJson(file.readAll());
    file.close(); // 改名前必须关掉
    if (wantArray ? doc.isArray() : doc.isObject()) return true;
    setAsideBroken(path);
    return false;
}

} // namespace

namespace DataFile {

bool readObject(const QString &path, QJsonObject &out) {
    QJsonDocument doc;
    if (!readDocument(path, doc, false)) return false;
    out = doc.object();
    return true;
}

bool readArray(const QString &path, QJsonArray &out) {
    QJsonDocument doc;
    if (!readDocument(path, doc, true)) return false;
    out = doc.array();
    return true;
}

bool writeAtomically(const QString &path, const QByteArray &data, QString &error, bool textMode) {
    QSaveFile file(path);
    if (!file.open(textMode ? QIODevice::WriteOnly | QIODevice::Text : QIODevice::WriteOnly)) {
        error = file.errorString();
        return false;
    }
    if (file.write(data) != data.size()) {
        error = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

bool writeJson(const QString &path, const QJsonDocument &doc) {
    QString error;
    return writeAtomically(path, doc.toJson(), error, true); // 和以前一样按文本模式写（CRLF）
}

} // namespace DataFile
