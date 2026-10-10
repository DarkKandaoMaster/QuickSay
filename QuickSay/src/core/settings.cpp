#include "core/settings.h"

#include "core/files.h"

#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QScreen>
#include <QTimer>

namespace {

QJsonObject g_values;
bool g_writesFrozen = false;
QTimer *g_saveTimer = nullptr;
const int kSaveLaterDelayMs = 500;

bool boolValue(const char *key, bool fallback) {
    return g_values.value(key).toBool(fallback);
}

int intValue(const char *key, int fallback) {
    return g_values.value(key).toInt(fallback);
}

void setValue(const char *key, const QJsonValue &value) {
    g_values[key] = value;
}

// 老版本缺的字段逐个补上默认值，再把改过名的老字段迁移过来
void migrate(QJsonObject &c) {
    using namespace SettingsKey;
    auto fill = [&c](const char *key, const QJsonValue &value) {
        if (!c.contains(key)) c[key] = value;
    };
    fill(OutputDelay, 200);
    fill(DefaultItemHeight, 40);
    fill(ItemPaddingHorizontal, 16);
    fill(ItemPaddingVertical, 12);
    fill(ScrollStep, 10);
    // 旧开关是“钉住时输入短语”，新开关是“钉住时不输入短语”，含义相反所以要取反；老字段不存在时新开关默认勾选
    fill(BadgeKeyPassthrough, !c[LegacyBadgeKeyInputPhrase].toBool(false));
    fill(EnterKeyPassthrough, !c[LegacyEnterKeyInputPhrase].toBool(false));
    fill(ArrowKeyPassthrough, true);
    fill(BadgeKeyCtrl, false);
    fill(EnterKeyCtrl, false);
    fill(ArrowKeyCtrl, false);
    c.remove(LegacyBadgeKeyInputPhrase);
    c.remove(LegacyEnterKeyInputPhrase);
    // 2.0.0 以前“以管理员权限开机自启”是一个开关，现在拆成了独立的“以管理员权限启动”
    fill(RunAsAdmin, c[LegacyAutoStartAsAdmin].toBool(false));
    fill(SettingsWindowWidth, 620);
    fill(SettingsWindowHeight, 576);
    fill(GroupPanelDelay, 1500);
    fill(SelectFirstOnGroupSwitch, true);
    fill(GroupPanelAlwaysVisible, false);
    // 2.0.2 及以前安全区按行数算，短语项高度一变就跟着变，现在改成固定像素。老默认的 1 行换成新默认，改过的按默认项高折算
    if (!c.contains(ScrollSafePixels) && c.value(LegacyScrollSafeRows).isDouble() && c[LegacyScrollSafeRows].toInt() != 1)
        c[ScrollSafePixels] = c[LegacyScrollSafeRows].toInt() * c[DefaultItemHeight].toInt(40);
    fill(ScrollSafePixels, 60);
    c.remove(LegacyScrollSafeRows);
    fill(CheckUpdatesOnStartup, true);
    fill(IgnoredVersion, "");
    c.remove(LegacyAutoStartAsAdmin);
}

QJsonObject defaults() {
    using namespace SettingsKey;
    const QRect screen = QGuiApplication::primaryScreen()->geometry();
    const int centerX = (screen.width() - 360) / 2;
    const int centerY = (screen.height() - 500) / 2;
    const int besideX = (screen.width() - 500) / 2 + 501; // 其他窗口默认摆在主窗口右边，不和它重叠
    QJsonObject c;
    c[Hotkey] = "Ctrl+Shift+V";
    c[AlwaysOnTop] = true;
    c[OutputDelay] = 200;
    c[MainWidth] = 360;
    c[MainHeight] = 500;
    c[DefaultItemHeight] = 40;
    c[ItemPaddingHorizontal] = 16;
    c[ItemPaddingVertical] = 12;
    c[ScrollStep] = 10;
    c[GroupPanelDelay] = 1500;
    c[SelectFirstOnGroupSwitch] = true;
    c[GroupPanelAlwaysVisible] = false;
    c[ScrollSafePixels] = 60;
    c[BadgeTopLeft] = false;
    c[BadgeKeyPassthrough] = true;
    c[EnterKeyPassthrough] = true;
    c[ArrowKeyPassthrough] = true;
    c[BadgeKeyCtrl] = false; // Ctrl+角标会顶掉前台程序的 Ctrl+C、Ctrl+V 之类，默认不开
    c[EnterKeyCtrl] = false;
    c[ArrowKeyCtrl] = false;
    c[AutoStart] = true;
    c[RunAsAdmin] = false;
    c[Pinned] = true;
    c["chuangkou_x"] = centerX;
    c["chuangkou_y"] = centerY;
    c["shezhichuangkou_x"] = besideX;
    c["shezhichuangkou_y"] = centerY;
    c[SettingsWindowWidth] = 620; // 和 TrafficMonitor 中文设置窗口一致
    c[SettingsWindowHeight] = 576;
    c[CheckUpdatesOnStartup] = true;
    c[IgnoredVersion] = "";
    c["tianjiachuangkou_x"] = besideX;
    c["tianjiachuangkou_y"] = centerY;
    c["xiugaichuangkou_x"] = besideX;
    c["xiugaichuangkou_y"] = centerY;
    return c;
}

} // namespace

