#include "ui/grouppanel.h"

#include "core/groupmodel.h"
#include "core/settings.h"
#include "platform/nativeui.h"
#include "ui/mainwindow.h"
#include "ui/phraselist.h"
#include "ui/stylesheets.h"

#include <QApplication>
#include <QCursor>
#include <QDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QVariantAnimation>
#include <QWheelEvent>

#include <windows.h>
#include <dwmapi.h>

namespace {

const int kButtonMaxWidth = 150; // 左上角分组按钮最宽多少，主窗口窄时会更窄
const int kPanelItemWidth = 150; // 面板里每个分组多宽，面板宽度由它推出来
const int kPanelItemHeight = 40; // 面板里每个分组多高（含上下各 2px 外边距）
const int kHoverPollIntervalMs = 120;

} // namespace

GroupButton::GroupButton(QWidget *parent) : QPushButton(parent) {
    setObjectName("groupButton"); // 背景、边框、文字由 QSS 画，这里只补一个箭头
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    animation_ = new QVariantAnimation(this);
    animation_->setDuration(150);
    connect(animation_, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        angle_ = value.toReal();
        update();
    });
}

void GroupButton::rotateArrowTo(qreal degrees) {
    if (qFuzzyCompare(angle_ + 1.0, degrees + 1.0)) return; // qFuzzyCompare 比不了 0
    animation_->stop();
    animation_->setStartValue(angle_);
    animation_->setEndValue(degrees);
    animation_->start();
}

void GroupButton::enterEvent(QEnterEvent *event) {
    emit hovered(); // 收起不靠 leaveEvent，见 GroupPanel::startHoverPolling
    QPushButton::enterEvent(event);
}

void GroupButton::wheelEvent(QWheelEvent *event) {
    emit wheelScrolled(event->angleDelta().y());
    event->accept();
}

void GroupButton::paintEvent(QPaintEvent *event) {
    QPushButton::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(11, height() / 2.0); // 横坐标要和 QSS 里 #groupButton 的 padding-left 对上
    painter.rotate(angle_);
    QPen pen(QColor("#323130")); // 和按钮字色一致，悬停也不变
    pen.setWidthF(1.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    QPolygonF arrow;
    arrow << QPointF(-5, -2.5) << QPointF(0, 2.5) << QPointF(5, -2.5); // 倒着的 ^，顺时针转 90° 后尖端朝左
    painter.drawPolyline(arrow);
}

GroupPanelWindow::GroupPanelWindow() : QListWidget(nullptr) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint); // Tool：不在任务栏多出一个窗口
    setAttribute(Qt::WA_ShowWithoutActivating, true); // 和主窗口一样不抢前台，否则点分组后短语粘不进原来的程序
    setFocusPolicy(Qt::NoFocus);
    setObjectName("groupPanel");
    setStyleSheet(QString(StyleSheets::kApp) + StyleSheets::kGroupPanel); // 没有父对象，样式表得自己挂
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); // 安全区滚动按像素算
    setTextElideMode(Qt::ElideRight);
    setDragDropMode(QAbstractItemView::InternalMove); // 上下拖动改分组顺序
    setDefaultDropAction(Qt::MoveAction);
}

void GroupPanelWindow::prepareNativeFrame() {
    HWND hwnd = reinterpret_cast<HWND>(winId());
    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    if ((style & (WS_CAPTION | WS_SYSMENU)) == (WS_CAPTION | WS_SYSMENU)) return; // 只需要补一次
    // 系统只给带标题栏样式的窗口放关闭动画；标题栏占的地方在 nativeEvent 里吃掉，外观不变（照抄 Tauri 的做法）
    SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_CAPTION | WS_SYSMENU);
    // Qt::Tool 挂的 WS_EX_TOOLWINDOW 也会让系统不放动画，得摘掉；同时挂上 WS_EX_NOACTIVATE，
    // 系统就不会给它建任务栏按钮——前提是显示之前就挂上
    const LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, (exStyle & ~WS_EX_TOOLWINDOW) | WS_EX_NOACTIVATE);
    // 带标题栏的窗口在 Win11 上会被切成圆角，要求保持直角。MinGW 头文件没有这两个常量：
    // 33 = DWMWA_WINDOW_CORNER_PREFERENCE，1 = DWMWCP_DONOTROUND。Win10 上调用失败也无所谓
    DWORD noRounding = 1;
    DwmSetWindowAttribute(hwnd, 33, &noRounding, sizeof(noRounding));
    // 必须带 SWP_FRAMECHANGED，nativeEvent 才会收到 WM_NCCALCSIZE
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

bool GroupPanelWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_NCCALCSIZE) { // 整个窗口都算客户区，偷偷加上的标题栏就没地方画了
        *result = 0;
        return true;
    }
    return QListWidget::nativeEvent(eventType, message, result);
}

void GroupPanelWindow::wheelEvent(QWheelEvent *event) {
    emit wheelScrolled(event->angleDelta().y()); // 面板上滚轮也是切分组，不是滚列表
    event->accept();
}

