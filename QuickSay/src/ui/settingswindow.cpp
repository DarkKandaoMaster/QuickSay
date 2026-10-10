#include "ui/settingswindow.h"

#include "app/quicksayapp.h"
#include "app/updatechecker.h"
#include "core/backup.h"
#include "core/files.h"
#include "core/groupmodel.h"
#include "core/settings.h"
#include "core/version.h"
#include "hotkey/globalhotkey.h"
#include "hotkey/hotkeyinput.h"
#include "platform/startup.h"
#include "ui/grouppanel.h"
#include "ui/mainwindow.h"
#include "ui/phraselist.h"
#include "ui/stylesheets.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <windows.h>

namespace {

// 自己画页签里的图标和文字：Qt 默认两者间距比 Windows 原生页签宽，这里固定 5px，接近 TrafficMonitor 的 CTabCtrl
class SettingsTabBar : public QTabBar {
  public:
    using QTabBar::QTabBar;

  protected:
    void paintEvent(QPaintEvent *) override {
        QStylePainter painter(this);
        // 选中页签最后画，免得被相邻页签盖住凸出来的边框
        for (int i = 0; i < count(); ++i) {
            if (i != currentIndex()) paintTab(painter, i);
        }
        if (currentIndex() >= 0) paintTab(painter, currentIndex());
    }

  private:
    void paintTab(QStylePainter &painter, int index) {
        QStyleOptionTab option;
        initStyleOption(&option, index);
        painter.drawControl(QStyle::CE_TabBarTabShape, option); // 底色和边框仍交给 QSS

        const bool selected = option.state.testFlag(QStyle::State_Selected);
        QRect content = option.rect;
        // 选中页签高 2px，居中的图文自然抬高 1px，再上移 1px 凑够 2px；未选中页签矮 2px，图文往下放
        if (selected) content.translate(0, -1);
        else content.adjust(0, 2, 0, 0);
        const QIcon::Mode mode = option.state.testFlag(QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled;
        const QIcon::State state = selected ? QIcon::On : QIcon::Off;
        const QSize iconSize = option.icon.actualSize(this->iconSize(), mode, state);
        const int textWidth = option.fontMetrics.horizontalAdvance(option.text);
        const int spacing = !option.icon.isNull() && !option.text.isEmpty() ? 5 : 0;
        int left = content.center().x() - (iconSize.width() + spacing + textWidth) / 2;

        if (!option.icon.isNull()) {
            // 图标视觉重心偏高，单独下移 1px 和原生页签对齐
            const QRect iconRect(left, content.center().y() - iconSize.height() / 2 + 1, iconSize.width(), iconSize.height());
            option.icon.paint(&painter, iconRect, Qt::AlignCenter, mode, state);
            left = iconRect.right() + 1 + spacing;
        }
        const QRect textRect(left, content.top(), textWidth, content.height());
        style()->drawItemText(&painter, textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic, option.palette,
            option.state.testFlag(QStyle::State_Enabled), option.text, QPalette::WindowText);
    }
};

// 只有 QTabWidget 的子类才能换掉内部页签栏
class SettingsTabWidget : public QTabWidget {
  public:
    explicit SettingsTabWidget(QWidget *parent) : QTabWidget(parent) {
        setTabBar(new SettingsTabBar(this));
    }
};

// 滚轮落在数字框或没展开的下拉框上时不改值，改为滚动所在的页面
class WheelToScrollFilter : public QObject {
  public:
    using QObject::QObject;

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() != QEvent::Wheel) return QObject::eventFilter(watched, event);
        auto *widget = qobject_cast<QWidget *>(watched);
        if (!widget) return QObject::eventFilter(watched, event);
        if (auto *combo = qobject_cast<QComboBox *>(widget)) {
            if (combo->view() && combo->view()->isVisible()) return QObject::eventFilter(watched, event);
        }
        QWidget *ancestor = widget->parentWidget();
        while (ancestor && !qobject_cast<QScrollArea *>(ancestor))
            ancestor = ancestor->parentWidget();
        if (auto *area = qobject_cast<QScrollArea *>(ancestor)) QApplication::sendEvent(area->viewport(), event);
        return true; // 找没找到滚动页都吃掉，绝不能让滚轮改值
    }
};

// 一页 = 独立的滚动区域（各页记住自己的滚动位置）+ 竖排分组框的内容控件
QScrollArea *makePage(QTabWidget *tabs, const QString &iconFile, const QString &title, QVBoxLayout *&layout) {
    auto *area = new QScrollArea;
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->viewport()->setObjectName("settingsViewport");
    auto *content = new QWidget;
    content->setObjectName("settingsPage");
    layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    area->setWidget(content);
    tabs->addTab(area, QIcon(Paths::icon(iconFile)), title);
    return area;
}

// 分组框用网格排：第 0 列说明文字，第 1 列控件，第 2 列吃掉多余宽度让控件靠左
QGridLayout *makeGroupBox(const QString &title, QVBoxLayout *pageLayout) {
    auto *box = new QGroupBox(title);
    auto *grid = new QGridLayout(box);
    grid->setContentsMargins(10, 10, 10, 10);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(2, 1);
    pageLayout->addWidget(box);
    return grid;
}

QLabel *addRow(QGridLayout *grid, const QString &text, QWidget *widget) {
    const int row = grid->rowCount();
    auto *label = new QLabel(text);
    grid->addWidget(label, row, 0);
    grid->addWidget(widget, row, 1);
    return label;
}

