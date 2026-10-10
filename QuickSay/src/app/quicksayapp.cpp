#include "app/quicksayapp.h"

#include "app/updatechecker.h"
#include "core/files.h"
#include "core/settings.h"
#include "hotkey/globalhotkey.h"
#include "output/keyinjector.h"
#include "output/outputrunner.h"
#include "platform/nativeui.h"
#include "platform/startup.h"
#include "ui/batchedit.h"
#include "ui/grouppanel.h"
#include "ui/mainwindow.h"
#include "ui/popups.h"
#include "ui/settingswindow.h"

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QLineEdit>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTranslator>
#include <QWinEventNotifier>

#include <windows.h>
#include <wtsapi32.h>

// 程序没带 Qt 的翻译文件，快捷键输入框空着时会显示英文的 “Press shortcut”。
// 只翻这一句，不值得为它带一整套 qm。要在建任何窗口之前装上。
class BuiltinTranslator : public QTranslator {
  public:
    using QTranslator::QTranslator;
    bool isEmpty() const override;
    QString translate(const char *context, const char *sourceText, const char *disambiguation, int n) const override;
};

// 进程级的 Win32 消息处理：
//   - 锁屏、注销、会话断开、睡眠时立即停止正在进行的高级输入
//   - 主窗口和分组面板呼出时不放 DWM 弹出动画，收起时保留
//   - 点主窗口不激活它（点搜索框除外，那会进入搜索模式）
class AppNativeEventFilter : public QAbstractNativeEventFilter {
  public:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
};

namespace {
const int kHookWatchdogIntervalMs = 60 * 1000; // 钩子被系统摘掉后最多失灵这么久
} // namespace

QuickSayApp::QuickSayApp(bool autostarted) {
    // 必须抢在任何可能显示主窗口、装浏览钩子的代码之前装好，原因见 HotkeyHook::install
    HotkeyHook::install();
    hookWatchdog_.setInterval(kHookWatchdogIntervalMs);
    connect(&hookWatchdog_, &QTimer::timeout, this, &QuickSayApp::reinstallHooks);
    hookWatchdog_.start();

    // 用户又启动了一次 QuickSay：后启动的实例置位事件后自己退出，这边把主窗口显示出来
    instanceNotifier_ = new QWinEventNotifier(SingleInstance::showWindowEvent(), this);
    connect(instanceNotifier_, &QWinEventNotifier::activated, this, [this]() {
        ResetEvent(SingleInstance::showWindowEvent()); // 手动重置事件，不重置会一直触发
        if (mainWindow_) mainWindow_->summonByUser();
    });

    qApp->installTranslator(new BuiltinTranslator(this)); // 要赶在建任何窗口之前，见 BuiltinTranslator
    qApp->setQuitOnLastWindowClosed(false); // 关窗口只是收到托盘
    Settings::load();
    Settings::save(); // 立即保存一次，把老版本缺的字段补上
    // 纠正程序挪过位置后过期的路径，保证注册表项和计划任务互斥。不打扰模式：这里绝不弹 UAC
    AutoStart::apply(true);

    mainWindow_ = std::make_unique<MainWindow>(groups_, hotkeys_);
    nativeFilter_ = std::make_unique<AppNativeEventFilter>();
    qApp->installNativeEventFilter(nativeFilter_.get());
    // WTS 通知要注册到真实的窗口句柄上；主窗口开机时不显示，句柄也一直在
    const HWND sessionWindow = reinterpret_cast<HWND>(mainWindow_->winId());
    const bool sessionRegistered = WTSRegisterSessionNotification(sessionWindow, NOTIFY_FOR_THIS_SESSION) != FALSE;
    connect(qApp, &QCoreApplication::aboutToQuit, this, [sessionWindow, sessionRegistered]() {
        OutputRunner::stop(); // 退出前停掉计时器、恢复输出状态
        Settings::flushPendingSave();
        if (sessionRegistered) WTSUnRegisterSessionNotification(sessionWindow);
    });

    // 短语快捷键在 MainWindow 里已经注册过，呼出快捷键排在它们之后注册
    auto *showHotkey = new GlobalHotkey(QKeySequence(Settings::hotkey()), true, &hotkeys_);
    connect(showHotkey, &GlobalHotkey::activated, mainWindow_.get(), &MainWindow::toggleFromHotkey);
    hotkeys_.setShowHotkey(showHotkey);
    const QKeySequence captureSequence(Settings::captureHotkey());
    auto *captureHotkey = new GlobalHotkey(captureSequence, !captureSequence.isEmpty(), &hotkeys_); // 空的不注册，QHotkey 注册空快捷键会报警告
    connect(captureHotkey, &GlobalHotkey::activated, this, &QuickSayApp::captureSelection);
    hotkeys_.setCaptureHotkey(captureHotkey);

    settingsWindow_ = std::make_unique<SettingsWindow>(this, mainWindow_.get(), hotkeys_);
    mainWindow_->addAuxiliaryWindow(settingsWindow_.get());
    connect(mainWindow_.get(), &MainWindow::settingsRequested, settingsWindow_.get(), &SettingsWindow::open);
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);

    createTrayIcon();

    if (!autostarted) mainWindow_->summon();
    // 开机自启的那份多等一会儿：开机时系统还在忙，网络也未必通了
    if (Settings::checkUpdatesOnStartup()) QTimer::singleShot(autostarted ? 10000 : 3000, qApp, []() {
        UpdateChecker::check(false);
    });
}

