#include "core/backup.h"

#include "core/files.h"
#include "core/groupmodel.h"
#include "core/phrase.h"
#include "core/settings.h"
#include "core/version.h"

#include <QCryptographicHash>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QScreen>
#include <QSet>
#include <QVector>
#include <QtMath>

#include <climits>

#include <windows.h>

namespace {

const char *const kBeginMarker = "------------------- QUICKSAY BACKUP BEGIN -------------------";
const char *const kEndMarker = "-------------------- QUICKSAY BACKUP END --------------------";
const char *const kHeaderSeparator = "----------------------------------------------------------------";
const char *const kExportedAtPrefix = "- EXPORTED AT:       ";
const char *const kFormatVersionPrefix = "- FORMAT VERSION:    ";
const char *const kAppVersionPrefix = "- QUICKSAY VERSION:  ";
const char *const kChecksumPrefix = "- SHA256:            ";

// 老版本导出的备份缺字段时按新版默认值补齐，改过名的字段按相反含义迁移
void upgradeSettings(QJsonObject &settings) {
    using namespace SettingsKey;
    if (!settings.contains(BadgeKeyPassthrough) && settings.value(LegacyBadgeKeyInputPhrase).isBool()) settings[BadgeKeyPassthrough] = !settings[LegacyBadgeKeyInputPhrase].toBool();
    if (!settings.contains(EnterKeyPassthrough) && settings.value(LegacyEnterKeyInputPhrase).isBool()) settings[EnterKeyPassthrough] = !settings[LegacyEnterKeyInputPhrase].toBool();
    settings.remove(LegacyBadgeKeyInputPhrase);
    settings.remove(LegacyEnterKeyInputPhrase);
    auto fill = [&settings](const char *key, const QJsonValue &value) {
        if (!settings.contains(key)) settings[key] = value;
    };
    fill(ArrowKeyPassthrough, true);
    fill(BadgeKeyCtrl, false);
    fill(EnterKeyCtrl, false);
    fill(ArrowKeyCtrl, false);
    fill(SelectFirstOnGroupSwitch, true);
    fill(GroupPanelAlwaysVisible, false);
    fill(GroupPanelDelay, 1500);
    if (!settings.contains(ScrollSafePixels) && settings.value(LegacyScrollSafeRows).isDouble() && settings[LegacyScrollSafeRows].toInt() != 1)
        settings[ScrollSafePixels] = settings[LegacyScrollSafeRows].toInt() * settings[DefaultItemHeight].toInt(40);
    fill(ScrollSafePixels, 60);
    settings.remove(LegacyScrollSafeRows);
    fill(GroupPanelSafePixels, 50);
    fill(ToolTipDelay, 0);
}

bool validateSettings(const QJsonObject &settings, QString &error) {
    using namespace SettingsKey;
    auto integerInRange = [&settings](const char *key, int minimum, int maximum) {
        const QJsonValue value = settings.value(key);
        if (!value.isDouble()) return false;
        const double number = value.toDouble();
        return qIsFinite(number) && number == qFloor(number) && number >= minimum && number <= maximum;
    };

    const char *boolKeys[] = {AlwaysOnTop, BadgeTopLeft, BadgeKeyPassthrough, EnterKeyPassthrough, ArrowKeyPassthrough, BadgeKeyCtrl, EnterKeyCtrl, ArrowKeyCtrl, AutoStart, RunAsAdmin, Pinned, GroupPanelAlwaysVisible, SelectFirstOnGroupSwitch};
    for (const char *key : boolKeys) {
        if (!settings.value(key).isBool()) {
            error = QString("设置字段 %1 无效").arg(key);
            return false;
        }
    }

    const QJsonValue hotkey = settings.value(Hotkey);
    if (!hotkey.isString() || hotkey.toString().isEmpty() || hotkey.toString().size() > 128 ||
        !integerInRange(MainWidth, 250, 2000) || !integerInRange(MainHeight, 250, 2000) ||
        !integerInRange(DefaultItemHeight, 10, 100) || !integerInRange(ItemPaddingHorizontal, 0, 100) ||
        !integerInRange(ItemPaddingVertical, 0, 100) || !integerInRange(ScrollStep, 1, 100) || !integerInRange(OutputDelay, 0, 2000) ||
        !integerInRange(GroupPanelDelay, 0, 5000) || !integerInRange(ScrollSafePixels, 0, 1000) ||
        !integerInRange(GroupPanelSafePixels, 0, 1000) ||
        !integerInRange(ToolTipDelay, 0, 5000)) {
        error = "备份中的基础设置无效";
        return false;
    }

    for (WindowSlot slot : {WindowSlot::Main, WindowSlot::Settings, WindowSlot::AddPhrase, WindowSlot::EditPhrase}) {
        const QString prefix = Settings::windowKeyPrefix(slot);
        for (const QString &key : {prefix + "_x", prefix + "_y"}) {
            if (!integerInRange(key.toLatin1().constData(), INT_MIN, INT_MAX)) {
                error = QString("窗口坐标字段 %1 无效").arg(key);
                return false;
            }
        }
    }
    return true;
}

bool validateGroups(const QJsonArray &groups, QSet<QString> &names, QString &error) {
    if (groups.isEmpty() || groups.size() > 10000) {
        error = "备份中的分组数量无效";
        return false;
    }
    for (const QJsonValue &value : groups) {
        if (!value.isObject()) {
            error = "备份中存在无效分组";
            return false;
        }
        const QJsonObject group = value.toObject();
        const QString name = group.value("tab").toString();
        const double itemHeight = group.value("item_height").toDouble(-1);
        const double columns = group.value("columns").toDouble(-1);
        if (!group.value("tab").isString() || name.isEmpty() || names.contains(name) ||
            !group.value("item_height").isDouble() || itemHeight != qFloor(itemHeight) || itemHeight < 10 || itemHeight > 100 ||
            !group.value("columns").isDouble() || columns != qFloor(columns) || columns < 1 || columns > 10) {
            error = "备份中的分组名称或分组设置无效";
            return false;
        }
        names.insert(name);
    }
    return true;
}

bool validatePhrases(const QJsonArray &phrases, const QSet<QString> &groupNames, QString &error) {
    if (phrases.size() > 100000) {
        error = "备份中的短语数量过多";
        return false;
    }
    for (const QJsonValue &value : phrases) {
        if (!value.isObject()) {
            error = "备份中存在无效短语";
            return false;
        }
        const QJsonObject phrase = value.toObject();
        if (!phrase.value("text").isString() || !phrase.value("remark").isString() || !phrase.value("tab").isString() || !phrase.value("hotkey").isString() ||
            !groupNames.contains(phrase.value("tab").toString()) || phrase.value("hotkey").toString().size() > 128) {
            error = "备份中的短语字段或所属分组无效";
            return false;
        }
    }
    return true;
}

} // namespace