void addCheckBox(QGridLayout *grid, QCheckBox *checkBox) {
    grid->addWidget(checkBox, grid->rowCount(), 0, 1, 3);
}

// 缩进的子选项：父选项不勾时子选项取消勾选并整行隐藏。返回整行，填值时父选项信号被屏蔽，要靠它手动摆正显隐
QWidget *addSubCheckBox(QGridLayout *grid, QCheckBox *parentCheck, QCheckBox *childCheck) {
    auto *row = new QWidget; // 用容器整体右推，不直接给复选框设 margin，免得碰到它的原生外观
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(24, 0, 0, 0);
    rowLayout->addWidget(childCheck);
    grid->addWidget(row, grid->rowCount(), 0, 1, 3);
    QObject::connect(parentCheck, &QCheckBox::toggled, row, [row, childCheck](bool on) {
        if (!on) childCheck->setChecked(false); // 别藏着一个仍然生效的开关
        row->setVisible(on);
    });
    row->setVisible(parentCheck->isChecked());
    return row;
}

QLabel *addHint(QGridLayout *grid, const QString &text) {
    auto *label = new QLabel(text);
    label->setObjectName("settingsHint");
    label->setWordWrap(true);
    grid->addWidget(label, grid->rowCount(), 0, 1, 3);
    return label;
}

QSpinBox *makeSpinBox(int minimum, int maximum, int width, QObject *wheelFilter) {
    auto *box = new QSpinBox;
    box->setRange(minimum, maximum);
    box->setFixedSize(width, 24); // Qt 默认比 TrafficMonitor 的矮 4px
    box->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    box->installEventFilter(wheelFilter);
    return box;
}

QPushButton *makeButton(const QString &text, QWidget *parent = nullptr) {
    auto *button = new QPushButton(text, parent);
    button->setFixedSize(92, 28); // 和 TrafficMonitor 一致
    return button;
}

} // namespace

SettingsWindow::SettingsWindow(QuickSayApp *app, MainWindow *mainWindow, HotkeyRegistry &hotkeys)
    : QWidget(nullptr), app_(app), mainWindow_(mainWindow), hotkeys_(hotkeys) {
    setObjectName("settingsWindow"); // 专用 QSS 靠这个 ID
    setStyleSheet(StyleSheets::kSettings);
    setWindowTitle("QuickSay-设置");
    setWindowIcon(QIcon(Paths::icon("软件图标.svg")));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(13, 14, 13, 14); // 照 TrafficMonitor 量的
    outer->setSpacing(7);
    auto *tabs = new SettingsTabWidget(this);
    tabs->setIconSize(QSize(16, 16));
    outer->addWidget(tabs, 1);

    wheelFilter_ = new WheelToScrollFilter(this);
    buildMainWindowPage(tabs);
    buildInputPage(tabs);
    buildGeneralPage(tabs);
    buildAboutPage(tabs);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(12);
    buttons->addStretch();
    QPushButton *okButton = makeButton("确定", this);
    QPushButton *cancelButton = makeButton("取消", this);
    applyButton_ = makeButton("应用", this);
    buttons->addWidget(okButton);
    buttons->addWidget(cancelButton);
    buttons->addWidget(applyButton_);
    outer->addLayout(buttons);

    loadValues();
    connectChangeTracking();
    connect(applyButton_, &QPushButton::clicked, this, [this]() {
        apply();
    });
    connect(okButton, &QPushButton::clicked, this, [this]() {
        if (apply()) close();
    });
    connect(cancelButton, &QPushButton::clicked, this, &QWidget::close); // closeEvent 会丢弃未应用的修改
}

void SettingsWindow::buildMainWindowPage(QTabWidget *tabs) {
    QVBoxLayout *page = nullptr;
    makePage(tabs, "item.ico", "主窗口设置", page);
    auto spin = [this](int minimum, int maximum, int width) {
        return makeSpinBox(minimum, maximum, width, wheelFilter_);
    };

    QGridLayout *window = makeGroupBox("窗口", page);
    widthSpin_ = spin(250, 2000, 70);
    addRow(window, "主窗口宽度（像素）", widthSpin_);
    heightSpin_ = spin(250, 2000, 70);
    addRow(window, "主窗口高度（像素）", heightSpin_);
    alwaysOnTopCheck_ = new QCheckBox("主窗口始终置顶");
    addCheckBox(window, alwaysOnTopCheck_);
    searchOnSummonCheck_ = new QCheckBox("呼出主窗口后直接进入搜索");
    addCheckBox(window, searchOnSummonCheck_);
    toolTipDelaySpin_ = spin(0, 5000, 70);
    addRow(window, "鼠标悬停多久后显示提示（毫秒）", toolTipDelaySpin_);

    QGridLayout *items = makeGroupBox("短语项", page);
    itemHeightSpin_ = spin(10, 100, 59);
    addRow(items, "默认行高（像素）", itemHeightSpin_);
    addHint(items, "只影响新建分组时的行高。要改某个分组的行高，请在主窗口右键修改该分组。");
    paddingHorizontalSpin_ = spin(0, 100, 59);
    addRow(items, "短语项左右内边距（像素）", paddingHorizontalSpin_);
    paddingVerticalSpin_ = spin(0, 100, 59);
    addRow(items, "短语项上下内边距（像素）", paddingVerticalSpin_);
    badgeTopLeftCheck_ = new QCheckBox("角标显示在短语项左上角（不勾选则显示在右上角）");
    addCheckBox(items, badgeTopLeftCheck_);
    addHint(items, "角标位置改完后，鼠标移到主窗口上才会生效。");

    QGridLayout *groups = makeGroupBox("分组", page);
    groupPanelAlwaysVisibleCheck_ = new QCheckBox("始终显示分组面板");
    addCheckBox(groups, groupPanelAlwaysVisibleCheck_);
    groupPanelDelaySpin_ = spin(0, 5000, 70);
    groupPanelDelayLabel_ = addRow(groups, "鼠标移开或停止翻组多久后收起分组面板（毫秒）", groupPanelDelaySpin_);
    // 面板常显时根本不收起，延时这一行就藏起来
    connect(groupPanelAlwaysVisibleCheck_, &QCheckBox::toggled, groupPanelDelaySpin_, [this]() {
        updateGroupPanelDelayVisibility();
    });
    updateGroupPanelDelayVisibility();
    groupPanelSafeAreaSpin_ = spin(0, 1000, 59);
    addRow(groups, "翻分组时分组面板上下各留出的距离（像素）", groupPanelSafeAreaSpin_);
    selectFirstCheck_ = new QCheckBox("切换分组后始终选中第一条短语");
    addCheckBox(groups, selectFirstCheck_);

    QGridLayout *scroll = makeGroupBox("滚动", page);
    scrollStepSpin_ = spin(1, 100, 59);
    addRow(scroll, "滚动条滚动速度", scrollStepSpin_);
    safeAreaSpin_ = spin(0, 1000, 59);
    addRow(scroll, "浏览短语时上下各留出的距离（像素）", safeAreaSpin_);
    page->addStretch();
}

