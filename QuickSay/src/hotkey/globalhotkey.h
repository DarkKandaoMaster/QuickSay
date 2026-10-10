#pragma once

#include <QKeySequence>
#include <QObject>
#include <QVector>

class QHotkey;

// 全局快捷键。按键位分三条路注册：
//   普通键：原样交给 QHotkey（底层 RegisterHotKey）
//   小键盘数字/运算键：QHotkey 会把它们映射成主键盘的 VK，所以换成小键盘专用 VK 后再交给 QHotkey
//   方向键、Home/End/PgUp/PgDn/Ins/Del、回车：主键盘和小键盘 VK 相同、只差扩展位，RegisterHotKey 分不清，
//     改由低级键盘钩子自己核对扩展位（见 HotkeyHook）
// 快捷键字符串沿用 QKeySequence 的 PortableText，小键盘上的键带 "Num"（如 Ctrl+Num+4、Num+Left）。
class GlobalHotkey : public QObject {
    Q_OBJECT

  public:
    GlobalHotkey(const QKeySequence &sequence, bool autoRegister, QObject *parent = nullptr);
    ~GlobalHotkey() override;

    // 用户录入的原样快捷键（查重靠它；NativeShortcut 构造的 QHotkey 返回的是空序列，不能转发）
    QKeySequence shortcut() const;
    bool setRegistered(bool registered);
    bool setShortcut(const QKeySequence &sequence, bool autoRegister);

    // 钩子用：VK、扩展位和四个修饰键全部精确相等才算命中（RegisterHotKey 也是精确匹配）
    bool matches(quint32 vk, bool extended, bool ctrl, bool shift, bool alt, bool meta) const;
    // 键盘浏览钩子用：走 RegisterHotKey 且已注册成功、VK 和四个修饰键全部相等
    bool matchesSystemHotkey(quint32 vk, bool ctrl, bool shift, bool alt, bool meta) const;

  signals:
    void activated();

  private:
    QKeySequence sequence_;
    QHotkey *qhotkey_ = nullptr; // 走 RegisterHotKey 时才有
    bool usesHook_ = false;
    quint32 vk_ = 0;
    bool extended_ = false;
    bool ctrl_ = false;
    bool shift_ = false;
    bool alt_ = false;
    bool meta_ = false;
};

// 用户快捷键专用的低级键盘钩子，只认走钩子那条路的快捷键。和主窗口键盘浏览的钩子是两回事。
namespace HotkeyHook {

// 程序启动时装一次，必须先于键盘浏览钩子：低级钩子后装的先执行，浏览钩子排在前面，
// 主窗口显示时方向键、回车才会先归键盘浏览处理。
void install();

// 回调超时（主线程被 UAC 等待、COM 之类卡住）时 Windows 会悄悄摘掉钩子，句柄看着还在，所以定期卸掉重装。
// 重装后它排到了浏览钩子前面，调用方要紧接着重装浏览钩子。
void reinstall();

} // namespace HotkeyHook

class GlobalHotkey;
class QKeySequence;
class QListWidget;

// 呼出主窗口的全局快捷键 + 每条短语自己的快捷键。
// 短语快捷键和列表行号一一对应（没设快捷键的行是 nullptr），所以任何增删改、拖动排序之后都要 rebuild。
class HotkeyRegistry : public QObject {
  public:
    using QObject::QObject;

    void setShowHotkey(GlobalHotkey *hotkey);
    GlobalHotkey *showHotkey() const;

    void rebuildPhraseHotkeys(QListWidget &list);
    GlobalHotkey *phraseHotkeyAt(int row) const;
    void setPhraseHotkeysRegistered(bool registered);

    // 有没有别的短语已经用了这个快捷键（不查呼出快捷键）
    bool isUsedByPhrase(const QKeySequence &sequence, const GlobalHotkey *except) const;

    // 这个按键组合是不是某个走 RegisterHotKey 的全局快捷键（呼出快捷键或短语快捷键）
    bool isSystemHotkey(quint32 vk, bool ctrl, bool shift, bool alt, bool meta) const;

  private:
    GlobalHotkey *showHotkey_ = nullptr;
    QVector<GlobalHotkey *> phraseHotkeys_;
};