namespace Backup {

QByteArray create(const QListWidget &phrases, const GroupModel &groups) {
    // 元数据里固定记 UTC 时间，显示给用户时再转本地时间
    const QString exportedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QJsonObject root;
    root["format_version"] = kBackupFormatVersion;
    root["quicksay_version"] = kAppVersion;
    root["exported_at"] = exportedAt;
    root["settings"] = Settings::json();
    root["groups"] = groups.toJson();
    root["phrases"] = PhraseStore::toJson(phrases);

    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    const QByteArray checksum = QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex();
    const QByteArray base64 = payload.toBase64();
    QByteArray wrapped;
    for (int i = 0; i < base64.size(); i += 64) {
        wrapped += base64.mid(i, 64);
        wrapped += '\n';
    }
    if (!wrapped.isEmpty()) wrapped.chop(1);

    QByteArray result;
    result += QByteArray(kBeginMarker) + "\n";
    result += kExportedAtPrefix + exportedAt.toUtf8() + "\n";
    result += kFormatVersionPrefix + kBackupFormatVersion.toUtf8() + "\n";
    result += kAppVersionPrefix + kAppVersion.toUtf8() + "\n";
    result += kChecksumPrefix + checksum + "\n";
    result += QByteArray(kHeaderSeparator) + "\n";
    result += wrapped + "\n";
    result += kEndMarker;
    return result;
}

bool parse(const QByteArray &fileData, Data &backup, QString &error) {
    if (fileData.isEmpty() || fileData.size() > 64 * 1024 * 1024) {
        error = "文件为空或超过64 MB";
        return false;
    }
    for (char ch : fileData) {
        if (static_cast<unsigned char>(ch) > 127) {
            error = "文件包含无效字符"; // 合法文件只有 ASCII 文件头和 Base64 正文
            return false;
        }
    }

    QString text = QString::fromLatin1(fileData);
    text.replace("\r\n", "\n");
    if (text.contains('\r')) {
        error = "文件换行格式无效";
        return false;
    }
    QStringList lines = text.split('\n');
    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast(); // 允许编辑器在末尾补换行
    if (lines.size() < 8 || lines.first() != kBeginMarker || lines.last() != kEndMarker) {
        error = "文件头或结束标记无效";
        return false;
    }
    if (lines.value(5) != kHeaderSeparator) {
        error = "文件头分隔线无效";
        return false;
    }
    auto headerValue = [&lines](int index, const QString &prefix, QString &value) {
        const QString line = lines.value(index);
        if (!line.startsWith(prefix)) return false;
        value = line.mid(prefix.size());
        return !value.isEmpty();
    };
    QString exportedAtText, formatVersion, appVersion, checksumText;
    if (!headerValue(1, kExportedAtPrefix, exportedAtText) ||
        !headerValue(2, kFormatVersionPrefix, formatVersion) ||
        !headerValue(3, kAppVersionPrefix, appVersion) ||
        !headerValue(4, kChecksumPrefix, checksumText)) {
        error = "文件头元数据无效";
        return false;
    }
    if (formatVersion != kBackupFormatVersion) {
        error = QString("不支持的备份格式版本：%1").arg(formatVersion);
        return false;
    }
    if (!QRegularExpression("^[A-Za-z0-9._+\\-]{1,40}$").match(appVersion).hasMatch() ||
        !QRegularExpression("^[0-9a-f]{64}$").match(checksumText).hasMatch()) {
        error = "版本号或完整性校验值无效";
        return false;
    }
    const QDateTime exportedAt = QDateTime::fromString(exportedAtText, Qt::ISODateWithMs);
    if (!exportedAt.isValid() || !exportedAtText.endsWith('Z')) {
        error = "UTC导出时间无效";
        return false;
    }

    QString base64Text;
    const QRegularExpression base64Line("^[A-Za-z0-9+/]+={0,2}$");
    for (int i = 6; i < lines.size() - 1; ++i) {
        if (lines[i].isEmpty() || lines[i].size() > 64 || !base64Line.match(lines[i]).hasMatch()) {
            error = "备份正文格式无效";
            return false;
        }
        base64Text += lines[i];
    }
    if (base64Text.isEmpty() || base64Text.size() % 4 != 0) {
        error = "备份正文长度无效";
        return false;
    }
    const QByteArray payload = QByteArray::fromBase64(base64Text.toLatin1());
    if (payload.toBase64() != base64Text.toLatin1()) {
        error = "备份正文Base64编码无效";
        return false;
    }
    if (QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex()) != checksumText) {
        error = "文件完整性校验失败，文件可能已损坏";
        return false;
    }