void Settings::load() {
    if (DataFile::readObject(Paths::configFile(), g_values)) {
        migrate(g_values);
        return;
    }
    g_values = defaults();
    save();
}

void Settings::save() {
    if (g_saveTimer) g_saveTimer->stop(); // 整份都写了，排着的延迟保存不用再写
    if (g_writesFrozen) return;
    DataFile::writeJson(Paths::configFile(), QJsonDocument(g_values));
}

void Settings::saveLater() {
    if (!g_saveTimer) {
        g_saveTimer = new QTimer(qApp);
        g_saveTimer->setSingleShot(true);
        g_saveTimer->setInterval(kSaveLaterDelayMs);
        QObject::connect(g_saveTimer, &QTimer::timeout, []() {
            save();
        });
    }
    g_saveTimer->start(); // 再次调用会重新计时，拖动期间只在停手后写一次
}

void Settings::flushPendingSave() {
    if (g_saveTimer && g_saveTimer->isActive()) save();
}

const QJsonObject &Settings::json() {
    return g_values;
}

void Settings::freezeWrites() {
    g_writesFrozen = true;
}

bool Settings::writesFrozen() {
    return g_writesFrozen;
}

QString Settings::windowKeyPrefix(WindowSlot slot) {
    switch (slot) {
    case WindowSlot::Main:
        return "chuangkou";
    case WindowSlot::Settings:
        return "shezhichuangkou";
    case WindowSlot::AddPhrase:
        return "tianjiachuangkou";
    case WindowSlot::EditPhrase:
        return "xiugaichuangkou";
    }
    return QString();
}

QPoint Settings::windowPos(WindowSlot slot) {
    const QString prefix = windowKeyPrefix(slot);
    return QPoint(g_values.value(prefix + "_x").toInt(), g_values.value(prefix + "_y").toInt());
}

void Settings::setWindowPos(WindowSlot slot, const QPoint &pos) {
    const QString prefix = windowKeyPrefix(slot);
    g_values[prefix + "_x"] = pos.x();
    g_values[prefix + "_y"] = pos.y();
}

