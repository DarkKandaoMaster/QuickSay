#include "core/files.h"

#include "core/phrase.h"
#include "core/settings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
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

QString dataDir() {
    return appDir() + "/data";
}

QString settingsFile() {
    return dataDir() + "/settings.json";
}

QString phrasesFile() {
    return dataDir() + "/phrases.json";
}

QString icon(const QString &fileName) {
    return ":/icons/" + fileName;
}

} // namespace Paths

namespace {

// 返回 false 表示文件不存在或已经被当成损坏文件挪开
bool readDocument(const QString &path, QJsonDocument &doc, bool wantArray) {
    QFile file(path);
    if (!file.exists()) return false;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) doc = QJsonDocument::fromJson(file.readAll());
    file.close(); // 改名前必须关掉
    if (wantArray ? doc.isArray() : doc.isObject()) return true;
    DataFile::setAsideBroken(path);
    return false;
}

} // namespace

namespace DataFile {

void migrateLegacyFiles() {
    const QString oldSettings = Paths::appDir() + "/config.json";
    const QString oldPhrases = Paths::appDir() + "/data.json";
    const QString oldGroups = Paths::appDir() + "/tab.json";
    if (!QFile::exists(oldSettings) && !QFile::exists(oldPhrases) && !QFile::exists(oldGroups)) return;

    QJsonObject settings;
    if (!QFile::exists(Paths::settingsFile()) && readObject(oldSettings, settings)) {
        Settings::upgrade(settings);
        if (!writeJson(Paths::settingsFile(), QJsonDocument(settings))) return;
    }
    if (!QFile::exists(Paths::phrasesFile())) {
        QJsonArray groups, phrases;
        const bool hasGroups = readArray(oldGroups, groups);
        const bool hasPhrases = readArray(oldPhrases, phrases);
        // 两个都没读到（不存在或已当成损坏文件挪开）就不写，留给 PhraseStore::load 写默认内容
        if (hasGroups || hasPhrases) {
            const QJsonArray merged = PhraseStore::fromLegacy(groups, phrases, settings.value(SettingsKey::DefaultItemHeight).toInt(40));
            if (!writeJson(Paths::phrasesFile(), QJsonDocument(PhraseStore::fileContent(merged)))) return;
        }
    }
    QFile::remove(oldSettings);
    QFile::remove(oldPhrases);
    QFile::remove(oldGroups);
}

void setAsideBroken(const QString &path) {
    QFile::rename(path, QString("%1.broken.%2").arg(path).arg(QDateTime::currentMSecsSinceEpoch()));
}

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
    QDir().mkpath(QFileInfo(path).absolutePath());
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
