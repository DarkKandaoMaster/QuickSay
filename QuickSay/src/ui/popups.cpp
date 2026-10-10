#include "ui/popups.h"

#include <QApplication>
#include <QCursor>
#include <QLabel>
#include <QScreen>
#include <QTimer>

namespace {

const int kDisplayMs = 1000;
const QPoint kCursorOffset(16, 20); // 和系统悬停提示一样落在光标右下方，不挡住光标

QLabel *g_label = nullptr;
QTimer *g_hideTimer = nullptr;

void ensureCreated() {
    if (g_label) return;
    // 不用 QToolTip：它全程只有一个，会和短语列表的悬停提示互相顶掉，而且 Qt 收到按键、点击就把它收起来
    // WindowDoesNotAcceptFocus 即 WS_EX_NOACTIVATE，不抢用户正在打字的窗口；WindowTransparentForInput 让鼠标点穿过去
    g_label = new QLabel(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
    g_label->setAttribute(Qt::WA_ShowWithoutActivating);
    g_label->setStyleSheet("QLabel{background-color:#ffffff;color:#222222;border:1px solid #c7c7c7;}");
    g_hideTimer = new QTimer(g_label);
    g_hideTimer->setSingleShot(true);
    QObject::connect(g_hideTimer, &QTimer::timeout, g_label, &QWidget::hide);
    // 没有父对象的顶层窗口要在 QApplication 析构前删掉
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, []() {
        delete g_label;
        g_label = nullptr;
        g_hideTimer = nullptr;
    });
}

} // namespace

void showCursorHint(const QString &text) {
    ensureCreated();
    g_label->setText(text);
    g_label->adjustSize();
    const QPoint cursor = QCursor::pos();
    QPoint pos = cursor + kCursorOffset;
    if (QScreen *screen = QGuiApplication::screenAt(cursor)) {
        // 光标在屏幕右边或底边时往里收，别画到屏幕外面
        const QRect area = screen->availableGeometry();
        pos.setX(qMin(pos.x(), area.right() - g_label->width() + 1));
        if (pos.y() + g_label->height() > area.bottom() + 1) pos.setY(cursor.y() - g_label->height() - 4);
        pos.setX(qMax(pos.x(), area.left()));
        pos.setY(qMax(pos.y(), area.top()));
    }
    g_label->move(pos);
    g_label->show();
    g_label->raise();
    g_hideTimer->start(kDisplayMs);
}