GroupPanel::GroupPanel(MainWindow *mainWindow, GroupModel &groups, GroupButton *button)
    : QObject(mainWindow), mainWindow_(mainWindow), groups_(groups), button_(button), window_(new GroupPanelWindow) {
    hideTimer_.setSingleShot(true);
    connect(&hideTimer_, &QTimer::timeout, this, &GroupPanel::hideIfIdle);

    // 用轮询光标坐标而不是 Enter/Leave：按钮和面板是两个顶层窗口，面板里还套着视口，
    // 鼠标在它们之间移动时 Enter/Leave 对不齐。直接比两个矩形最省事也最准。
    hoverPoll_.setInterval(kHoverPollIntervalMs);
    connect(&hoverPoll_, &QTimer::timeout, this, [this]() {
        const QPoint cursor = QCursor::pos();
        const bool onButton = button_->isVisible() && QRect(button_->mapToGlobal(QPoint(0, 0)), button_->size()).contains(cursor);
        const bool onPanel = window_->isVisible() && window_->frameGeometry().contains(cursor);
        hovering_ = onButton || onPanel;
        if (hovering_) hideTimer_.stop();
        else if (!hideTimer_.isActive()) hideTimer_.start(Settings::groupPanelDelayMs());
    });

    connect(button_, &GroupButton::hovered, this, &GroupPanel::show);
    connect(button_, &GroupButton::wheelScrolled, this, &GroupPanel::switchByWheel);
    connect(window_, &GroupPanelWindow::wheelScrolled, this, &GroupPanel::switchByWheel);

    connect(window_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const int row = window_->row(item);
        if (row >= 0 && row < groups_.count()) groups_.setCurrentIndex(row);
    });
    connect(window_->model(), &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex &, int start, int, const QModelIndex &, int row) {
        onRowsMoved(start, row);
    });

    // 按钮上显示的就是当前分组，右键菜单作用在当前分组上；面板上作用在点到的分组上，点到空白处只有新建
    button_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(button_, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        showContextMenu(button_, pos, groups_.currentIndex(), false);
    });
    window_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(window_, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *item = window_->itemAt(pos);
        showContextMenu(window_, pos, item ? window_->row(item) : -1, true);
    });
}

GroupPanel::~GroupPanel() {
    delete window_;
}

void GroupPanel::onRowsMoved(int start, int row) {
    const int target = row > start ? row - 1 : row; // row 是“插到第几行前面”，往下拖要扣掉自己原来那一行
    if (start == target) return;
    reordering_ = true; // 列表已经是拖好的样子，下面切换当前分组时不要再重建
    groups_.move(start, target);
    reordering_ = false;
    window_->setCurrentRow(groups_.currentIndex());
}

void GroupPanel::showContextMenu(QWidget *anchor, const QPoint &pos, int index, bool insertAfterIndex) {
    QMenu menu;
    menu.setStyleSheet(StyleSheets::kApp); // 没有父对象，样式表得自己挂
    QAction *editAction = nullptr;
    QAction *batchAction = nullptr;
    QAction *deleteAction = nullptr;
    if (index >= 0) {
        editAction = menu.addAction("修改分组");
        batchAction = menu.addAction("批量编辑短语");
    }
    QAction *newAction = menu.addAction(insertAfterIndex && index >= 0 ? "在当前分组后新建分组" : "新建分组");
    if (index >= 0) deleteAction = menu.addAction("删除分组");

    QAction *chosen = menu.exec(anchor->mapToGlobal(pos));
    if (!chosen) {
        // 什么都没选
    } else if (chosen == newAction) {
        emit newGroupRequested(insertAfterIndex && index >= 0 ? index + 1 : -1);
    } else if (chosen == editAction) {
        emit editGroupRequested(index);
    } else if (chosen == batchAction) {
        emit batchEditRequested(index);
    } else if (chosen == deleteAction) {
        emit deleteGroupRequested(index);
    }
    hideIfIdle(); // 菜单关掉时鼠标多半已经不在面板上了
}

void GroupPanel::switchByWheel(int delta) {
    if (delta > 0) groups_.setCurrentIndex(qMax(0, groups_.currentIndex() - 1));
    else if (delta < 0) groups_.setCurrentIndex(qMin(groups_.count() - 1, groups_.currentIndex() + 1));
    // 自己再滚一次：切换当前分组的处理只在面板已显示、行数对得上时才滚
    syncSelection();
}

void GroupPanel::refreshButtonText() {
    const QString name = groups_.currentName();
    const int available = button_->width() - 24; // 扣掉左边箭头和右边留白（QSS padding 左 20 右 2）
    if (available <= 0) { // 按钮宽度还没定，等 MainWindow::applySize 里再截一次
        button_->setText(name);
        return;
    }
    button_->setText(QFontMetrics(button_->font()).elidedText(name, Qt::ElideRight, available));
}

int GroupPanel::buttonMaxWidth() {
    return kButtonMaxWidth;
}

void GroupPanel::syncSelection() {
    const int row = groups_.currentIndex();
    if (row < 0 || row >= window_->count()) return;
    const int direction = row >= window_->currentRow() ? 1 : -1; // 往后翻当成↓；刚重建过 currentRow 是 -1，也按↓
    selectWithSafeArea(window_, window_->item(row), direction);
}