void SettingsWindow::buildInputPage(QTabWidget *tabs) {
    QVBoxLayout *page = nullptr;
    makePage(tabs, "taskbar_window.ico", "输入设置", page);

    QGridLayout *advanced = makeGroupBox("高级输入", page);
    outputDelaySpin_ = makeSpinBox(0, 2000, 70, wheelFilter_);
    addRow(advanced, "高级输入间隔（毫秒）", outputDelaySpin_);
    addHint(advanced, "高级输入里每两个动作之间等待的时间。目标程序反应慢时可以调大一些。");

    QGridLayout *keyboard = makeGroupBox("键盘操作", page);
    badgePassthroughCheck_ = new QCheckBox("钉住窗口时，不允许按下短语项对应角标按键输入短语");
    addCheckBox(keyboard, badgePassthroughCheck_);
    badgeCtrlCheck_ = new QCheckBox("钉住窗口时，允许按下Ctrl+角标按键输入短语");
    badgeCtrlRow_ = addSubCheckBox(keyboard, badgePassthroughCheck_, badgeCtrlCheck_);
    enterPassthroughCheck_ = new QCheckBox("钉住窗口时，不允许按下回车键输入短语");
    addCheckBox(keyboard, enterPassthroughCheck_);
    enterCtrlCheck_ = new QCheckBox("钉住窗口时，允许按下Ctrl+回车键输入短语");
    enterCtrlRow_ = addSubCheckBox(keyboard, enterPassthroughCheck_, enterCtrlCheck_);
    arrowPassthroughCheck_ = new QCheckBox("钉住窗口时，不允许按下方向键切换短语或分组");
    addCheckBox(keyboard, arrowPassthroughCheck_);
    arrowCtrlCheck_ = new QCheckBox("钉住窗口时，允许按下Ctrl+方向键切换短语或分组");
    arrowCtrlRow_ = addSubCheckBox(keyboard, arrowPassthroughCheck_, arrowCtrlCheck_);
    page->addStretch();
}

