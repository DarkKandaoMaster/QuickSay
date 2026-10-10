#pragma once

#include <QObject>

#include <functional>

class GlobalHotkey;
class HotkeyRegistry;
class QKeySequence;
class QKeySequenceEdit;
class QWidget;

// 检查快捷键是否可用，不可用时弹警告。规则：恰好一个组合键；至少一个修饰键加一个主键，
// 或者是单独的 F1~F11、Insert；不能和别的短语重复（except 是正在修改的那条短语自己的快捷键）。
// 被别的程序占用的组合在录入时就录不进来，不用在这里管。
bool validateHotkey(const QKeySequence &sequence, const HotkeyRegistry &registry, QWidget *parent, const GlobalHotkey *except = nullptr);

// 快捷键输入框的事件过滤器：
//   - 自己录入小键盘按键（QKeySequenceEdit 会丢掉 KeypadModifier，小键盘 4 会录成 "4"）
//   - 获得焦点时注销全部短语快捷键（以及 alsoSuspend 指向的快捷键），失去焦点时恢复，让用户能自由录入
// 只用于短语快捷键输入框时 alsoSuspend 传 nullptr。
class HotkeyEditFilter : public QObject {
  public:
    HotkeyEditFilter(QKeySequenceEdit *edit, HotkeyRegistry &registry, GlobalHotkey *alsoSuspend, QObject *parent);

    // 失去焦点时，在短语快捷键恢复之后、alsoSuspend 恢复之前调用（设置窗口用它校验呼出快捷键）
    void setFocusOutHandler(std::function<void()> handler);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    QKeySequenceEdit *edit_;
    HotkeyRegistry &registry_;
    GlobalHotkey *alsoSuspend_;
    std::function<void()> focusOutHandler_;
};