void GroupPanel::rebuild() {
    if (reordering_) return;
    bool unchanged = window_->count() == groups_.count();
    for (int i = 0; unchanged && i < groups_.count(); ++i) {
        if (window_->item(i)->text() != groups_.name(i)) unchanged = false;
    }
    // 内容真变了才重建：用左右键连续翻组时每翻一次都会走到这里
    if (!unchanged) {
        window_->clear();
        for (int i = 0; i < groups_.count(); ++i) {
            auto *item = new QListWidgetItem(groups_.name(i), window_);
            item->setTextAlignment(Qt::AlignCenter);
            item->setSizeHint(QSize(0, kPanelItemHeight));
        }
    }
    syncSelection();
}

void GroupPanel::startHoverPolling() {
    hovering_ = true; // 刚弹出来先当鼠标在上面，下一次轮询会纠正
    hideTimer_.stop();
    hoverPoll_.start();
}

void GroupPanel::show() {
    if (!mainWindow_->isVisible()) return;
    rebuild();

    // 面板没有内边距和边框，只有分组左右各 2px 外边距；高度固定为主窗口的 2/3，装不下就在面板里滚动
    const int width = kPanelItemWidth + 4;
    const int height = mainWindow_->height() * 2 / 3;
    window_->setFixedSize(width, height);

    const QRect mainFrame = mainWindow_->frameGeometry(); // 带边框，才是真正贴着主窗口外沿
    int x = mainFrame.left() - width;
    int y = button_->mapToGlobal(QPoint(0, 0)).y() - 2; // 比按钮高 2px 才显得齐平
    QScreen *screen = QGuiApplication::screenAt(mainFrame.center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect available = screen->availableGeometry();
        if (x < available.left()) x = mainFrame.right() + 1; // 左边放不下就放到主窗口右边
        y = qBound(available.top(), y, qMax(available.top(), available.bottom() - height));
    }
    window_->move(x, y);

    window_->prepareNativeFrame(); // 要赶在第一次显示之前改样式
    if (!window_->isVisible()) window_->show();
    WindowStyle::addNoActivate(window_->winId());
    // 主窗口置顶时面板也置顶；否则插在主窗口正上面一层，跟着主窗口沉浮
    WindowStyle::showNoActivate(window_->winId(), Settings::alwaysOnTop(), mainWindow_->winId());

    syncSelection(); // 面板藏着时量不到视口高度，显示出来后再滚一次
    button_->rotateArrowTo(90);
    if (!Settings::groupPanelAlwaysVisible()) startHoverPolling();
}

void GroupPanel::hide() {
    hoverPoll_.stop();
    hideTimer_.stop();
    hovering_ = false;
    window_->hide();
    button_->rotateArrowTo(0);
}

void GroupPanel::hideIfIdle() {
    if (!window_->isVisible()) return;
    if (Settings::groupPanelAlwaysVisible()) return;
    if (hovering_ || mainWindow_->navigator()->groupInertiaActive()) return;
    // 右键菜单开着、或者正按着鼠标拖分组时不收；轮询发现计时器停了会重新计时
    if (QApplication::activePopupWidget() || QApplication::mouseButtons() != Qt::NoButton) return;
    hide();
}

void GroupPanel::applyAlwaysVisible() {
    if (!Settings::groupPanelAlwaysVisible()) {
        hideIfIdle();
        return;
    }
    if (!mainWindow_->isSearchMode() && mainWindow_->isVisible()) show();
}

bool GroupPanel::isVisible() const {
    return window_->isVisible();
}

int GroupPanel::rowCount() const {
    return window_->count();
}

WId GroupPanel::existingWinId() const {
    return window_->internalWinId(); // 不用 winId()：别在原生事件过滤器里顺手把系统窗口建出来
}

namespace GroupDialog {

bool exec(QWidget *parent, const QString &title, QString &name, int &itemHeight, int &columns) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setFixedSize(280, 167);
    QFormLayout layout(&dialog);

    QLineEdit nameEdit(&dialog);
    nameEdit.setText(name);
    layout.addRow("分组名称：", &nameEdit);

    QSpinBox heightSpin(&dialog);
    heightSpin.setRange(10, 100);
    heightSpin.setValue(itemHeight);
    layout.addRow("行高：", &heightSpin);

    QSpinBox columnsSpin(&dialog);
    columnsSpin.setRange(1, 10);
    columnsSpin.setValue(columns);
    layout.addRow("列数：", &columnsSpin);

    QHBoxLayout buttons;
    QPushButton cancelButton("取消", &dialog);
    QPushButton okButton("确定", &dialog);
    okButton.setDefault(true);
    buttons.addStretch();
    buttons.addWidget(&cancelButton);
    buttons.addWidget(&okButton);
    layout.addRow(&buttons);

    QObject::connect(&okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(&cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return false;
    name = nameEdit.text();
    itemHeight = heightSpin.value();
    columns = columnsSpin.value();
    return true;
}

} // namespace GroupDialog
