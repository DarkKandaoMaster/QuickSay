#include "output/outputrunner.h"

#include "core/settings.h"
#include "output/keyinjector.h"
#include "ui/batchedit.h"

#include <QApplication>
#include <QClipboard>

#include <windows.h>

OutputRunner *OutputRunner::current_ = nullptr;

void OutputRunner::start(const QString &text) {
    if (BatchEdit::isActive()) return; // 否则短语会被直接粘进正在编辑的临时文件
    if (current_) return;
    current_ = new OutputRunner(parseOutputActions(text), qApp);
    current_->run();
}

void OutputRunner::stop() {
    if (current_) current_->finish();
}

OutputRunner::OutputRunner(const QVector<OutputAction> &actions, QObject *parent) : QObject(parent), actions_(actions), timer_(this) {
    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, [this]() {
        runWaitingStep();
    });
}

OutputRunner::~OutputRunner() {
    // 正常情况下 finish() 早已清理过；随 QApplication 一起销毁时再兜底一次
    timer_.stop();
    if (current_ == this) current_ = nullptr;
}

void OutputRunner::run() {
    if (!KeyInjector::releasePressedModifiers()) {
        finish();
        return;
    }
    runCurrentAction();
}

// 正常结束、可靠的发送失败、系统事件打断都走这里
void OutputRunner::finish() {
    if (ended_) return;
    ended_ = true;
    timer_.stop();
    waitingStep_ = Step::None;
    if (current_ == this) current_ = nullptr;
    deleteLater();
}

void OutputRunner::waitFor(int ms, Step step) {
    if (ended_) return;
    waitingStep_ = step;
    timer_.start(qMax(0, ms));
}

void OutputRunner::finishAction() {
    if (index_ >= actions_.size()) {
        finish();
        return;
    }
    waitFor(Settings::outputDelayMs(), Step::NextAction);
}

// 比进程而不是窗口：目标程序自己弹个对话框、换个子窗口不算切走
bool OutputRunner::targetStillInFront() {
    if (!Settings::stopOutputOnWindowSwitch()) return true;
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    // 不在 start 时就记：从搜索框回车输出时，前台要等 leaveSearchMode 还给原来的程序后才对
    if (targetProcess_ == 0) targetProcess_ = process;
    return process == targetProcess_;
}

void OutputRunner::paste() {
    if (!targetStillInFront()) {
        finish();
        return;
    }
    if (KeyInjector::sendCtrlV()) finishAction();
    else finish();
}

void OutputRunner::runWaitingStep() {
    if (ended_) return;
    const Step step = waitingStep_;
    waitingStep_ = Step::None;
    switch (step) {
    case Step::NextAction:
        runCurrentAction();
        break;
    case Step::PasteText:
    case Step::PasteFile:
        paste();
        break;
    case Step::WriteFileClipboard:
        if (!KeyInjector::setClipboardFile(pendingFile_.filePath)) finish();
        else waitFor(50, Step::PasteFile); // 同样给剪贴板留 50ms 传播时间
        break;
    case Step::FinishSleep:
        finishAction();
        break;
    case Step::None:
        break;
    }
}

void OutputRunner::runCurrentAction() {
    if (ended_) return;
    if (index_ >= actions_.size()) {
        finish();
        return;
    }

    const OutputAction action = actions_.at(index_++);
    switch (action.type) {
    case OutputAction::Type::Text:
        QApplication::clipboard()->setText(action.text);
        // 写剪贴板后要等目标程序能读到新内容；立刻粘贴会读到旧内容，粘出空行或上一条
        waitFor(50, Step::PasteText);
        break;
    case OutputAction::Type::File:
        pendingFile_ = action;
        // 微信里连着粘图片偶尔出错，先等 1 秒能规避
        waitFor(1000, Step::WriteFileClipboard);
        break;
    case OutputAction::Type::Press:
        if (targetStillInFront() && KeyInjector::sendCombo(action)) finishAction();
        else finish();
        break;
    case OutputAction::Type::Sleep:
        waitFor(action.sleepMs, Step::FinishSleep);
        break;
    }
}
