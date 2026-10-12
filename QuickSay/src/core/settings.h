#pragma once

#include <QJsonObject>
#include <QPoint>
#include <QSize>
#include <QString>

// data/settings.json 里的键名。键名属于磁盘格式（也写进备份文件），改名要在 settings.cpp 的 kRenamedKeys 里加一条迁移。
namespace SettingsKey {
inline constexpr char Hotkey[] = "summon_hotkey";
inline constexpr char AlwaysOnTop[] = "always_on_top";
inline constexpr char OutputDelay[] = "output_delay";
inline constexpr char MainWidth[] = "main_window_width";
inline constexpr char MainHeight[] = "main_window_height";
inline constexpr char DefaultItemHeight[] = "default_item_height";
inline constexpr char ItemPaddingHorizontal[] = "phrase_item_padding_horizontal";
inline constexpr char ItemPaddingVertical[] = "phrase_item_padding_vertical";
inline constexpr char ScrollStep[] = "scroll_step";
inline constexpr char GroupPanelDelay[] = "group_panel_delay";
inline constexpr char SelectFirstOnGroupSwitch[] = "select_first_on_group_switch";
inline constexpr char GroupPanelAlwaysVisible[] = "group_panel_always_visible";
inline constexpr char ScrollSafePixels[] = "scroll_safe_area";
inline constexpr char GroupPanelSafePixels[] = "group_panel_safe_area";
inline constexpr char BadgeTopLeft[] = "badge_top_left";
inline constexpr char BadgeKeyPassthrough[] = "badge_key_passthrough_when_pinned";
inline constexpr char EnterKeyPassthrough[] = "enter_key_passthrough_when_pinned";
inline constexpr char ArrowKeyPassthrough[] = "arrow_key_passthrough_when_pinned";
inline constexpr char BadgeKeyCtrl[] = "badge_key_ctrl_when_pinned";
inline constexpr char EnterKeyCtrl[] = "enter_key_ctrl_when_pinned";
inline constexpr char ArrowKeyCtrl[] = "arrow_key_ctrl_when_pinned";
inline constexpr char AutoStart[] = "auto_start";
inline constexpr char RunAsAdmin[] = "run_as_admin";
inline constexpr char Pinned[] = "pinned";
inline constexpr char CheckUpdatesOnStartup[] = "check_updates_on_startup";
inline constexpr char IgnoredVersion[] = "ignored_version";
inline constexpr char SettingsWindowWidth[] = "settings_window_width";
inline constexpr char SettingsWindowHeight[] = "settings_window_height";
inline constexpr char ToolTipDelay[] = "tooltip_delay";
inline constexpr char CaptureHotkey[] = "capture_hotkey";
inline constexpr char SearchOnSummon[] = "search_on_summon";
inline constexpr char StopOutputOnSwitch[] = "stop_output_on_window_switch";
// 最后写这份文件的 QuickSay 版本号，以后改格式时按它判断是哪一版的数据（core/version.h 的 compareVersions）
inline constexpr char Version[] = "version";
} // namespace SettingsKey

// 各窗口记住的位置
enum class WindowSlot {
    Main,
    Settings,
    AddPhrase,
    EditPhrase,
};

// 全部设置都放在一个 QJsonObject 里，原样读写 data/settings.json（不认识的键也保留）。
class Settings {
  public:
    // settings.json 不存在时写入一份默认设置；存在时补齐老版本缺的字段。需要 QGuiApplication（默认窗口位置要用屏幕尺寸）。
    static void load();
    // 老版本的设置升级成当前格式：改过名的键换成新名字，拆开的、含义反过来的开关迁移过来，缺的字段补默认值（窗口位置除外）。
    // 读 settings.json、搬 2.1.0 以前的 config.json、导入备份都走这里，不需要 QApplication
    static void upgrade(QJsonObject &values);
    static void save();
    // 拖窗口、拉尺寸这种一秒几十次的改动用它：停手 500ms 后才写盘。退出、关机前由 flushPendingSave() 补写
    static void saveLater();
    static void flushPendingSave();

    static const QJsonObject &json();

    // 导入备份成功后到旧进程退出之前，所有落盘一律跳过，免得旧的内存状态覆盖刚导入的文件。
    static void freezeWrites();
    static bool writesFrozen();

    // 窗口位置的键名前缀（如 "main_window" → main_window_x / main_window_y）
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
    static int groupPanelSafePixels(); // 翻组时分组面板上下各留出的距离，和短语列表的分开设
    static void setGroupPanelSafePixels(int value);
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
    static QString captureHotkey(); // “把选中的文字存为新短语”的全局快捷键，空串表示不启用
    static void setCaptureHotkey(const QString &value);
    static bool searchOnSummon(); // 用户呼出主窗口（快捷键、托盘、再次启动）后直接进入搜索
    static void setSearchOnSummon(bool value);
    static bool stopOutputOnWindowSwitch(); // 高级输入过程中前台换成了别的程序就停下
    static void setStopOutputOnWindowSwitch(bool value);
    static int toolTipDelayMs(); // 鼠标停在短语项、分组上多久后显示悬停提示
    static void setToolTipDelayMs(int value);

    // main() 开头还没有 QApplication 时，直接从 settings.json 里读“以管理员权限启动”。老文件此前已由 DataFile::migrateLegacyFiles() 搬好
    static bool readRunAsAdminFromFile();
};
