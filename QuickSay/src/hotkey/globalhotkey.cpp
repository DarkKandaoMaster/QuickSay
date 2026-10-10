#include "hotkey/globalhotkey.h"

#include "core/phrase.h"
#include "output/keyinjector.h"
#include "output/outputrunner.h"

#include <QHash>
#include <QHotkey>
#include <QListWidget>
#include <QVector>

#include <windows.h>

namespace {

enum class Route {
    QHotkeyAsIs,
    QHotkeyKeypadVk,
    Hook,
};

HHOOK g_hook = nullptr;
QVector<GlobalHotkey *> g_hookHotkeys; // 已注册、走钩子那条路的快捷键

// 已经被钩子当成快捷键吞掉、还没松开的键：键是 VK | (扩展位 << 16)，值是最后一次收到按下的时间。
// 按住不放时系统会不停发自动重复的按下消息，靠它认出来，不让快捷键连发。
QHash<quint32, DWORD> g_swallowedKeys;

// 距离上次按下超过这么久还没收到松开，就当成残留（比如按着快捷键时锁屏，松开消息丢了），按新按下处理。
// 必须比系统“重复延迟”最长的 1000ms 还长，否则按住不放会被误判成新按下而连发。
const DWORD kStaleKeyMs = 1500;

// 判断 sequence 该走哪条路；走钩子或小键盘 VK 时顺便算出 VK 码和钩子要核对的扩展位
Route routeFor(const QKeySequence &sequence, DWORD &vk, bool &extended) {
    vk = 0;
    extended = false;
    if (sequence.isEmpty()) return Route::QHotkeyAsIs; // 交给 QHotkey，它自己会注册失败
    const Qt::Key key = sequence[0].key(); // 和 QHotkey 一样只看第一个组合键
    const bool keypad = sequence[0].keyboardModifiers() & Qt::KeypadModifier;

    // 方向键这一族不管带不带 Num 都走钩子：否则主键盘←用 RegisterHotKey(VK_LEFT) 注册，小键盘←也会被它吞掉
    switch (key) {
    case Qt::Key_Left:
        vk = VK_LEFT;
        break;
    case Qt::Key_Right:
        vk = VK_RIGHT;
        break;
    case Qt::Key_Up:
        vk = VK_UP;
        break;
    case Qt::Key_Down:
        vk = VK_DOWN;
        break;
    case Qt::Key_Home:
        vk = VK_HOME;
        break;
    case Qt::Key_End:
        vk = VK_END;
        break;
    case Qt::Key_PageUp:
        vk = VK_PRIOR;
        break;
    case Qt::Key_PageDown:
        vk = VK_NEXT;
        break;
    case Qt::Key_Insert:
        vk = VK_INSERT;
        break;
    case Qt::Key_Delete:
        vk = VK_DELETE;
        break;
    case Qt::Key_Return: // 主键盘回车，没有扩展位
        vk = VK_RETURN;
        return Route::Hook;
    case Qt::Key_Enter: // 小键盘回车，带扩展位。Qt 只在扩展位为 1 时才产生 Key_Enter，所以不管带不带 Num 都当小键盘回车
        vk = VK_RETURN;
        extended = true;
        return Route::Hook;
    default:
        break;
    }
    if (vk != 0) {
        extended = !keypad; // 主键盘那一小块方向键区是扩展键；NumLock 关着时的小键盘方向键不是
        return Route::Hook;
    }
    if (!keypad) return Route::QHotkeyAsIs;

    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        vk = VK_NUMPAD0 + (key - Qt::Key_0);
        return Route::QHotkeyKeypadVk;
    }
    switch (key) {
    case Qt::Key_Asterisk:
        vk = VK_MULTIPLY;
        break;
    case Qt::Key_Plus:
        vk = VK_ADD;
        break;
    case Qt::Key_Minus:
        vk = VK_SUBTRACT;
        break;
    case Qt::Key_Slash:
        vk = VK_DIVIDE;
        break;
    case Qt::Key_Period:
    case Qt::Key_Comma:
        vk = VK_DECIMAL;
        break; // 有些布局下小键盘的点是逗号
    case Qt::Key_Clear:
        vk = VK_CLEAR;
        break; // NumLock 关着时的小键盘 5
    default:
        return Route::QHotkeyAsIs; // 其他带 Num 的键（比如 NumLock 本身）原样交给 QHotkey
    }
    return Route::QHotkeyKeypadVk;
}

bool isKeyDown(int vk) {
    return GetAsyncKeyState(vk) & 0x8000;
}