QString Settings::hotkey() {
    return g_values.value(SettingsKey::Hotkey).toString();
}
void Settings::setHotkey(const QString &value) {
    setValue(SettingsKey::Hotkey, value);
}
bool Settings::alwaysOnTop() {
    return boolValue(SettingsKey::AlwaysOnTop, true);
}
void Settings::setAlwaysOnTop(bool value) {
    setValue(SettingsKey::AlwaysOnTop, value);
}
int Settings::outputDelayMs() {
    return intValue(SettingsKey::OutputDelay, 200);
}
void Settings::setOutputDelayMs(int value) {
    setValue(SettingsKey::OutputDelay, value);
}
int Settings::mainWidth() {
    return intValue(SettingsKey::MainWidth, 360);
}
void Settings::setMainWidth(int value) {
    setValue(SettingsKey::MainWidth, value);
}
int Settings::mainHeight() {
    return intValue(SettingsKey::MainHeight, 500);
}
void Settings::setMainHeight(int value) {
    setValue(SettingsKey::MainHeight, value);
}
int Settings::defaultItemHeight() {
    return intValue(SettingsKey::DefaultItemHeight, 40);
}
void Settings::setDefaultItemHeight(int value) {
    setValue(SettingsKey::DefaultItemHeight, value);
}
int Settings::itemPaddingHorizontal() {
    return intValue(SettingsKey::ItemPaddingHorizontal, 16);
}
void Settings::setItemPaddingHorizontal(int value) {
    setValue(SettingsKey::ItemPaddingHorizontal, value);
}
int Settings::itemPaddingVertical() {
    return intValue(SettingsKey::ItemPaddingVertical, 12);
}
void Settings::setItemPaddingVertical(int value) {
    setValue(SettingsKey::ItemPaddingVertical, value);
}
int Settings::scrollStep() {
    return intValue(SettingsKey::ScrollStep, 10);
}
void Settings::setScrollStep(int value) {
    setValue(SettingsKey::ScrollStep, value);
}
int Settings::groupPanelDelayMs() {
    return intValue(SettingsKey::GroupPanelDelay, 1500);
}
void Settings::setGroupPanelDelayMs(int value) {
    setValue(SettingsKey::GroupPanelDelay, value);
}
bool Settings::selectFirstOnGroupSwitch() {
    return boolValue(SettingsKey::SelectFirstOnGroupSwitch, true);
}
void Settings::setSelectFirstOnGroupSwitch(bool value) {
    setValue(SettingsKey::SelectFirstOnGroupSwitch, value);
}
bool Settings::groupPanelAlwaysVisible() {
    return boolValue(SettingsKey::GroupPanelAlwaysVisible, false);
}
void Settings::setGroupPanelAlwaysVisible(bool value) {
    setValue(SettingsKey::GroupPanelAlwaysVisible, value);
}
int Settings::scrollSafePixels() {
    return intValue(SettingsKey::ScrollSafePixels, 60);
}
void Settings::setScrollSafePixels(int value) {
    setValue(SettingsKey::ScrollSafePixels, value);
}
bool Settings::badgeTopLeft() {
    return boolValue(SettingsKey::BadgeTopLeft, false);
}
void Settings::setBadgeTopLeft(bool value) {
    setValue(SettingsKey::BadgeTopLeft, value);
}
bool Settings::badgeKeyPassthroughWhenPinned() {
    return boolValue(SettingsKey::BadgeKeyPassthrough, true);
}
void Settings::setBadgeKeyPassthroughWhenPinned(bool value) {
    setValue(SettingsKey::BadgeKeyPassthrough, value);
}
bool Settings::enterKeyPassthroughWhenPinned() {
    return boolValue(SettingsKey::EnterKeyPassthrough, true);
}
void Settings::setEnterKeyPassthroughWhenPinned(bool value) {
    setValue(SettingsKey::EnterKeyPassthrough, value);
}
bool Settings::arrowKeyPassthroughWhenPinned() {
    return boolValue(SettingsKey::ArrowKeyPassthrough, true);
}
void Settings::setArrowKeyPassthroughWhenPinned(bool value) {
    setValue(SettingsKey::ArrowKeyPassthrough, value);
}
bool Settings::badgeKeyCtrlWhenPinned() {
    return boolValue(SettingsKey::BadgeKeyCtrl, false);
}
void Settings::setBadgeKeyCtrlWhenPinned(bool value) {
    setValue(SettingsKey::BadgeKeyCtrl, value);
}
bool Settings::enterKeyCtrlWhenPinned() {
    return boolValue(SettingsKey::EnterKeyCtrl, false);
}
void Settings::setEnterKeyCtrlWhenPinned(bool value) {
    setValue(SettingsKey::EnterKeyCtrl, value);
}
bool Settings::arrowKeyCtrlWhenPinned() {
    return boolValue(SettingsKey::ArrowKeyCtrl, false);
}
void Settings::setArrowKeyCtrlWhenPinned(bool value) {
    setValue(SettingsKey::ArrowKeyCtrl, value);
}
bool Settings::autoStart() {
    return boolValue(SettingsKey::AutoStart, true);
}
void Settings::setAutoStart(bool value) {
    setValue(SettingsKey::AutoStart, value);
}
bool Settings::runAsAdmin() {
    return boolValue(SettingsKey::RunAsAdmin, false);
}
void Settings::setRunAsAdmin(bool value) {
    setValue(SettingsKey::RunAsAdmin, value);
}
bool Settings::pinned() {
    return boolValue(SettingsKey::Pinned, true);
}
void Settings::setPinned(bool value) {
    setValue(SettingsKey::Pinned, value);
}
bool Settings::checkUpdatesOnStartup() {
    return boolValue(SettingsKey::CheckUpdatesOnStartup, true);
}
void Settings::setCheckUpdatesOnStartup(bool value) {
    setValue(SettingsKey::CheckUpdatesOnStartup, value);
}
QString Settings::ignoredVersion() {
    return g_values.value(SettingsKey::IgnoredVersion).toString();
}
void Settings::setIgnoredVersion(const QString &value) {
    setValue(SettingsKey::IgnoredVersion, value);
}

QSize Settings::settingsWindowSize() {
    return QSize(intValue(SettingsKey::SettingsWindowWidth, 620), intValue(SettingsKey::SettingsWindowHeight, 576));
}

void Settings::setSettingsWindowSize(const QSize &value) {
    setValue(SettingsKey::SettingsWindowWidth, value.width());
    setValue(SettingsKey::SettingsWindowHeight, value.height());
}

bool Settings::readRunAsAdminFromFile() {
    QFile file(Paths::configFile());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false; // 第一次运行，还没有配置文件
    const QJsonObject c = QJsonDocument::fromJson(file.readAll()).object();
    return c[SettingsKey::RunAsAdmin].toBool(c[SettingsKey::LegacyAutoStartAsAdmin].toBool(false));
}