    QJsonParseError jsonError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !document.isObject()) {
        error = "备份正文不是有效的JSON对象";
        return false;
    }
    const QJsonObject root = document.object();
    if (root.value("format_version").toString() != formatVersion || root.value("quicksay_version").toString() != appVersion || root.value("exported_at").toString() != exportedAtText ||
        !root.value("settings").isObject() || !root.value("groups").isArray() || !root.value("phrases").isArray()) {
        error = "文件头与备份正文不一致，或未找到完整数据";
        return false;
    }

    QJsonObject settings = root.value("settings").toObject();
    upgradeSettings(settings);
    if (!validateSettings(settings, error)) return false;

    const QJsonArray groups = root.value("groups").toArray();
    QSet<QString> groupNames;
    if (!validateGroups(groups, groupNames, error)) return false;

    const QJsonArray phrases = root.value("phrases").toArray();
    if (!validatePhrases(phrases, groupNames, error)) return false;

    backup.settings = settings;
    backup.groups = groups;
    backup.phrases = phrases;
    backup.appVersion = appVersion;
    backup.exportedAtUtc = exportedAt.toUTC();
    return true;
}

void moveWindowsOntoScreens(QJsonObject &settings) {
    QScreen *primary = QGuiApplication::primaryScreen();
    if (!primary) return;
    const int mainWidth = settings.value(SettingsKey::MainWidth).toInt();
    const int mainHeight = settings.value(SettingsKey::MainHeight).toInt();
    for (WindowSlot slot : {WindowSlot::Main, WindowSlot::Settings, WindowSlot::AddPhrase, WindowSlot::EditPhrase}) {
        const QString prefix = Settings::windowKeyPrefix(slot);
        const QString xKey = prefix + "_x";
        const QString yKey = prefix + "_y";
        // 只有主窗口用设置里的宽高，其他窗口按 500*500 判断
        const int width = slot == WindowSlot::Main ? mainWidth : 500;
        const int height = slot == WindowSlot::Main ? mainHeight : 500;
        const QRect rect(settings.value(xKey).toInt(), settings.value(yKey).toInt(), width, height);
        bool onAnyScreen = false;
        for (QScreen *screen : QGuiApplication::screens()) {
            if (screen && screen->availableGeometry().intersects(rect)) {
                onAnyScreen = true;
                break;
            }
        }
        if (onAnyScreen) continue;
        const QRect primaryRect = primary->availableGeometry();
        settings[xKey] = primaryRect.x() + (primaryRect.width() - width) / 2;
        settings[yKey] = primaryRect.y() + (primaryRect.height() - height) / 2;
    }
}