void SettingsWindow::buildGeneralPage(QTabWidget *tabs) {
    QVBoxLayout *page = nullptr;
    makePage(tabs, "setting.ico", "常规设置", page);

    QGridLayout *hotkey = makeGroupBox("快捷键", page);
    hotkeyEdit_ = new QKeySequenceEdit(QKeySequence(Settings::hotkey()));
    hotkeyEdit_->setFixedSize(170, 24);
    addRow(hotkey, "全局快捷键（呼出主窗口）", hotkeyEdit_);
    // 编辑期间临时注销所有快捷键；焦点离开时不合规就恢复原值。合规的新快捷键要等“应用”才注册
    auto *filter = new HotkeyEditFilter(hotkeyEdit_, hotkeys_, hotkeys_.showHotkey(), this);
    filter->setFocusOutHandler([this]() {
        const QKeySequence sequence = hotkeyEdit_->keySequence();
        if (sequence.isEmpty() || !validateHotkey(sequence, hotkeys_, hotkeyEdit_)) hotkeyEdit_->setKeySequence(QKeySequence(Settings::hotkey()));
    });
    hotkeyEdit_->installEventFilter(filter);

    captureHotkeyEdit_ = new QKeySequenceEdit(QKeySequence(Settings::captureHotkey()));
    captureHotkeyEdit_->setFixedSize(170, 24);
    captureHotkeyEdit_->setClearButtonEnabled(true); // 清空就是不启用
    addRow(hotkey, "把选中的文字存为新短语", captureHotkeyEdit_);
    addHint(hotkey, "在任意程序里选中文字后按下这个快捷键，QuickSay会模拟Ctrl+C复制选中的文字，存为主窗口当前分组的最后一条短语。留空表示不启用。");
    auto *captureFilter = new HotkeyEditFilter(captureHotkeyEdit_, hotkeys_, hotkeys_.captureHotkey(), this);
    captureFilter->setFocusOutHandler([this]() {
        if (!validateCaptureHotkey(captureHotkeyEdit_->keySequence())) captureHotkeyEdit_->setKeySequence(QKeySequence(Settings::captureHotkey()));
    });
    captureHotkeyEdit_->installEventFilter(captureFilter);

    // 两个开关互相独立：只开管理员的话开机不自启、手动启动时自己提权；都开的话开机由计划任务直接以管理员权限启动
    QGridLayout *startup = makeGroupBox("启动", page);
    autoStartCheck_ = new QCheckBox("开机自启动");
    addCheckBox(startup, autoStartCheck_);
    runAsAdminCheck_ = new QCheckBox("以管理员权限启动");
    addCheckBox(startup, runAsAdminCheck_);
    addHint(startup, "勾选后就能允许QuickSay在任何地方输入，比如以管理员权限运行的记事本。");

    QGridLayout *backup = makeGroupBox("备份与恢复", page);
    auto *backupRow = new QWidget;
    auto *backupLayout = new QHBoxLayout(backupRow);
    backupLayout->setSpacing(8);
    backupLayout->setContentsMargins(0, 0, 0, 0);
    QPushButton *exportButton = makeButton("导出备份");
    QPushButton *importButton = makeButton("导入备份");
    backupLayout->addWidget(exportButton);
    backupLayout->addWidget(importButton);
    backupLayout->addStretch();
    backup->addWidget(backupRow, backup->rowCount(), 0, 1, 3);
    addHint(backup, "备份包含全部设置、分组和短语。导入会完全覆盖现有内容，并立即重启QuickSay。");
    connect(exportButton, &QPushButton::clicked, this, &SettingsWindow::exportBackup);
    connect(importButton, &QPushButton::clicked, this, &SettingsWindow::importBackup);
    page->addStretch();
}

void SettingsWindow::buildAboutPage(QTabWidget *tabs) {
    QVBoxLayout *page = nullptr;
    makePage(tabs, "info.ico", "关于", page);

    QGridLayout *about = makeGroupBox("关于 QuickSay", page);
    addRow(about, "版本", new QLabel(kAppVersion));
    auto *link = new QLabel("<a href=\"https://github.com/DarkKandaoMaster/QuickSay\">https://github.com/DarkKandaoMaster/QuickSay</a>");
    link->setOpenExternalLinks(true);
    link->setCursor(Qt::PointingHandCursor);
    addRow(about, "项目地址", link);
    addHint(about, "QuickSay 是一个 Windows 上的快捷短语工具：把常用的话存起来，并在需要时使用 QuickSay 输入。");

    QGridLayout *update = makeGroupBox("更新", page);
    checkUpdatesCheck_ = new QCheckBox("启动时检查更新");
    addCheckBox(update, checkUpdatesCheck_);
    QPushButton *checkNowButton = makeButton("立即检查");
    update->addWidget(checkNowButton, update->rowCount(), 0, 1, 3, Qt::AlignLeft);
    connect(checkNowButton, &QPushButton::clicked, this, []() {
        UpdateChecker::check(true);
    });
    page->addStretch();
}

void SettingsWindow::connectChangeTracking() {
    connect(hotkeyEdit_, &QKeySequenceEdit::keySequenceChanged, this, &SettingsWindow::updateApplyButton);
    connect(captureHotkeyEdit_, &QKeySequenceEdit::keySequenceChanged, this, &SettingsWindow::updateApplyButton);
    for (QCheckBox *check : {alwaysOnTopCheck_, searchOnSummonCheck_, groupPanelAlwaysVisibleCheck_, selectFirstCheck_, badgeTopLeftCheck_, badgePassthroughCheck_, enterPassthroughCheck_, arrowPassthroughCheck_,
             badgeCtrlCheck_, enterCtrlCheck_, arrowCtrlCheck_, autoStartCheck_, runAsAdminCheck_, checkUpdatesCheck_}) {
        connect(check, &QCheckBox::toggled, this, &SettingsWindow::updateApplyButton);
    }
    for (QSpinBox *spin : {widthSpin_, heightSpin_, itemHeightSpin_, paddingHorizontalSpin_, paddingVerticalSpin_, scrollStepSpin_, safeAreaSpin_, groupPanelSafeAreaSpin_, groupPanelDelaySpin_, outputDelaySpin_, toolTipDelaySpin_}) {
        connect(spin, &QSpinBox::valueChanged, this, &SettingsWindow::updateApplyButton);
    }
}

void SettingsWindow::updateGroupPanelDelayVisibility() {
    const bool visible = !groupPanelAlwaysVisibleCheck_->isChecked();
    groupPanelDelayLabel_->setVisible(visible);
    groupPanelDelaySpin_->setVisible(visible);
}

