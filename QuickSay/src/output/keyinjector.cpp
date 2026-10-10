#include "output/keyinjector.h"

#include "output/outputparser.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QVector>

#include <memory>

#include <windows.h>

namespace {

// 'QSAY'。SendInput 是异步的，钩子收到这些按键时 SendInput 早就返回了，所以只能靠按键自己带的标记认
const ULONG_PTR kInjectionTag = 0x51534159;

// 和 DROPFILES 布局一致，MinGW 头文件里的定义不总是可用
struct DropFiles {
    DWORD pFiles;
    POINT pt;
    BOOL fNC;
    BOOL fWide;
};

bool isExtendedKey(WORD vk) {
    switch (vk) {
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_INSERT:
    case VK_DELETE:
    case VK_LWIN:
    case VK_RWIN:
        return true;
    default:
        return false;
    }
}

INPUT keyInput(WORD vk, bool keyUp, bool markExtended) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwExtraInfo = kInjectionTag;
    if (keyUp) input.ki.dwFlags |= KEYEVENTF_KEYUP;
    if (markExtended && isExtendedKey(vk)) input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    return input;
}

bool send(QVector<INPUT> &inputs) {
    if (inputs.isEmpty()) return true;
    return SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT)) == static_cast<UINT>(inputs.size());
}

bool sendCtrlPlus(WORD key) {
    QVector<INPUT> inputs = {keyInput(VK_LCONTROL, false, false), keyInput(key, false, false), keyInput(key, true, false), keyInput(VK_LCONTROL, true, false)};
    if (send(inputs)) return true;
    // 失败后的尽力清理；调用方随即停止，不做猜测性重试
    QVector<INPUT> releases = {keyInput(key, true, false), keyInput(VK_LCONTROL, true, false)};
    send(releases);
    return false;
}

} // namespace

namespace KeyInjector {

bool sendCtrlV() {
    return sendCtrlPlus('V');
}

bool sendCtrlC() {
    return sendCtrlPlus('C');
}

bool sendCombo(const OutputAction &action) {
    // 按下和抬起放进同一次 SendInput，既减少中途留下按下状态的窗口，也能用返回值判断是否完整发送
    QVector<INPUT> inputs;
    for (WORD modifier : action.modifiers)
        inputs.append(keyInput(modifier, false, true));
    if (action.key != 0) {
        inputs.append(keyInput(action.key, false, true));
        inputs.append(keyInput(action.key, true, true));
    }
    for (int i = action.modifiers.size() - 1; i >= 0; --i)
        inputs.append(keyInput(action.modifiers.at(i), true, true));
    if (send(inputs)) return true;

    QVector<INPUT> releases;
    if (action.key != 0) releases.append(keyInput(action.key, true, true));
    for (int i = action.modifiers.size() - 1; i >= 0; --i)
        releases.append(keyInput(action.modifiers.at(i), true, true));
    send(releases);
    return false;
}

bool isAltOrWinPressed() {
    return (GetAsyncKeyState(VK_MENU) & 0x8000) || (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000);
}

bool releasePressedModifiers() {
    static const WORD modifiers[] = {VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LSHIFT, VK_RSHIFT, VK_LWIN, VK_RWIN};
    bool pressed[8] = {};
    for (int i = 0; i < 8; ++i)
        pressed[i] = (GetAsyncKeyState(modifiers[i]) & 0x8000) != 0; // 先整体拍快照，再逐个抬起
    for (int i = 0; i < 8; ++i) {
        if (!pressed[i]) continue;
        INPUT input = keyInput(modifiers[i], true, true);
        if (SendInput(1, &input, sizeof(INPUT)) != 1) return false;
    }
    return true;
}

void sendMenuMaskKey() {
    // 带着自家标记，快捷键钩子直接放行，不会绕回来
    INPUT inputs[2] = {keyInput(0xE8, false, false), keyInput(0xE8, true, false)};
    SendInput(2, inputs, sizeof(INPUT));
}

bool setClipboardFile(const QString &filePath) {
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) return false;

    const QString nativePath = QDir::toNativeSeparators(info.absoluteFilePath());
    const int pathBytes = (nativePath.size() + 2) * sizeof(wchar_t); // 末尾双 \0
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DropFiles) + pathBytes);
    if (!memory) return false;
    BYTE *data = static_cast<BYTE *>(GlobalLock(memory));
    if (!data) {
        GlobalFree(memory);
        return false;
    }
    auto *dropFiles = reinterpret_cast<DropFiles *>(data);
    dropFiles->pFiles = sizeof(DropFiles);
    dropFiles->fWide = TRUE;
    memcpy(data + sizeof(DropFiles), nativePath.utf16(), pathBytes - sizeof(wchar_t));
    GlobalUnlock(memory);

    if (!OpenClipboard(nullptr)) {
        GlobalFree(memory);
        return false;
    }
    if (!EmptyClipboard()) {
        CloseClipboard();
        GlobalFree(memory);
        return false;
    }
    const bool ok = SetClipboardData(CF_HDROP, memory) != nullptr;
    const bool closed = CloseClipboard() != FALSE;
    if (!ok) GlobalFree(memory); // 成功后内存归系统所有，CloseClipboard 失败也不能再释放
    return ok && closed;
}

bool isOwnInjection(quint32 flags, quintptr extraInfo) {
    return (flags & LLKHF_INJECTED) && extraInfo == kInjectionTag;
}

} // namespace KeyInjector

namespace {

const int kPollIntervalMs = 20;
const int kTimeoutMs = 1000; // 前台程序没选中任何东西时 Ctrl+C 什么也不做，剪贴板不会变，等这么久就放弃

bool g_running = false;

} // namespace

namespace SelectionCapture {

void start(std::function<void(const QString &text)> onFinished) {
    if (g_running) return;
    g_running = true;

    // 用户这会儿多半还按着快捷键里的修饰键，不先抬起，发出去的 Ctrl+C 会变成 Ctrl+Alt+C 之类
    if (KeyInjector::isAltOrWinPressed()) KeyInjector::sendMenuMaskKey();
    // 先记下序号再发 Ctrl+C：只认这之后剪贴板发生的变化，免得把复制之前就在剪贴板里的旧内容当成选区
    const DWORD sequence = GetClipboardSequenceNumber();
    if (!KeyInjector::releasePressedModifiers() || !KeyInjector::sendCtrlC()) {
        g_running = false;
        onFinished(QString());
        return;
    }

    auto *timer = new QTimer(qApp);
    timer->setInterval(kPollIntervalMs);
    auto elapsed = std::make_shared<int>(0);
    auto changedSeen = std::make_shared<bool>(false);
    QObject::connect(timer, &QTimer::timeout, timer, [timer, sequence, elapsed, changedSeen, onFinished]() {
        *elapsed += kPollIntervalMs;
        const bool changed = GetClipboardSequenceNumber() != sequence;
        // 序号一变就读的话，有的程序分几次往剪贴板里写格式，可能刚好读到写了一半的内容，所以变了之后再多等一轮
        if (changed && !*changedSeen) {
            *changedSeen = true;
            return;
        }
        if (!changed && *elapsed < kTimeoutMs) return;
        timer->stop();
        timer->deleteLater();
        g_running = false;
        onFinished(changed ? QApplication::clipboard()->text() : QString());
    });
    timer->start();
}

} // namespace SelectionCapture