QuickSayApp::~QuickSayApp() {
    qApp->removeNativeEventFilter(nativeFilter_.get());
}

void QuickSayApp::reinstallHooks() {
    // 顺序同 CLAUDE.md：先快捷键钩子、再浏览钩子，浏览钩子先执行
    HotkeyHook::reinstall();
    BrowseHook::reinstallIfActive();
}

void QuickSayApp::createTrayIcon() {
    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setIcon(QIcon(Paths::icon("软件图标.svg")));
    connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (BatchEdit::isActive()) return; // 记事本开着时不能让用户退出，否则会留下含短语正文的临时文件
        if (reason == QSystemTrayIcon::Trigger) {
            mainWindow_->summonByUser();
        } else if (reason == QSystemTrayIcon::Context) {
            switch (TrayMenu::exec()) {
            case TrayMenu::Choice::Settings:
                settingsWindow_->open();
                break;
            case TrayMenu::Choice::Quit:
                qApp->quit();
                break;
            case TrayMenu::Choice::None:
                break;
            }
        }
    });
    trayIcon_->show();
    trayIcon_->setToolTip("QuickSay"); // 必须在 show() 之后
}

void QuickSayApp::captureSelection() {
    // 记事本批量编辑期间短语都锁着；导入备份后不许再落盘
    if (BatchEdit::isActive() || Settings::writesFrozen()) return;
    SelectionCapture::start([this](const QString &text) {
        if (BatchEdit::isActive() || Settings::writesFrozen()) return; // 等剪贴板的这一秒里状态可能变了
        if (text.trimmed().isEmpty()) {
            trayIcon_->showMessage("QuickSay", "没有复制到文字，请先选中要保存的文字。", QSystemTrayIcon::Warning, 3000);
            return;
        }
        const QString group = mainWindow_->addPhraseToCurrentGroup(text);
        // 成功了不发托盘通知：Windows 通知会排队，连存十条就要一条条弹上半分钟，还都堆进通知中心
        showCursorHint(QString("已存入分组“%1”").arg(group));
    });
}

void QuickSayApp::setHotkeysRegistered(bool registered) {
    // 先让出快捷键：新实例起来得比这份退得快的话，会撞上还没释放的快捷键
    if (!registered) hotkeys_.setPhraseHotkeysRegistered(false);
    if (GlobalHotkey *showHotkey = hotkeys_.showHotkey()) showHotkey->setRegistered(registered);
    GlobalHotkey *captureHotkey = hotkeys_.captureHotkey();
    if (captureHotkey && !captureHotkey->shortcut().isEmpty()) captureHotkey->setRegistered(registered);
    if (registered) hotkeys_.setPhraseHotkeysRegistered(true);
}

void QuickSayApp::releaseSingleInstance() {
    // 不让出互斥体，新实例会被单实例检测拦下，结果谁也没起来。先停监听，它盯着的句柄马上要关掉
    instanceNotifier_->setEnabled(false);
    SingleInstance::release();
}