// 把设置填回所有控件，等于丢弃未应用的修改
void SettingsWindow::loadValues() {
    const QList<QWidget *> controls = {hotkeyEdit_, captureHotkeyEdit_, alwaysOnTopCheck_, searchOnSummonCheck_, toolTipDelaySpin_, widthSpin_, heightSpin_, itemHeightSpin_, paddingHorizontalSpin_, paddingVerticalSpin_, scrollStepSpin_, safeAreaSpin_,
        groupPanelDelaySpin_, groupPanelSafeAreaSpin_, groupPanelAlwaysVisibleCheck_, selectFirstCheck_, badgeTopLeftCheck_, outputDelaySpin_, badgePassthroughCheck_, enterPassthroughCheck_, arrowPassthroughCheck_,
        badgeCtrlCheck_, enterCtrlCheck_, arrowCtrlCheck_, autoStartCheck_, runAsAdminCheck_, checkUpdatesCheck_};
    for (QWidget *control : controls)
        control->blockSignals(true);
    hotkeyEdit_->setKeySequence(QKeySequence(Settings::hotkey()));
    captureHotkeyEdit_->setKeySequence(QKeySequence(Settings::captureHotkey()));
    alwaysOnTopCheck_->setChecked(Settings::alwaysOnTop());
    toolTipDelaySpin_->setValue(Settings::toolTipDelayMs());
    searchOnSummonCheck_->setChecked(Settings::searchOnSummon());
    widthSpin_->setValue(Settings::mainWidth());
    heightSpin_->setValue(Settings::mainHeight());
    itemHeightSpin_->setValue(Settings::defaultItemHeight());
    paddingHorizontalSpin_->setValue(Settings::itemPaddingHorizontal());
    paddingVerticalSpin_->setValue(Settings::itemPaddingVertical());
    scrollStepSpin_->setValue(Settings::scrollStep());
    safeAreaSpin_->setValue(Settings::scrollSafePixels());
    groupPanelDelaySpin_->setValue(Settings::groupPanelDelayMs());
    groupPanelSafeAreaSpin_->setValue(Settings::groupPanelSafePixels());
    groupPanelAlwaysVisibleCheck_->setChecked(Settings::groupPanelAlwaysVisible());
    selectFirstCheck_->setChecked(Settings::selectFirstOnGroupSwitch());
    updateGroupPanelDelayVisibility(); // 信号被屏蔽了，显隐要手动摆正
    badgeTopLeftCheck_->setChecked(Settings::badgeTopLeft());
    outputDelaySpin_->setValue(Settings::outputDelayMs());
    badgePassthroughCheck_->setChecked(Settings::badgeKeyPassthroughWhenPinned());
    enterPassthroughCheck_->setChecked(Settings::enterKeyPassthroughWhenPinned());
    arrowPassthroughCheck_->setChecked(Settings::arrowKeyPassthroughWhenPinned());
    badgeCtrlRow_->setVisible(badgePassthroughCheck_->isChecked());
    enterCtrlRow_->setVisible(enterPassthroughCheck_->isChecked());
    arrowCtrlRow_->setVisible(arrowPassthroughCheck_->isChecked());
    // 子选项必须在父选项之后填：父选项的 toggled 会清空子选项
    badgeCtrlCheck_->setChecked(Settings::badgeKeyCtrlWhenPinned());
    enterCtrlCheck_->setChecked(Settings::enterKeyCtrlWhenPinned());
    arrowCtrlCheck_->setChecked(Settings::arrowKeyCtrlWhenPinned());
    autoStartCheck_->setChecked(Settings::autoStart());
    runAsAdminCheck_->setChecked(Settings::runAsAdmin());
    checkUpdatesCheck_->setChecked(Settings::checkUpdatesOnStartup());
    for (QWidget *control : controls)
        control->blockSignals(false);
    updateApplyButton();
}

bool SettingsWindow::hasUnappliedChanges() const {
    return hotkeyEdit_->keySequence().toString() != Settings::hotkey() ||
        captureHotkeyEdit_->keySequence().toString() != Settings::captureHotkey() ||
        alwaysOnTopCheck_->isChecked() != Settings::alwaysOnTop() ||
        toolTipDelaySpin_->value() != Settings::toolTipDelayMs() ||
        searchOnSummonCheck_->isChecked() != Settings::searchOnSummon() ||
        widthSpin_->value() != Settings::mainWidth() ||
        heightSpin_->value() != Settings::mainHeight() ||
        itemHeightSpin_->value() != Settings::defaultItemHeight() ||
        paddingHorizontalSpin_->value() != Settings::itemPaddingHorizontal() ||
        paddingVerticalSpin_->value() != Settings::itemPaddingVertical() ||
        scrollStepSpin_->value() != Settings::scrollStep() ||
        safeAreaSpin_->value() != Settings::scrollSafePixels() ||
        groupPanelDelaySpin_->value() != Settings::groupPanelDelayMs() ||
        groupPanelSafeAreaSpin_->value() != Settings::groupPanelSafePixels() ||
        groupPanelAlwaysVisibleCheck_->isChecked() != Settings::groupPanelAlwaysVisible() ||
        selectFirstCheck_->isChecked() != Settings::selectFirstOnGroupSwitch() ||
        badgeTopLeftCheck_->isChecked() != Settings::badgeTopLeft() ||
        outputDelaySpin_->value() != Settings::outputDelayMs() ||
        badgePassthroughCheck_->isChecked() != Settings::badgeKeyPassthroughWhenPinned() ||
        enterPassthroughCheck_->isChecked() != Settings::enterKeyPassthroughWhenPinned() ||
        arrowPassthroughCheck_->isChecked() != Settings::arrowKeyPassthroughWhenPinned() ||
        badgeCtrlCheck_->isChecked() != Settings::badgeKeyCtrlWhenPinned() ||
        enterCtrlCheck_->isChecked() != Settings::enterKeyCtrlWhenPinned() ||
        arrowCtrlCheck_->isChecked() != Settings::arrowKeyCtrlWhenPinned() ||
        autoStartCheck_->isChecked() != Settings::autoStart() ||
        runAsAdminCheck_->isChecked() != Settings::runAsAdmin() ||
        checkUpdatesCheck_->isChecked() != Settings::checkUpdatesOnStartup();
}

