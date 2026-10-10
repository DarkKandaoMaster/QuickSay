#pragma once

#include "output/outputparser.h"

#include <QObject>
#include <QTimer>
#include <QVector>

// 把一段高级输入按动作串行执行，动作之间隔 Settings::outputDelayMs()。
// 同一时间只有一个成员计时器在等待，停掉它就能一次性作废所有尚未执行的步骤。
class OutputRunner : public QObject {
  public:
    // 正在输出时忽略新的请求；记事本批量编辑期间也忽略
    static void start(const QString &text);
    // 锁屏、注销、会话断开、睡眠时调用；没有正在输出就什么也不做
    static void stop();

  private:
    enum class Step {
        None,
        NextAction,
        PasteText,
        WriteFileClipboard,
        PasteFile,
        FinishSleep,
    };

    explicit OutputRunner(const QVector<OutputAction> &actions, QObject *parent);
    ~OutputRunner() override;

    void run();
    void finish();
    void waitFor(int ms, Step step);
    void finishAction();
    void runWaitingStep();
    void runCurrentAction();
    void paste();
    // 设置里开了“切换窗口就停止”时，每次真正发按键前核对前台程序还是不是开始输出时那个
    bool targetStillInFront();

    QVector<OutputAction> actions_;
    int index_ = 0;
    QTimer timer_;
    Step waitingStep_ = Step::None;
    OutputAction pendingFile_;
    bool ended_ = false;
    unsigned long targetProcess_ = 0; // 第一次发按键时的前台进程 ID

    static OutputRunner *current_;
};
