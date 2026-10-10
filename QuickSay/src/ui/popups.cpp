#include "ui/popups.h"

#include "platform/nativeui.h"
#include "ui/stylesheets.h"

#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

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

namespace {

const QSize kPopupSize(400, 260);

// 视口上的鼠标松开：有选中内容就结束。放在 viewport 上而不是编辑框上，鼠标事件实际发给的是视口
class PickFilter : public QObject {
  public:
    PickFilter(QWidget *popup, QPlainTextEdit *edit, std::function<void(const QString &)> onPicked)
        : QObject(popup), popup_(popup), edit_(edit), onPicked_(std::move(onPicked)) {}

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() != QEvent::MouseButtonRelease || static_cast<QMouseEvent *>(event)->button() != Qt::LeftButton) return QObject::eventFilter(watched, event);
        QString picked = edit_->textCursor().selectedText();
        if (picked.isEmpty()) return QObject::eventFilter(watched, event); // 只是点了一下，没选东西
        picked.replace(QChar::ParagraphSeparator, '\n'); // selectedText 用 U+2029 表示换行
        picked.replace(QChar::LineSeparator, '\n');
        popup_->close();
        // 等弹出窗口真正关掉、鼠标抓取还给系统之后再输出
        const auto onPicked = onPicked_;
        QTimer::singleShot(0, qApp, [onPicked, picked]() {
            onPicked(picked);
        });
        return true;
    }

  private:
    QWidget *popup_;
    QPlainTextEdit *edit_;
    std::function<void(const QString &)> onPicked_;
};

} // namespace

namespace SelectTextPopup {

void show(const QString &text, const QPoint &globalPos, std::function<void(const QString &picked)> onPicked) {
    // 不接受焦点：点进去拖选时也不能把前台从目标程序那里抢过来，否则选完的文字粘不回去
    auto *popup = new QWidget(nullptr, Qt::Popup | Qt::WindowDoesNotAcceptFocus);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setStyleSheet(StyleSheets::kApp); // 没有父对象，样式表得自己挂
    auto *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(4, 4, 4, 4);
    auto *edit = new QPlainTextEdit(popup);
    edit->setPlainText(text);
    edit->setReadOnly(true);
    edit->setContextMenuPolicy(Qt::NoContextMenu);
    edit->viewport()->setCursor(Qt::IBeamCursor); // 只读编辑框默认是箭头，看不出能选
    layout->addWidget(edit);
    edit->viewport()->installEventFilter(new PickFilter(popup, edit, std::move(onPicked)));

    popup->resize(kPopupSize);
    QPoint pos = globalPos;
    if (QScreen *screen = QGuiApplication::screenAt(globalPos)) { // 别伸出屏幕
        const QRect available = screen->availableGeometry();
        pos.setX(qBound(available.left(), pos.x(), available.right() - kPopupSize.width()));
        pos.setY(qBound(available.top(), pos.y(), available.bottom() - kPopupSize.height()));
    }
    popup->move(pos);
    WindowStyle::addNoActivate(popup->winId());
    popup->show();
}

} // namespace SelectTextPopup