void SettingsWindow::updateApplyButton() {
    applyButton_->setEnabled(hasUnappliedChanges());
}

// “应用”和“确定”都走这里：先校验，再把界面上的值落到设置和正在运行的程序上。返回 false 表示没应用成功
bool SettingsWindow::apply() {
    // 焦点离开输入框时旧快捷键已恢复注册，这里才是新快捷键真正生效的地方
    const QKeySequence newHotkey = hotkeyEdit_->keySequence();
    if (newHotkey.toString() != Settings::hotkey()) {
        if (newHotkey.isEmpty() || !validateHotkey(newHotkey, hotkeys_, hotkeyEdit_)) {
            hotkeyEdit_->setKeySequence(QKeySequence(Settings::hotkey()));
            updateApplyButton();
            return false;
        }
        if (newHotkey == captureHotkeyEdit_->keySequence()) {
            QMessageBox::warning(hotkeyEdit_, "快捷键不合规", "不能和“把选中的文字存为新短语”的快捷键相同  ");
            hotkeyEdit_->setKeySequence(QKeySequence(Settings::hotkey()));
            updateApplyButton();
            return false;
        }
        hotkeys_.showHotkey()->setShortcut(newHotkey, true);
        Settings::setHotkey(newHotkey.toString());
    }
    const QKeySequence newCaptureHotkey = captureHotkeyEdit_->keySequence();
    if (newCaptureHotkey.toString() != Settings::captureHotkey()) {
        if (!validateCaptureHotkey(newCaptureHotkey)) {
            captureHotkeyEdit_->setKeySequence(QKeySequence(Settings::captureHotkey()));
            updateApplyButton();
            return false;
        }
        const bool enabled = !newCaptureHotkey.isEmpty();
        if (!hotkeys_.captureHotkey()->setShortcut(newCaptureHotkey, enabled) && enabled) {
            QMessageBox::warning(captureHotkeyEdit_, "快捷键不可用", "该快捷键可能已被其他程序占用，请换一个  ");
            hotkeys_.captureHotkey()->setShortcut(QKeySequence(Settings::captureHotkey()), !Settings::captureHotkey().isEmpty());
            captureHotkeyEdit_->setKeySequence(QKeySequence(Settings::captureHotkey()));
            updateApplyButton();
            return false;
        }
        Settings::setCaptureHotkey(newCaptureHotkey.toString());
    }

    const bool itemHeightChanged = itemHeightSpin_->value() != Settings::defaultItemHeight();
    const bool badgeChanged = badgeTopLeftCheck_->isChecked() != Settings::badgeTopLeft();
    const bool sizeChanged = widthSpin_->value() != Settings::mainWidth() || heightSpin_->value() != Settings::mainHeight();
    const bool paddingChanged = paddingHorizontalSpin_->value() != Settings::itemPaddingHorizontal() || paddingVerticalSpin_->value() != Settings::itemPaddingVertical();
    const bool alwaysOnTopChanged = alwaysOnTopCheck_->isChecked() != Settings::alwaysOnTop();

    // 安全区、面板延时、切组首选项、输出间隔、悬停提示延迟这些都是用的时候现读设置，写进去就立即生效
    Settings::setMainWidth(widthSpin_->value());
    Settings::setMainHeight(heightSpin_->value());
    Settings::setAlwaysOnTop(alwaysOnTopCheck_->isChecked());
    Settings::setToolTipDelayMs(toolTipDelaySpin_->value());
    Settings::setSearchOnSummon(searchOnSummonCheck_->isChecked());
    Settings::setDefaultItemHeight(itemHeightSpin_->value());
    Settings::setItemPaddingHorizontal(paddingHorizontalSpin_->value());
    Settings::setItemPaddingVertical(paddingVerticalSpin_->value());
    Settings::setScrollStep(scrollStepSpin_->value());
    Settings::setScrollSafePixels(safeAreaSpin_->value());
    Settings::setGroupPanelDelayMs(groupPanelDelaySpin_->value());
    Settings::setGroupPanelSafePixels(groupPanelSafeAreaSpin_->value());
    Settings::setGroupPanelAlwaysVisible(groupPanelAlwaysVisibleCheck_->isChecked());
    Settings::setSelectFirstOnGroupSwitch(selectFirstCheck_->isChecked());
    Settings::setBadgeTopLeft(badgeTopLeftCheck_->isChecked());
    Settings::setOutputDelayMs(outputDelaySpin_->value());
    Settings::setBadgeKeyPassthroughWhenPinned(badgePassthroughCheck_->isChecked());
    Settings::setEnterKeyPassthroughWhenPinned(enterPassthroughCheck_->isChecked());
    Settings::setArrowKeyPassthroughWhenPinned(arrowPassthroughCheck_->isChecked());
    Settings::setBadgeKeyCtrlWhenPinned(badgeCtrlCheck_->isChecked());
    Settings::setEnterKeyCtrlWhenPinned(enterCtrlCheck_->isChecked());
    Settings::setArrowKeyCtrlWhenPinned(arrowCtrlCheck_->isChecked());
    Settings::setCheckUpdatesOnStartup(checkUpdatesCheck_->isChecked());
    Settings::save();

    mainWindow_->applyScrollStep();
    if (paddingChanged) mainWindow_->reapplyPhraseLayout(); // 内边距变了行高也变，要连多列一起重排
    if (sizeChanged) mainWindow_->applySize();
    mainWindow_->groupPanel()->applyAlwaysVisible(); // 常显开关可能刚改过；面板高度按主窗口高度算，也要重摆
    if (alwaysOnTopChanged) mainWindow_->applyAlwaysOnTop();

    // 自启这两项最后应用：可能弹 UAC、可能失败，失败要恢复原样
    const bool oldAutoStart = Settings::autoStart();
    const bool oldRunAsAdmin = Settings::runAsAdmin();
    const bool newRunAsAdmin = runAsAdminCheck_->isChecked();
    const bool startupChanged = autoStartCheck_->isChecked() != oldAutoStart || newRunAsAdmin != oldRunAsAdmin;
    if (startupChanged) {
        Settings::setAutoStart(autoStartCheck_->isChecked());
        Settings::setRunAsAdmin(newRunAsAdmin);
        const bool applied = AutoStart::apply(false);
        QuickSayApp::reinstallHooks(); // 刚才可能卡在 UAC 上好几秒
        if (!applied) {
            Settings::setAutoStart(oldAutoStart);
            Settings::setRunAsAdmin(oldRunAsAdmin);
            AutoStart::apply(true); // 按原设置恢复系统状态，不打扰模式保证这里不再弹 UAC
            Settings::save();
            loadValues();
            QMessageBox::information(this, "提示", "应用开机自启动设置失败");
            return false;
        }
        Settings::save();
    }

    updateApplyButton();
    if (itemHeightChanged) QMessageBox::information(this, "提示", "该设置名称为“默认行高”，因此只影响新建分组时的行高。  \n如果要修改某个分组的行高，请右键修改该分组。  ");
    if (badgeChanged) QMessageBox::information(this, "提示", "鼠标移到主窗口后生效  ");

    // 刚勾上“以管理员权限启动”而当前进程还是普通权限：问一句要不要现在重启，而不是直接把正在用的 QuickSay 掐掉
    if (startupChanged && newRunAsAdmin && !oldRunAsAdmin && !Elevation::isProcessElevated()) {
        QMessageBox box(QMessageBox::Question, "QuickSay", "重启QuickSay生效", QMessageBox::NoButton, this);
        QPushButton *restartButton = box.addButton("立即重启", QMessageBox::RejectRole); // 交换角色让它显示在右边
        QPushButton *laterButton = box.addButton("暂不重启", QMessageBox::AcceptRole); // 不重启也行，下次启动照样生效
        box.setDefaultButton(restartButton);
        box.setEscapeButton(laterButton);
        box.exec();
        if (box.clickedButton() == restartButton && !app_->restartElevated()) {
            QMessageBox::information(this, "提示", "重启失败，请手动重启"); // 用户拒绝了 UAC；开关仍会在下次启动时生效
        }
    }
    return true;
}

