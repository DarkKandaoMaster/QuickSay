#include "hotkey/hotkeyinput.h"

#include "hotkey/globalhotkey.h"

#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QMessageBox>
#include <QStringList>

namespace {

// Qt 6.5 的 QKeySequenceEdit 只保留 Ctrl/Alt/Meta（Shift 另算），把 KeypadModifier 丢了，
// 小键盘按键录进去就和主键盘分不开，所以带 KeypadModifier 的按键由这里自己拼好塞进输入框。
// 返回 true 表示已经处理，过滤器要吞掉这次按键。
bool recordKeypadKey(QKeySequenceEdit *edit, QKeyEvent *event) {
    const int key = event->key();
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta || key == Qt::Key_AltGr || key == Qt::Key_unknown || key == Qt::Key_NumLock) return false;
    if (!(event->modifiers() & Qt::KeypadModifier)) return false;
    Qt::KeyboardModifiers modifiers = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier | Qt::KeypadModifier);
    // 小键盘回车录成 "Ctrl+Enter" 而不是 "Ctrl+Num+Enter"：Key_Enter 只可能来自小键盘回车，
    // 带不带 Num 是同一个键，两种写法并存会让按字符串查重失效
    if (key == Qt::Key_Enter) modifiers &= ~Qt::KeypadModifier;
    edit->setKeySequence(QKeySequence(QKeyCombination(modifiers, Qt::Key(key))));
    return true;
}

} // namespace

bool validateHotkey(const QKeySequence &sequence, const HotkeyRegistry &registry, QWidget *parent, const GlobalHotkey *except) {
    const QString title = "快捷键不合规";
    if (sequence.isEmpty()) {
        QMessageBox::warning(parent, title, "快捷键不能为空  ");
        return false;
    }
    if (sequence.count() != 1) {
        QMessageBox::warning(parent, title, "快捷键不能为多个快捷键的组合  ");
        return false;
    }

    const QString text = sequence.toString();
    const bool hasModifier = text.contains("Ctrl") || text.contains("Alt") || text.contains("Shift") || text.contains("Meta");
    const QString last = text.split('+').last();
    const bool hasPrimary = last != "Ctrl" && last != "Alt" && last != "Shift" && last != "Meta";
    // "Num+Ins" 是 NumLock 关着时小键盘 0 录出来的 Insert，和主键盘 Insert 同等对待
    static const QStringList standaloneKeys = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "Ins", "Num+Ins"};
    if (!(hasModifier && hasPrimary) && !standaloneKeys.contains(text)) {
        QMessageBox::warning(parent, title, "快捷键必须包含至少一个修饰键（Ctrl/Alt/Shift/Meta）和一个主键；  \n或者是一个单独的F1~F11、Insert。  ");
        return false;
    }
    if (registry.isUsedByPhrase(sequence, except)) {
        QMessageBox::warning(parent, title, "快捷键已被使用  ");
        return false;
    }
    return true;
}

HotkeyEditFilter::HotkeyEditFilter(QKeySequenceEdit *edit, HotkeyRegistry &registry, GlobalHotkey *alsoSuspend, QObject *parent)
    : QObject(parent), edit_(edit), registry_(registry), alsoSuspend_(alsoSuspend) {}

void HotkeyEditFilter::setFocusOutHandler(std::function<void()> handler) {
    focusOutHandler_ = std::move(handler);
}

bool HotkeyEditFilter::eventFilter(QObject *watched, QEvent *event) {
    if (watched != edit_) return QObject::eventFilter(watched, event);
    switch (event->type()) {
    case QEvent::KeyPress:
        return recordKeypadKey(edit_, static_cast<QKeyEvent *>(event));
    case QEvent::FocusIn:
        registry_.setPhraseHotkeysRegistered(false);
        if (alsoSuspend_) alsoSuspend_->setRegistered(false);
        return false;
    case QEvent::FocusOut:
        registry_.setPhraseHotkeysRegistered(true);
        if (focusOutHandler_) focusOutHandler_();
        if (alsoSuspend_) alsoSuspend_->setRegistered(true);
        return false;
    default:
        return QObject::eventFilter(watched, event);
    }
}
