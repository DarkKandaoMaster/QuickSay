#pragma once

#include <QJsonObject>
#include <QPoint>
#include <QSize>
#include <QString>

// config.json 里的键名。键名属于磁盘格式（也写进备份文件），为兼容老版本保留原样，不要改。
namespace SettingsKey {
inline constexpr char Hotkey[] = "hotkey";
inline constexpr char AlwaysOnTop[] = "zhiding";
inline constexpr char OutputDelay[] = "delay";
inline constexpr char MainWidth[] = "width";
inline constexpr char MainHeight[] = "height";
inline constexpr char DefaultItemHeight[] = "default_item_height";
inline constexpr char ItemPaddingHorizontal[] = "phrase_item_padding_horizontal";
inline constexpr char ItemPaddingVertical[] = "phrase_item_padding_vertical";
inline constexpr char ScrollStep[] = "gundong";
inline constexpr char GroupPanelDelay[] = "fenzu_yanshi";
inline constexpr char SelectFirstOnGroupSwitch[] = "fenzu_shouxuan_diyitiao";
inline constexpr char GroupPanelAlwaysVisible[] = "fenzu_changxian";
inline constexpr char ScrollSafePixels[] = "anquanqu_xiangsu";
inline constexpr char BadgeTopLeft[] = "jiaobiao";
inline constexpr char BadgeKeyPassthrough[] = "badge_key_passthrough_when_pinned";
inline constexpr char EnterKeyPassthrough[] = "enter_key_passthrough_when_pinned";
inline constexpr char ArrowKeyPassthrough[] = "arrow_key_passthrough_when_pinned";
inline constexpr char BadgeKeyCtrl[] = "badge_key_ctrl_when_pinned";
inline constexpr char EnterKeyCtrl[] = "enter_key_ctrl_when_pinned";
inline constexpr char ArrowKeyCtrl[] = "arrow_key_ctrl_when_pinned";
inline constexpr char AutoStart[] = "ziqidong";
inline constexpr char RunAsAdmin[] = "guanliyuan";
inline constexpr char Pinned[] = "tudingflag";
inline constexpr char CheckUpdatesOnStartup[] = "qidong_jiancha_gengxin";
inline constexpr char IgnoredVersion[] = "hulve_banben";
inline constexpr char SettingsWindowWidth[] = "shezhichuangkou_w";
inline constexpr char SettingsWindowHeight[] = "shezhichuangkou_h";

// 老版本用过的键，只在迁移时读一次
inline constexpr char LegacyAutoStartAsAdmin[] = "ziqidong_guanliyuan";
inline constexpr char LegacyBadgeKeyInputPhrase[] = "badge_key_input_phrase_when_pinned";
inline constexpr char LegacyEnterKeyInputPhrase[] = "enter_key_input_phrase_when_pinned";
inline constexpr char LegacyScrollSafeRows[] = "anquanqu_hangshu";
} // namespace SettingsKey

// 各窗口记住的位置
enum class WindowSlot {
    Main,
    Settings,
    AddPhrase,
    EditPhrase,
};

// 全部设置都放在一个 QJsonObject 里，原样读写 config.json（不认识的键也保留）。
class Settings {
  public:
    // config.json 不存在时写入一份默认设置；存在时补齐老版本缺的字段。需要 QGuiApplication（默认窗口位置要用屏幕尺寸）。
    static void load();
    static void save();
    // 拖窗口、拉尺寸这种一秒几十次的改动用它：停手 500ms 后才写盘。退出、关机前由 flushPendingSave() 补写
    static void saveLater();
    static void flushPendingSave();

    static const QJsonObject &json();

    // 导入备份成功后到旧进程退出之前，所有落盘一律跳过，免得旧的内存状态覆盖刚导入的文件。
    static void freezeWrites();
    static bool writesFrozen();

    // 窗口位置的键名前缀（如 "chuangkou" → chuangkou_x / chuangkou_y）
    static QString windowKeyPrefix(WindowSlot slot);
    static QPoint windowPos(WindowSlot slot);
    static void setWindowPos(WindowSlot slot, const QPoint &pos);

    static QString hotkey();
    static void setHotkey(const QString &value);
    static bool alwaysOnTop();
    static void setAlwaysOnTop(bool value);
    static int outputDelayMs();
    static void setOutputDelayMs(int value);
    static int mainWidth();
    static void setMainWidth(int value);
    static int mainHeight();
    static void setMainHeight(int value);
    static int defaultItemHeight();
    static void setDefaultItemHeight(int value);
    static int itemPaddingHorizontal();
    static void setItemPaddingHorizontal(int value);
    static int itemPaddingVertical();
    static void setItemPaddingVertical(int value);
    static int scrollStep();
    static void setScrollStep(int value);
    static int groupPanelDelayMs(); // 鼠标移开后收起分组面板的延时，同时也是翻组惯性的持续时间
    static void setGroupPanelDelayMs(int value);
    static bool selectFirstOnGroupSwitch();
    static void setSelectFirstOnGroupSwitch(bool value);
    static bool groupPanelAlwaysVisible();
    static void setGroupPanelAlwaysVisible(bool value);
    static int scrollSafePixels();
    static void setScrollSafePixels(int value);
    static bool badgeTopLeft();
    static void setBadgeTopLeft(bool value);
    static bool badgeKeyPassthroughWhenPinned();
    static void setBadgeKeyPassthroughWhenPinned(bool value);
    static bool enterKeyPassthroughWhenPinned();
    static void setEnterKeyPassthroughWhenPinned(bool value);
    static bool arrowKeyPassthroughWhenPinned();
    static void setArrowKeyPassthroughWhenPinned(bool value);
    static bool badgeKeyCtrlWhenPinned();
    static void setBadgeKeyCtrlWhenPinned(bool value);
    static bool enterKeyCtrlWhenPinned();
    static void setEnterKeyCtrlWhenPinned(bool value);
    static bool arrowKeyCtrlWhenPinned();
    static void setArrowKeyCtrlWhenPinned(bool value);
    static bool autoStart();
    static void setAutoStart(bool value);
    static bool runAsAdmin();
    static void setRunAsAdmin(bool value);
    static bool pinned();
    static void setPinned(bool value);
    static bool checkUpdatesOnStartup();
    static void setCheckUpdatesOnStartup(bool value);
    static QString ignoredVersion();
    static void setIgnoredVersion(const QString &value);
    static QSize settingsWindowSize(); // 设置窗口外框尺寸（含标题栏和边框）
    static void setSettingsWindowSize(const QSize &value);

    // main() 开头还没有 QApplication 时，直接从 config.json 里读“以管理员权限启动”。
    static bool readRunAsAdminFromFile();
};
