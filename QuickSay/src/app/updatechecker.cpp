#include "app/updatechecker.h"

#include "core/files.h"
#include "core/settings.h"
#include "core/version.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <thread>

#include <windows.h>
#include <winhttp.h>

namespace {

const wchar_t *const kApiHost = L"api.github.com";
const wchar_t *const kApiPath = L"/repos/DarkKandaoMaster/QuickSay/releases/latest";
const QString kReleasesPage = "https://github.com/DarkKandaoMaster/QuickSay/releases"; // 拿不到具体 release 地址时退回发布列表页

bool g_checking = false; // 启动后的自动检查和手动检查可能撞在一起，同时只跑一个
int g_automaticFailures = 0;

struct Result {
    bool ok = false;
    QString version; // 如 "1.8.1"
    QString url; // release 页面，更新日志和下载都在上面
};

// 同步阻塞，只能在后台线程调用
Result fetchLatestRelease() {
    Result result;
    HINTERNET session = WinHttpOpen(L"QuickSay", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0); // 走系统代理设置
    if (!session) return result;
    WinHttpSetTimeouts(session, 8000, 8000, 8000, 8000); // 别让 10 秒后的重试排到几十秒之后
    QByteArray body;
    if (HINTERNET connection = WinHttpConnect(session, kApiHost, INTERNET_DEFAULT_HTTPS_PORT, 0)) {
        if (HINTERNET request = WinHttpOpenRequest(connection, L"GET", kApiPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)) {
            const wchar_t *headers = L"User-Agent: QuickSay\r\nAccept: application/vnd.github+json\r\n"; // GitHub 不带 User-Agent 直接 403
            if (WinHttpSendRequest(request, headers, -1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr)) {
                DWORD status = 0, length = sizeof(status);
                WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &length, WINHTTP_NO_HEADER_INDEX);
                if (status == 200) {
                    DWORD available = 0;
                    while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
                        QByteArray chunk(available, 0);
                        DWORD read = 0;
                        if (!WinHttpReadData(request, chunk.data(), available, &read)) break;
                        body.append(chunk.constData(), read);
                        if (body.size() > 1024 * 1024) break; // release JSON 也就几十 KB，超过 1MB 肯定不对
                    }
                }
            }
            WinHttpCloseHandle(request);
        }
        WinHttpCloseHandle(connection);
    }
    WinHttpCloseHandle(session);

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) return result;
    const QJsonObject release = doc.object();
    // 标签历来写成 "QuickSay_v1.8.1" 这样，从里面抠出第一串点分数字
    const QRegularExpressionMatch match = QRegularExpression("\\d+(?:\\.\\d+){1,3}").match(release["tag_name"].toString());
    if (!match.hasMatch()) return result;
    const QString url = release["html_url"].toString();
    result.ok = true;
    result.version = match.captured();
    result.url = url.startsWith("https://github.com/") ? url : kReleasesPage; // 只认 GitHub 自己的地址，防止响应被掉包把用户带去别处
    return result;
}

enum class Choice {
    Closed = 0,
    Left = 1,
    Download = 2,
};

// 一行说明 + “查看更新日志”链接 + 两个按钮。不挂样式表，用原生外观，和设置窗口一致。
// 左边按钮：自动检查时是“忽略该版本”，手动检查时是“取消”。
Choice showUpdateDialog(const QString &message, const QString &url, const QString &leftButtonText) {
    QDialog dialog;
    dialog.setWindowTitle("QuickSay");
    dialog.setWindowIcon(QIcon(Paths::icon("软件图标.svg")));
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 14);
    layout->setSpacing(10);
    layout->addWidget(new QLabel(message));
    auto *changelog = new QLabel(QString("<a href=\"%1\">查看更新日志</a>").arg(url));
    changelog->setOpenExternalLinks(true);
    changelog->setCursor(Qt::PointingHandCursor);
    layout->addWidget(changelog);
    layout->addStretch();
    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(12);
    buttons->addStretch();
    auto *leftButton = new QPushButton(leftButtonText, &dialog);
    auto *downloadButton = new QPushButton("前往下载", &dialog);
    for (QPushButton *button : {leftButton, downloadButton}) {
        button->setFixedSize(92, 28);
        buttons->addWidget(button);
    }
    layout->addLayout(buttons);
    downloadButton->setDefault(true);
    QObject::connect(leftButton, &QPushButton::clicked, &dialog, [&dialog]() {
        dialog.done(int(Choice::Left));
    });
    QObject::connect(downloadButton, &QPushButton::clicked, &dialog, [&dialog]() {
        dialog.done(int(Choice::Download));
    });
    dialog.setFixedSize(300, 148);
    const QRect screen = QGuiApplication::primaryScreen()->geometry();
    dialog.move(screen.center() - QPoint(dialog.width() / 2, dialog.height() / 2)); // 没有父窗口，自己居中
    return static_cast<Choice>(dialog.exec());
}

void showUpToDate() {
    QMessageBox box;
    box.setWindowTitle("QuickSay");
    box.setWindowIcon(QIcon(Paths::icon("软件图标.svg")));
    box.setIcon(QMessageBox::Information);
    box.setText("当前已经是最新版本。  ");
    box.setStandardButtons(QMessageBox::Ok);
    box.button(QMessageBox::Ok)->setText("确定");
    box.exec();
}

void handleResult(bool manual, const Result &result) {
    g_checking = false;
    if (manual) QApplication::restoreOverrideCursor();
    if (!result.ok) {
        if (manual) { // 失败也给出发布页入口，让用户能自己去看
            if (showUpdateDialog("检查更新失败，请检查网络连接。", kReleasesPage, "取消") == Choice::Download) QDesktopServices::openUrl(QUrl(kReleasesPage));
            return;
        }
        if (++g_automaticFailures >= 3) return;
        QTimer::singleShot(10000, qApp, []() {
            UpdateChecker::check(false);
        });
        return;
    }
    g_automaticFailures = 0;

    bool comparable = false;
    if (compareVersions(result.version, kAppVersion, comparable) <= 0 || !comparable) {
        if (manual) showUpToDate();
        return;
    }
    if (!manual && result.version == Settings::ignoredVersion()) return;
    const Choice choice = showUpdateDialog(QString("发现新版本 %1").arg(result.version), result.url, manual ? "取消" : "忽略该版本");
    if (choice == Choice::Download) {
        QDesktopServices::openUrl(QUrl(result.url));
    } else if (choice == Choice::Left && !manual) { // 直接关窗口不算忽略，只是这次不想看
        Settings::setIgnoredVersion(result.version);
        Settings::save();
    }
}

} // namespace

namespace UpdateChecker {

void check(bool manual) {
    if (g_checking) return;
    g_checking = true;
    if (manual) QApplication::setOverrideCursor(Qt::WaitCursor); // 不然点完好几秒没动静，像是按钮坏了
    std::thread([manual]() {
        const Result result = fetchLatestRelease();
        QMetaObject::invokeMethod(qApp, [manual, result]() {
            handleResult(manual, result);
        },
            Qt::QueuedConnection);
    }).detach();
}

} // namespace UpdateChecker