void SettingsWindow::open() {
    if (!isVisible()) {
        // 用户可能绕过 QuickSay 自己删了计划任务或注册表项，以系统里的真实状态为准
        const bool enabled = AutoStart::isEnabledInSystem();
        if (Settings::autoStart() != enabled) {
            Settings::setAutoStart(enabled);
            Settings::save();
        }
        loadValues();
        move(Settings::windowPos(WindowSlot::Settings));
        show();
    }
    if (isMinimized()) showNormal();
    raise();
    activateWindow();
}

// 正文里的 <img>/<file> 路径保持原文，不读取路径指向的文件
void SettingsWindow::exportBackup() {
    const QString defaultName = QString("QuickSay备份_%1").arg(QDate::currentDate().toString(Qt::ISODate));
    const QString selected = QFileDialog::getSaveFileName(this, "导出QuickSay备份", QDir::home().filePath(defaultName), "QuickSay备份文件 (*)");
    if (selected.isEmpty()) return;
    // 用户手动输了 .txt 之类的扩展名也照样导出成无扩展名文件
    const QFileInfo selectedInfo(selected);
    QString filePath = selected;
    if (!selectedInfo.suffix().isEmpty()) filePath = selectedInfo.dir().filePath(selectedInfo.completeBaseName());
    if (filePath != selected && QFile::exists(filePath)) {
        if (QMessageBox::question(this, "确认覆盖", QString("文件“%1”已存在，是否覆盖？").arg(QFileInfo(filePath).fileName()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    }
    QString error;
    if (!DataFile::writeAtomically(filePath, Backup::create(*mainWindow_->phraseList(), mainWindow_->groups()), error)) {
        QMessageBox::warning(this, "导出备份失败", QString("无法导出备份：%1").arg(error));
        return;
    }
    QMessageBox::information(this, "导出备份成功", QString("备份已导出到：\n%1").arg(QDir::toNativeSeparators(filePath)));
}

// 校验阶段只读不写；用户确认后才替换三个数据文件，所以取消、校验失败、替换失败都不会动现有数据
void SettingsWindow::importBackup() {
    const QString filePath = QFileDialog::getOpenFileName(this, "导入QuickSay备份", QDir::homePath(), "QuickSay备份文件 (*)");
    if (filePath.isEmpty()) return;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "导入备份失败", QString("无法读取备份：%1").arg(file.errorString()));
        return;
    }
    const QByteArray fileData = file.readAll();
    file.close();
    Backup::Data backup;
    QString error;
    if (!Backup::parse(fileData, backup, error)) {
        QMessageBox::warning(this, "导入备份失败", QString("文件无效或已损坏：%1").arg(error));
        return;
    }
    if (QMessageBox::warning(this, "确认导入备份", "导入备份将完全覆盖当前的全部设置、分组和短语，且无法撤销。\n\n是否继续？", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;

    Backup::moveWindowsOntoScreens(backup.settings);
    if (!Backup::replaceDataFiles(backup, error)) {
        QMessageBox::warning(this, "导入备份失败", QString("导入失败：%1").arg(error));
        return;
    }
    Settings::freezeWrites(); // 从这一刻起旧进程只准退出，绝不能再落盘覆盖导入结果

    bool versionKnown = false;
    const int comparison = compareVersions(backup.appVersion, kAppVersion, versionKnown);
    if (versionKnown && comparison > 0) {
        QMessageBox::warning(this, "导入成功", QString("刚刚导入的备份来自 QuickSay 的较新版本（%1），某些设置或短语内容可能无法被完整识别或支持。建议更新到最新版本。").arg(backup.appVersion));
    } else if (!versionKnown || comparison < 0) {
        const QString exportedAt = backup.exportedAtUtc.toLocalTime().toString("yyyy年M月d日 HH:mm:ss");
        QMessageBox::warning(this, "导入成功", QString("刚刚导入的备份版本较旧或未知（导出于：%1），建议重新导出以更新备份。").arg(exportedAt));
    } else {
        QMessageBox::information(this, "导入成功", "备份已完整导入，QuickSay 将立即重启。");
    }
    app_->restartAfterImport(this);
}

void SettingsWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (sizeInitialized_) return;
    sizeInitialized_ = true;
    // 目标尺寸只取一次：校正时再读设置会读到上次四舍五入后存回去的值，越变越大
    targetOuterSize_ = Settings::settingsWindowSize();
    // 这时原生窗口已建好但还没显示，Qt 也还没把几何同步过去，GetWindowRect 拿到的是个无关的尺寸，
    // 拿它算边框会让窗口先以错误大小露面再跳一下。所以这次按窗口样式直接算边框
    fitToTargetSize(computedBorder());
    QTimer::singleShot(0, this, [this]() {
        fitToTargetSize(outerSize() - size());
    }); // 真正贴到屏幕上后按实际边框再校正一次（抹掉换算的四舍五入误差、跨 DPI 屏的偏差）
}

// 不用 frameGeometry()：它只算 DWM 那圈看得见的边，不含 Win10/11 外面那圈透明拖拽边
QSize SettingsWindow::outerSize() {
    RECT rect;
    if (!GetWindowRect(reinterpret_cast<HWND>(winId()), &rect)) return frameGeometry().size();
    const qreal ratio = devicePixelRatioF(); // GetWindowRect 给的是物理像素
    return QSize(qRound((rect.right - rect.left) / ratio), qRound((rect.bottom - rect.top) / ratio));
}

bool SettingsWindow::validateCaptureHotkey(const QKeySequence &sequence) {
    if (sequence.isEmpty()) return true;
    if (!validateHotkey(sequence, hotkeys_, captureHotkeyEdit_)) return false;
    // validateHotkey 只和短语快捷键查重，呼出快捷键要自己比
    if (sequence == hotkeyEdit_->keySequence() || sequence.toString() == Settings::hotkey()) {
        QMessageBox::warning(captureHotkeyEdit_, "快捷键不合规", "不能和呼出主窗口的快捷键相同  ");
        return false;
    }
    return true;
}

// 和 GetWindowRect 一样含透明拖拽边，但不依赖窗口当前的实际几何
QSize SettingsWindow::computedBorder() {
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    const qreal ratio = devicePixelRatioF();
    RECT rect{0, 0, 0, 0};
    if (!AdjustWindowRectExForDpi(&rect, GetWindowLongW(hwnd, GWL_STYLE), FALSE, GetWindowLongW(hwnd, GWL_EXSTYLE), qRound(ratio * 96))) return QSize();
    return QSize(qRound((rect.right - rect.left) / ratio), qRound((rect.bottom - rect.top) / ratio));
}

// 把“外框 620*576”换算成客户区尺寸：最小外框和默认外框都和 TrafficMonitor 中文设置窗口一致
void SettingsWindow::fitToTargetSize(const QSize &border) {
    if (border.isEmpty()) return; // 窗口还没真正建好，交给延迟的那次校正
    setMinimumSize(QSize(620, 576) - border);
    resize(targetOuterSize_ - border);
}

void SettingsWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (Settings::writesFrozen() || !sizeInitialized_) return;
    if (isMaximized() || isMinimized() || !isVisible()) return; // 只记普通状态的尺寸
    Settings::setSettingsWindowSize(outerSize());
    Settings::saveLater();
}

void SettingsWindow::moveEvent(QMoveEvent *event) {
    QWidget::moveEvent(event);
    if (Settings::writesFrozen() || !isActiveWindow()) return;
    Settings::setWindowPos(WindowSlot::Settings, pos());
    Settings::saveLater();
}

void SettingsWindow::closeEvent(QCloseEvent *event) {
    loadValues(); // 标题栏的关闭按钮和“取消”一样，丢弃未应用的修改
    QWidget::closeEvent(event);
}