LRESULT CALLBACK hookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION) return CallNextHookEx(g_hook, code, wParam, lParam);
    const auto *info = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
    // 自家输出短语时模拟的方向键、回车不能反过来触发快捷键。别人注入的键照常认，和走 RegisterHotKey 的快捷键一致
    if (KeyInjector::isOwnInjection(info->flags, info->dwExtraInfo)) return CallNextHookEx(g_hook, code, wParam, lParam);

    const bool extended = info->flags & LLKHF_EXTENDED;
    const quint32 keyId = info->vkCode | (extended ? 0x10000u : 0u); // 主键盘回车和小键盘回车分开记
    if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
        g_swallowedKeys.remove(keyId); // 松开消息照常放行，RegisterHotKey 也不吞松开
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }
    if ((wParam != WM_KEYDOWN && wParam != WM_SYSKEYDOWN) || g_hookHotkeys.isEmpty()) return CallNextHookEx(g_hook, code, wParam, lParam);

    // 低级钩子里读到的是这次按键之前的状态，修饰键早已按下，读得准
    const bool ctrl = isKeyDown(VK_CONTROL);
    const bool shift = isKeyDown(VK_SHIFT);
    const bool alt = isKeyDown(VK_MENU);
    const bool meta = isKeyDown(VK_LWIN) || isKeyDown(VK_RWIN);

    // 不能用 GetAsyncKeyState(vk) 判断是不是还按着：被钩子吞掉的按下不会写进键状态表，读出来永远是“没按着”。
    // 所以看时间：自动重复每隔几十毫秒来一次，隔很久才又来的按下只能是松开消息丢了之后的新按下。
    bool autoRepeat = false;
    const auto swallowed = g_swallowedKeys.constFind(keyId);
    if (swallowed != g_swallowedKeys.constEnd()) {
        if (info->time - swallowed.value() <= kStaleKeyMs) autoRepeat = true; // 无符号相减，计数器绕回 0 也算得对
        else g_swallowedKeys.remove(keyId);
    }

    bool matched = false;
    for (GlobalHotkey *hotkey : std::as_const(g_hookHotkeys)) {
        if (!hotkey->matches(info->vkCode, extended, ctrl, shift, alt, meta)) continue;
        matched = true;
        // 钩子回调必须马上返回，真正的活丢回事件循环；hotkey 作上下文对象，它先被删了 Qt 会丢掉这次调用
        if (!autoRepeat) QMetaObject::invokeMethod(hotkey, [hotkey]() {
            emit hotkey->activated();
        },
            Qt::QueuedConnection);
    }
    if (!matched) return CallNextHookEx(g_hook, code, wParam, lParam);

    if (!autoRepeat && (alt || meta)) KeyInjector::sendMenuMaskKey();
    g_swallowedKeys.insert(keyId, info->time);
    return 1; // 吞掉，前台程序收不到（自动重复也一起吞）
}

} // namespace

GlobalHotkey::GlobalHotkey(const QKeySequence &sequence, bool autoRegister, QObject *parent) : QObject(parent) {
    setShortcut(sequence, autoRegister);
}

GlobalHotkey::~GlobalHotkey() {
    if (usesHook_) g_hookHotkeys.removeAll(this); // 否则钩子会拿着野指针去比对
}

QKeySequence GlobalHotkey::shortcut() const {
    return sequence_;
}

bool GlobalHotkey::setRegistered(bool registered) {
    if (qhotkey_) return qhotkey_->setRegistered(registered);
    if (!usesHook_) return false;
    if (!registered) g_hookHotkeys.removeAll(this);
    else if (!g_hookHotkeys.contains(this)) g_hookHotkeys.append(this);
    return true; // 钩子这条路不会和别的程序冲突
}

bool GlobalHotkey::setShortcut(const QKeySequence &sequence, bool autoRegister) {
    setRegistered(false);
    delete qhotkey_;
    qhotkey_ = nullptr;
    usesHook_ = false;
    sequence_ = sequence;

    DWORD vk = 0;
    bool extended = false;
    const Route route = routeFor(sequence, vk, extended);
    const Qt::KeyboardModifiers modifiers = sequence.isEmpty() ? Qt::KeyboardModifiers() : sequence[0].keyboardModifiers();
    if (route == Route::Hook) {
        usesHook_ = true;
        vk_ = vk;
        extended_ = extended;
        ctrl_ = modifiers & Qt::ControlModifier;
        shift_ = modifiers & Qt::ShiftModifier;
        alt_ = modifiers & Qt::AltModifier;
        meta_ = modifiers & Qt::MetaModifier;
        return autoRegister ? setRegistered(true) : true;
    }

    if (route == Route::QHotkeyKeypadVk) {
        quint32 nativeModifiers = 0;
        if (modifiers & Qt::ControlModifier) nativeModifiers |= MOD_CONTROL;
        if (modifiers & Qt::ShiftModifier) nativeModifiers |= MOD_SHIFT;
        if (modifiers & Qt::AltModifier) nativeModifiers |= MOD_ALT;
        if (modifiers & Qt::MetaModifier) nativeModifiers |= MOD_WIN;
        // 不要加 MOD_NOREPEAT：qhotkey_win.cpp 注册时自己会“+ MOD_NOREPEAT”，再加一次就错了
        qhotkey_ = new QHotkey(QHotkey::NativeShortcut(vk, nativeModifiers), autoRegister, this);
    } else {
        qhotkey_ = new QHotkey(sequence, autoRegister, this);
    }
    connect(qhotkey_, &QHotkey::activated, this, &GlobalHotkey::activated);
    return autoRegister ? qhotkey_->isRegistered() : true;
}