bool replaceDataFiles(const Data &backup, QString &error) {
    struct FileChange {
        QString path;
        QString stagedPath;
        QString oldPath;
        QByteArray newData;
        bool existed = false;
        bool oldRenamed = false;
        bool installed = false;
    };
    const QString suffix = QString(".%1.%2").arg(GetCurrentProcessId()).arg(QDateTime::currentMSecsSinceEpoch());
    auto change = [&suffix](const QString &path, const QByteArray &data) {
        FileChange file;
        file.path = path;
        file.stagedPath = path + ".import-new" + suffix;
        file.oldPath = path + ".import-old" + suffix;
        file.newData = data;
        return file;
    };
    QVector<FileChange> files = {
        change(Paths::configFile(), QJsonDocument(backup.settings).toJson()),
        change(Paths::phrasesFile(), QJsonDocument(backup.phrases).toJson()),
        change(Paths::groupsFile(), QJsonDocument(backup.groups).toJson()),
    };
    auto cleanupStaged = [&files]() {
        for (const FileChange &file : files)
            QFile::remove(file.stagedPath);
    };
    auto rollback = [&]() {
        bool ok = true;
        for (const FileChange &file : files) {
            if (file.installed && QFile::exists(file.path) && !QFile::remove(file.path)) ok = false;
        }
        for (const FileChange &file : files) {
            if (file.oldRenamed && !QFile::rename(file.oldPath, file.path)) ok = false;
        }
        cleanupStaged();
        return ok;
    };

    for (FileChange &file : files) {
        file.existed = QFile::exists(file.path);
        QString writeError;
        if (!DataFile::writeAtomically(file.stagedPath, file.newData, writeError)) {
            cleanupStaged();
            error = QString("无法准备新数据：%1").arg(writeError);
            return false;
        }
    }
    for (FileChange &file : files) {
        if (!file.existed) continue;
        if (!QFile::rename(file.path, file.oldPath)) {
            error = rollback() ? "无法暂存现有数据，现有数据未改变" : "无法暂存现有数据，而且回滚失败，请不要关闭QuickSay并立即检查数据文件";
            return false;
        }
        file.oldRenamed = true;
    }
    for (FileChange &file : files) {
        if (!QFile::rename(file.stagedPath, file.path)) {
            error = rollback() ? "无法安装新数据，现有数据已恢复" : "无法安装新数据，而且回滚失败，请不要关闭QuickSay并立即检查数据文件";
            return false;
        }
        file.installed = true;
    }
    for (const FileChange &file : files) {
        if (file.oldRenamed) QFile::remove(file.oldPath);
    }
    return true;
}

} // namespace Backup