bool QuickSayApp::restartElevated() {
    setHotkeysRegistered(false);
    releaseSingleInstance();
    // 刚为管理员开机自启弹过 UAC 建好了计划任务的话，直接用它重启，不再弹第二次
    if ((Settings::autoStart() && AutoStart::runAdminTaskNow()) || Elevation::relaunchElevated()) {
        qApp->quit();
        return true;
    }
    // 用户拒绝了 UAC：把让出去的东西抢回来，以普通权限接着跑
    reinstallHooks();
    SingleInstance::notifyIfRunning();
    instanceNotifier_->setHandle(SingleInstance::showWindowEvent()); // 重新抢到的是新句柄
    instanceNotifier_->setEnabled(true);
    setHotkeysRegistered(true);
    return false;
}

void QuickSayApp::restartAfterImport(QWidget *errorDialogParent) {
    // 不提供“稍后重启”：旧进程带着导入前的状态多跑一刻都可能把导入结果覆盖掉
    OutputRunner::stop();
    BrowseHook::stop();
    setHotkeysRegistered(false);
    releaseSingleInstance();
    QString error;
    if (!Elevation::launchViaExplorer(error)) {
        QMessageBox::critical(errorDialogParent, "重启失败", QString("备份已经导入，但无法通过 Windows Explorer 重新启动 QuickSay：%1\n\n请手动启动 QuickSay。").arg(error));
    }
    qApp->quit();
}

bool BuiltinTranslator::isEmpty() const {
    return false; // 返回 true 的话 Qt 根本不来问
}

QString BuiltinTranslator::translate(const char *context, const char *sourceText, const char *, int) const {
    if (qstrcmp(context, "QKeySequenceEdit") == 0 && qstrcmp(sourceText, "Press shortcut") == 0) return QStringLiteral("请按下快捷键");
    return QString(); // 空串表示不翻，用原文
}

bool AppNativeEventFilter::nativeEventFilter(const QByteArray &, void *message, qintptr *result) {
    const MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_WTSSESSION_CHANGE) {
        if (msg->wParam == WTS_SESSION_LOCK || msg->wParam == WTS_SESSION_LOGOFF || msg->wParam == WTS_CONSOLE_DISCONNECT || msg->wParam == WTS_REMOTE_DISCONNECT) OutputRunner::stop();
    } else if (msg->message == WM_POWERBROADCAST && msg->wParam == PBT_APMSUSPEND) { // 睡眠和休眠都会先收到它
        OutputRunner::stop();
    } else if (msg->message == WM_ENDSESSION && msg->wParam) {
        // 关机、注销时处理完这条消息进程就会被结束，事件循环里排着的延迟保存等不到了
        Settings::flushPendingSave();
    }

    MainWindow *window = MainWindow::instance();
    if (!window) return false;

    // 可见状态真正变化之前切换 DWM 过渡动画：要显示了就禁用，要隐藏了就恢复。lParam 非 0 是父窗口最小化之类连带的，不管
    if (msg->message == WM_SHOWWINDOW && msg->lParam == 0) {
        const HWND mainHwnd = reinterpret_cast<HWND>(window->winId());
        const HWND panelHwnd = reinterpret_cast<HWND>(window->groupPanel()->existingWinId());
        if (msg->hwnd == mainHwnd || (panelHwnd && msg->hwnd == panelHwnd)) WindowStyle::setTransitionsDisabled(reinterpret_cast<WId>(msg->hwnd), msg->wParam != 0);
    }

    if (msg->message == WM_MOUSEACTIVATE && !window->isSearchMode()) {
        const HWND mainHwnd = reinterpret_cast<HWND>(window->winId());
        // 对搜索框调 winId() 会让它（连同兄弟控件）变成原生子窗口，之后点它们都会收到各自的 WM_MOUSEACTIVATE
        const HWND searchHwnd = reinterpret_cast<HWND>(window->searchBox()->winId());
        if (msg->hwnd == searchHwnd || IsChild(searchHwnd, msg->hwnd)) {
            window->enterSearchMode();
            *result = MA_ACTIVATE;
            return true;
        }
        if (msg->hwnd == mainHwnd || IsChild(mainHwnd, msg->hwnd)) {
            *result = MA_NOACTIVATE;
            return true;
        }
    }
    return false;
}