bool GlobalHotkey::matches(quint32 vk, bool extended, bool ctrl, bool shift, bool alt, bool meta) const {
    return usesHook_ && vk == vk_ && extended == extended_ && ctrl == ctrl_ && shift == shift_ && alt == alt_ && meta == meta_;
}

bool GlobalHotkey::matchesSystemHotkey(quint32 vk, bool ctrl, bool shift, bool alt, bool meta) const {
    if (!qhotkey_ || !qhotkey_->isRegistered()) return false;
    const QHotkey::NativeShortcut native = qhotkey_->currentNativeShortcut(); // modifier 里不含 MOD_NOREPEAT，注册时才加
    if (!native.isValid() || native.key != vk) return false;
    return bool(native.modifier & MOD_CONTROL) == ctrl && bool(native.modifier & MOD_SHIFT) == shift && bool(native.modifier & MOD_ALT) == alt && bool(native.modifier & MOD_WIN) == meta;
}

namespace HotkeyHook {

void install() {
    if (!g_hook) g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, hookProc, GetModuleHandleW(nullptr), 0);
}

void reinstall() {
    if (g_hook) {
        UnhookWindowsHookEx(g_hook); // 已经被系统摘掉的话这里会失败，无所谓
        g_hook = nullptr;
    }
    install();
}

} // namespace HotkeyHook

void HotkeyRegistry::setShowHotkey(GlobalHotkey *hotkey) {
    showHotkey_ = hotkey;
}

GlobalHotkey *HotkeyRegistry::showHotkey() const {
    return showHotkey_;
}

void HotkeyRegistry::rebuildPhraseHotkeys(QListWidget &list) {
    for (GlobalHotkey *hotkey : std::as_const(phraseHotkeys_)) {
        if (!hotkey) continue;
        hotkey->setRegistered(false);
        delete hotkey;
    }
    phraseHotkeys_.clear();

    for (int i = 0; i < list.count(); ++i) {
        QListWidgetItem *item = list.item(i);
        const QString sequence = PhraseItem::hotkey(item);
        if (sequence.isEmpty()) {
            phraseHotkeys_.append(nullptr);
            continue;
        }
        auto *hotkey = new GlobalHotkey(QKeySequence(sequence), true, this);
        // 按下时才读正文，所以修改正文后不重建也能输出新内容
        connect(hotkey, &GlobalHotkey::activated, hotkey, [item]() {
            OutputRunner::start(PhraseItem::text(item));
        });
        phraseHotkeys_.append(hotkey);
    }
}

GlobalHotkey *HotkeyRegistry::phraseHotkeyAt(int row) const {
    return row >= 0 && row < phraseHotkeys_.size() ? phraseHotkeys_[row] : nullptr;
}

void HotkeyRegistry::setPhraseHotkeysRegistered(bool registered) {
    for (GlobalHotkey *hotkey : std::as_const(phraseHotkeys_)) {
        if (hotkey) hotkey->setRegistered(registered);
    }
}

bool HotkeyRegistry::isUsedByPhrase(const QKeySequence &sequence, const GlobalHotkey *except) const {
    for (const GlobalHotkey *hotkey : phraseHotkeys_) {
        if (hotkey && hotkey != except && hotkey->shortcut() == sequence) return true;
    }
    return false;
}

bool HotkeyRegistry::isSystemHotkey(quint32 vk, bool ctrl, bool shift, bool alt, bool meta) const {
    if (showHotkey_ && showHotkey_->matchesSystemHotkey(vk, ctrl, shift, alt, meta)) return true;
    for (const GlobalHotkey *hotkey : phraseHotkeys_) {
        if (hotkey && hotkey->matchesSystemHotkey(vk, ctrl, shift, alt, meta)) return true;
    }
    return false;
}
