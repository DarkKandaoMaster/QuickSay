#include "ui/mainwindow.h"

#include "core/files.h"
#include "core/groupmodel.h"
#include "core/phrase.h"
#include "core/settings.h"
#include "hotkey/globalhotkey.h"
#include "output/keyinjector.h"
#include "output/outputrunner.h"
#include "platform/nativeui.h"
#include "ui/batchedit.h"
#include "ui/grouppanel.h"
#include "ui/phraseeditor.h"
#include "ui/phraselist.h"
#include "ui/popups.h"
#include "ui/stylesheets.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QToolTip>

#include <windows.h>

MainWindow *MainWindow::instance_ = nullptr;

MainWindow::MainWindow(GroupModel &groups, HotkeyRegistry &hotkeys) : QWidget(nullptr), groups_(groups), hotkeys_(hotkeys) {
    instance_ = this;
    setStyleSheet(StyleSheets::kApp);
    setWindowTitle("QuickSay");
    setWindowIcon(QIcon(Paths::icon("logo.svg")));

    list_ = new PhraseList(this);
    PhraseStore::load(*list_, groups_);
    applyScrollStep();
    hotkeys_.rebuildPhraseHotkeys(*list_);
    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (searchMode_) leaveSearchMode();
        outputPhrase(item);
    });

    navigator_ = new PhraseNavigator(list_, groups_, this);
    groupButton_ = new GroupButton(this);
    groupPanel_ = new GroupPanel(this, groups_, groupButton_);
    navigator_->setGroupPanel(groupPanel_);
    groupPanel_->refreshButtonText();

    search_ = new QLineEdit(this);
    search_->setClearButtonEnabled(true);
    connect(search_, &QLineEdit::textChanged, this, [this](const QString &text) {
        list_->applyFilter(groups_.currentName(), text);
        list_->selectFirstVisible();
    });

    connect(list_->model(), &QAbstractItemModel::rowsMoved, this, [this]() {
        commitPhraseChanges();
    }); // 拖动排序完成
    connect(&groups_, &GroupModel::moved, this, [this]() {
        PhraseStore::save(*list_, groups_);
    });

    // 过滤要在量行高之前做，量到的才是当前分组里看得见的条目
    refreshFilter();
    reapplyPhraseLayout();
    list_->selectFirstVisible();
    connect(&groups_, &GroupModel::currentChanged, this, &MainWindow::onCurrentGroupChanged);
    connectGroupSignals();

    createIconButtons();

    addWindow_ = new PhraseEditorWindow(PhraseEditorWindow::Mode::Add, this, hotkeys_);
    editWindow_ = new PhraseEditorWindow(PhraseEditorWindow::Mode::Edit, this, hotkeys_);
    addAuxiliaryWindow(addWindow_);
    addAuxiliaryWindow(editWindow_);

    phraseMenu_.setStyleSheet(StyleSheets::kApp); // 没有父对象，样式表得自己挂
    pickTextAction_ = phraseMenu_.addAction("选取文字");
    editAction_ = phraseMenu_.addAction("修改");
    addAfterAction_ = phraseMenu_.addAction("在当前短语后添加短语");
    moveMenu_ = phraseMenu_.addMenu("移动到分组"); // 子菜单是 phraseMenu_ 的子对象，样式表跟着继承
    deleteAction_ = phraseMenu_.addAction("删除");
    list_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list_, &QWidget::customContextMenuRequested, this, &MainWindow::showPhraseContextMenu);

    qApp->installEventFilter(this);
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint); // 只留关闭按钮
    if (Settings::alwaysOnTop()) setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    applySize();
}

MainWindow::~MainWindow() {
    qApp->removeEventFilter(this);
    delete addWindow_;
    delete editWindow_;
    if (instance_ == this) instance_ = nullptr;
}

MainWindow *MainWindow::instance() {
    return instance_;
}

PhraseList *MainWindow::phraseList() const {
    return list_;
}
GroupModel &MainWindow::groups() const {
    return groups_;
}
GroupPanel *MainWindow::groupPanel() const {
    return groupPanel_;
}
PhraseNavigator *MainWindow::navigator() const {
    return navigator_;
}
QLineEdit *MainWindow::searchBox() const {
    return search_;
}
HotkeyRegistry &MainWindow::hotkeys() const {
    return hotkeys_;
}

void MainWindow::createIconButtons() {
    auto makeButton = [this](const QString &icon, const QString &toolTip) {
        auto *button = new QPushButton("", this);
        button->setObjectName("iconButton");
        button->setIcon(QIcon(Paths::icon(icon)));
        button->setIconSize(QSize(20, 20));
        button->setToolTip(toolTip);
        return button;
    };
    settingsButton_ = makeButton("settings.svg", "设置");
    connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::settingsRequested);

    addButton_ = makeButton("add.svg", "添加短语");
    connect(addButton_, &QPushButton::clicked, this, [this]() {
        addWindow_->openForAdd(groups_.currentName(), -1);
    });

    pinButton_ = makeButton(Settings::pinned() ? "pin_on.svg" : "pin_off.svg", "钉住/取消钉住窗口（ ` ）");
    connect(pinButton_, &QPushButton::clicked, this, &MainWindow::togglePinned);
}

void MainWindow::connectGroupSignals() {
    connect(groupPanel_, &GroupPanel::newGroupRequested, this, &MainWindow::createGroup);
    connect(groupPanel_, &GroupPanel::editGroupRequested, this, &MainWindow::editGroup);
    connect(groupPanel_, &GroupPanel::batchEditRequested, this, [this](int index) {
        BatchEdit::start(this, index);
    });
    connect(groupPanel_, &GroupPanel::batchAddRequested, this, [this](int index) {
        BatchEdit::startAdd(this, index);
    });
    connect(groupPanel_, &GroupPanel::deleteGroupRequested, this, &MainWindow::deleteGroup);
}

void MainWindow::applySize() {
    const int w = Settings::mainWidth();
    const int h = Settings::mainHeight();
    setFixedSize(w, h);
    list_->move(0, 46);
    list_->setFixedSize(w, h - 40); // 底下 6px 放到窗口外藏起来，水平滚动条才不难看
    groupButton_->move(5, 5);
    // 和右边图标按钮一样高；窄窗口时让出右上角搜索框、三个图标按钮和间距
    groupButton_->setFixedSize(qMin(GroupPanel::buttonMaxWidth(), w - 220), 36);
    groupPanel_->refreshButtonText();
    search_->move(w - 205, 7);
    search_->setFixedSize(90, 35);
    settingsButton_->move(w - 41, 5);
    settingsButton_->setFixedSize(36, 36);
    addButton_->move(w - 77, 5);
    addButton_->setFixedSize(36, 36);
    pinButton_->move(w - 113, 5);
    pinButton_->setFixedSize(36, 36);
}

void MainWindow::applyScrollStep() {
    list_->verticalScrollBar()->setSingleStep(Settings::scrollStep());
}

void MainWindow::reapplyPhraseLayout() {
    const int index = groups_.currentIndex();
    list_->applyLayout(groups_.itemHeight(index), groups_.columns(index));
}

void MainWindow::applyAlwaysOnTop() {
    if (Settings::alwaysOnTop()) setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    else setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    summon(); // 改窗口标志后窗口会被隐藏，要重新显示
}

void MainWindow::refreshFilter() {
    list_->applyFilter(groups_.currentName(), search_->text());
}

void MainWindow::commitPhraseChanges() {
    PhraseStore::save(*list_, groups_);
    hotkeys_.rebuildPhraseHotkeys(*list_);
    refreshFilter();
}

// 分组内按列表顺序显示，插在这一行就是该分组的最后一条
int MainWindow::rowAfterGroup(const QString &group) const {
    for (int i = list_->count() - 1; i >= 0; --i) {
        if (PhraseItem::group(list_->item(i)) == group) return i + 1;
    }
    return list_->count();
}

void MainWindow::movePhraseToGroup(QListWidgetItem *item, const QString &group) {
    if (!groups_.contains(group)) return;
    list_->takeItem(list_->row(item)); // 只是从列表里摘下来，条目对象还在，修改窗口手里的指针照样有效
    PhraseItem::setGroup(item, group);
    list_->insertItem(rowAfterGroup(group), item);
    commitPhraseChanges(); // 行号变了，短语快捷键必须重建
}

void MainWindow::discardPhrase(QListWidgetItem *item) {
    editWindow_->forgetItem(item);
    delete item;
}

void MainWindow::appendPhrasesToGroup(const QString &group, const QStringList &texts) {
    if (!groups_.contains(group) || texts.isEmpty()) return;
    int row = rowAfterGroup(group);
    for (const QString &text : texts)
        list_->insertItem(row++, PhraseItem::create(text, QString(), group, QString()));
    commitPhraseChanges();
}

QString MainWindow::addPhraseToCurrentGroup(const QString &text) {
    const QString group = groups_.currentName();
    list_->insertItem(rowAfterGroup(group), PhraseItem::create(text, QString(), group, QString()));
    commitPhraseChanges();
    return group;
}

void MainWindow::onCurrentGroupChanged(int index) {
    if (index >= 0 && index < groups_.count()) {
        list_->applyFilter(groups_.name(index), search_->text());
        list_->applyLayout(groups_.itemHeight(index), groups_.columns(index));
        list_->setCurrentItem(nullptr); // 切到空分组时不能还选着上一个分组里已隐藏的条目
        list_->selectFirstVisible();
    }
    groupPanel_->refreshButtonText();
    // 只挪面板的选中标记，不重建：点面板切分组就是从面板的 itemClicked 一路调过来的，
    // 当场删掉发信号的条目会让 Qt 踩空。分组增删改名排序时另外调 rebuild
    if (groupPanel_->isVisible() && groupPanel_->rowCount() == groups_.count()) groupPanel_->syncSelection();
}

void MainWindow::createGroup(int position) {
    QString name;
    int itemHeight = Settings::defaultItemHeight();
    int columns = 1;
    if (!GroupDialog::exec(this, "新建分组", name, itemHeight, columns) || name.isEmpty()) return;
    if (groups_.contains(name)) {
        QMessageBox::warning(this, "分组名重复", "该分组名已存在，请使用其他名称  ");
        return;
    }
    const int index = position < 0 ? groups_.count() : position;
    groups_.insert(index, {name, itemHeight, columns});
    groups_.setCurrentIndex(index);
    PhraseStore::save(*list_, groups_);
    groupPanel_->rebuild();
}

void MainWindow::editGroup(int index) {
    const QString oldName = groups_.name(index);
    QString newName = oldName;
    int itemHeight = groups_.itemHeight(index);
    int columns = groups_.columns(index);
    if (!GroupDialog::exec(this, "修改分组", newName, itemHeight, columns) || newName.isEmpty()) return;
    if (newName != oldName && groups_.contains(newName)) {
        QMessageBox::warning(this, "分组名重复", "该分组名已存在，请使用其他名称  ");
        return;
    }
    groups_.rename(index, newName);
    groups_.setLayout(index, itemHeight, columns);
    if (index == groups_.currentIndex()) list_->applyLayout(itemHeight, columns);
    // 短语按分组名关联，改名要把所有短语一起改过去
    for (int i = 0; i < list_->count(); ++i) {
        if (PhraseItem::group(list_->item(i)) == oldName) PhraseItem::setGroup(list_->item(i), newName);
    }
    PhraseStore::save(*list_, groups_); // 分组和短语在同一个文件里，一次写完，不会出现改了一半的分组名
    refreshFilter();
    groupPanel_->refreshButtonText();
    groupPanel_->rebuild();
}

void MainWindow::deleteGroup(int index) {
    if (groups_.count() <= 1) {
        QMessageBox::warning(this, "提示", "至少需要保留一个分组  ");
        return;
    }
    const QString name = groups_.name(index);
    if (QMessageBox::question(this, "确认删除", "确定要删除分组吗？该分组下所有短语也会被删除  ") != QMessageBox::Yes) return;
    groups_.remove(index);
    for (int i = list_->count() - 1; i >= 0; --i) {
        QListWidgetItem *item = list_->item(i);
        if (PhraseItem::group(item) != name) continue;
        discardPhrase(item);
    }
    commitPhraseChanges();
    groupPanel_->refreshButtonText();
    groupPanel_->rebuild();
}

void MainWindow::showPhraseContextMenu(const QPoint &pos) {
    QListWidgetItem *item = list_->itemAt(pos);
    if (!item) return;
    // 子菜单每次现建：分组随时会增删改名。列出除本分组以外的所有分组
    moveMenu_->clear();
    for (int i = 0; i < groups_.count(); ++i) {
        const QString name = groups_.name(i);
        if (name != PhraseItem::group(item)) moveMenu_->addAction(QString(name).replace('&', "&&"))->setData(name); // & 在菜单里是助记符
    }
    moveMenu_->setEnabled(!moveMenu_->isEmpty());
    QAction *chosen = phraseMenu_.exec(list_->mapToGlobal(pos));
    if (chosen && chosen->parent() == moveMenu_) {
        movePhraseToGroup(item, chosen->data().toString());
    } else if (chosen == pickTextAction_) {
        // 弹在右键的位置；选中一段、松开鼠标就输出那一段
        SelectTextPopup::show(PhraseItem::text(item), list_->mapToGlobal(pos), [this](const QString &picked) {
            outputText(picked);
        });
    } else if (chosen == editAction_) {
        editWindow_->openForEdit(item);
    } else if (chosen == addAfterAction_) {
        addWindow_->openForAdd(PhraseItem::group(item), list_->row(item));
    } else if (chosen == deleteAction_) {
        discardPhrase(item);
        commitPhraseChanges();
    }
}

void MainWindow::togglePinned() {
    Settings::setPinned(!Settings::pinned());
    Settings::save();
    updatePinButton();
}

void MainWindow::updatePinButton() {
    pinButton_->setIcon(QIcon(Paths::icon(Settings::pinned() ? "pin_on.svg" : "pin_off.svg")));
}

void MainWindow::outputPhrase(QListWidgetItem *item) {
    outputText(PhraseItem::text(item));
}

void MainWindow::outputText(const QString &text) {
    // 搜索模式下前台是 QuickSay 自己，不先把前台还回去就粘进搜索框了（右键“选取文字”就会走到这里）
    if (searchMode_) leaveSearchMode();
    if (!Settings::pinned()) close();
    OutputRunner::start(text);
}

void MainWindow::closeToTray() {
    if (QWidget *popup = QApplication::activePopupWidget()) popup->close();
    close();
}

void MainWindow::summon() {
    move(Settings::windowPos(WindowSlot::Main));
    showWithoutActivating();
}

void MainWindow::summonByUser() {
    if (BatchEdit::isActive()) return; // 别把锁住的主窗口盖到记事本上面
    summon();
    if (Settings::searchOnSummon()) enterSearchMode();
}

void MainWindow::showWithoutActivating() {
    searchMode_ = false;
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    WindowStyle::addNoActivate(winId());
    if (!isVisible()) show();
    WindowStyle::addNoActivate(winId());
    WindowStyle::showNoActivate(winId(), Settings::alwaysOnTop());
    // 主窗口已显示只是被盖住时不会再有 Show 事件；常显的面板是独立顶层窗口，得在这里一起提上来
    if (Settings::groupPanelAlwaysVisible()) groupPanel_->show();
    BrowseHook::start();
}

void MainWindow::toggleFromHotkey() {
    if (BatchEdit::isActive()) return; // 别把锁住的主窗口盖到记事本上面
    // 置顶时窗口显示着就必然在最前
    if (isVisible() && (Settings::alwaysOnTop() || WindowStyle::isFrontmost(this))) closeToTray();
    else summonByUser();
}

bool MainWindow::isSearchMode() const {
    return searchMode_;
}

void MainWindow::enterSearchMode() {
    searchMode_ = true; // 搜索期间仍可用分组面板，切分组后搜索词继续过滤新分组
    foregroundBeforeSearch_ = GetForegroundWindow();
    BrowseHook::stop();
    WindowStyle::setNoActivate(winId(), false);
    if (!isVisible()) show();
    raise();
    activateWindow();
    search_->setFocus(Qt::ShortcutFocusReason);
    search_->selectAll();
}

void MainWindow::leaveSearchMode(bool restorePreviousFocus) {
    searchMode_ = false;
    search_->clearFocus();
    WindowStyle::setNoActivate(winId(), true);
    WindowStyle::addNoActivate(winId());
    BrowseHook::start();
    HWND previous = static_cast<HWND>(foregroundBeforeSearch_);
    if (restorePreviousFocus && previous && IsWindow(previous)) SetForegroundWindow(previous);
    foregroundBeforeSearch_ = nullptr;
    groupPanel_->applyAlwaysVisible(); // 主窗口状态变过，按常显开关把面板摆回来
}

void MainWindow::addAuxiliaryWindow(QWidget *window) {
    auxiliaryWindows_.append(window);
}

bool MainWindow::hasBlockingWindow() const {
    if (TrayMenu::isOpen() || BatchEdit::isActive()) return true;
    if (QApplication::activeModalWidget() || QApplication::activePopupWidget()) return true;
    for (const QPointer<QWidget> &window : auxiliaryWindows_) {
        if (window && window->isVisible()) return true;
    }
    return false;
}

void MainWindow::setAllWindowsEnabled(bool enabled) {
    setEnabled(enabled);
    for (const QPointer<QWidget> &window : auxiliaryWindows_) {
        if (window) window->setEnabled(enabled);
    }
}

void MainWindow::moveEvent(QMoveEvent *event) {
    QWidget::moveEvent(event);
    if (!Settings::writesFrozen()) {
        Settings::setWindowPos(WindowSlot::Main, pos());
        Settings::saveLater();
    }
    // 面板是贴着主窗口外沿的独立窗口，主窗口被拖走时它不能留在原地
    if (!groupPanel_) return;
    if (Settings::groupPanelAlwaysVisible() && isVisible()) groupPanel_->show();
    else groupPanel_->hide();
}

void MainWindow::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    QToolTip::hideText();
    if (groupPanel_) groupPanel_->hide();
}

void MainWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    // 刚显示时位置大小还没定下来，等事件循环转一圈再摆面板
    QTimer::singleShot(0, this, [this]() {
        groupPanel_->applyAlwaysVisible();
    });
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == search_ && event->type() == QEvent::FocusOut && searchMode_) {
        QTimer::singleShot(0, this, [this]() {
            if (searchMode_ && !search_->hasFocus() && !isActiveWindow()) leaveSearchMode(false);
        });
    }
    if (event->type() == QEvent::KeyPress && handleKeyPress(static_cast<QKeyEvent *>(event))) return true;
    return QWidget::eventFilter(watched, event);
}

// 主窗口真正获得焦点时（主要是搜索模式）的按键处理。平时窗口不抢焦点，按键走 BrowseHook。
bool MainWindow::handleKeyPress(QKeyEvent *event) {
    const int key = event->key();
    const bool isEnter = key == Qt::Key_Return || key == Qt::Key_Enter;
    // 只看 searchMode_，不能再加 search_->hasFocus()：搜索框被 winId() 变成原生子窗口后，系统键盘焦点在它的 HWND 上时
    // Qt 按 HWND 把按键直接派给它，QApplication::focusWidget() 却未必是它，hasFocus() 为假就会掉进下面的浏览分支，Tab/Esc/回车被当成非搜索模式处理
    if (searchMode_) {
        if (!isActiveWindow() || QApplication::activePopupWidget()) return false; // 右键菜单、刚打开的修改窗口之类的按键别截
        if (key == Qt::Key_Escape) {
            if (!search_->text().isEmpty()) search_->clear();
            leaveSearchMode();
            return true;
        }
        if (key == Qt::Key_Tab) {
            leaveSearchMode();
            return true;
        }
        if (isEnter) {
            QListWidgetItem *item = list_->currentItem();
            leaveSearchMode();
            if (item) outputPhrase(item);
            return true;
        }
        if (key == Qt::Key_Up || key == Qt::Key_Down) {
            navigator_->moveVertical(key == Qt::Key_Up ? -1 : 1);
            return true;
        }
        // ←→ 也拿来切短语/翻组，不在搜索框里挪光标
        if (key == Qt::Key_Left || key == Qt::Key_Right) {
            navigator_->moveHorizontal(key == Qt::Key_Left ? -1 : 1);
            return true;
        }
        return false;
    }

    if (!isActiveWindow()) return false;
    switch (key) {
    case Qt::Key_Tab:
        if (search_->hasFocus()) list_->setFocus();
        else search_->setFocus();
        return true;
    case Qt::Key_Escape:
        closeToTray();
        return true;
    case Qt::Key_Left:
    case Qt::Key_Right:
        if (search_->hasFocus()) list_->setFocus();
        navigator_->moveHorizontal(key == Qt::Key_Left ? -1 : 1);
        return true;
    case Qt::Key_Up:
    case Qt::Key_Down:
        if (search_->hasFocus()) list_->setFocus();
        navigator_->moveVertical(key == Qt::Key_Up ? -1 : 1);
        return true;
    default:
        break;
    }
    if (isEnter) {
        if (search_->hasFocus()) {
            list_->setFocus();
            return true;
        }
        if (QListWidgetItem *item = list_->currentItem()) {
            outputPhrase(item);
            return true;
        }
    }
    if (search_->hasFocus()) return false;

    int number = -1;
    if (key >= Qt::Key_1 && key <= Qt::Key_9) number = key - Qt::Key_0;
    else if (key == Qt::Key_0) number = 10;
    else if (key >= Qt::Key_A && key <= Qt::Key_Z) number = key - Qt::Key_A + 11;
    if (number < 0) return false;
    if (QListWidgetItem *item = list_->visibleItemAt(number)) {
        outputPhrase(item);
        return true;
    }
    return false;
}

namespace {

HHOOK g_hook = nullptr;

bool isKeyDown(int vk) {
    return GetAsyncKeyState(vk) & 0x8000;
}

// 返回 true 表示按键已处理、要吞掉
bool handleKey(DWORD vk) {
    MainWindow *window = MainWindow::instance();
    if (!window || window->isSearchMode() || !window->isVisible() || window->hasBlockingWindow()) return false;

    const bool ctrl = isKeyDown(VK_CONTROL);
    const bool shift = isKeyDown(VK_SHIFT);
    const bool alt = isKeyDown(VK_MENU);
    const bool win = isKeyDown(VK_LWIN) || isKeyDown(VK_RWIN);
    const bool hasSystemModifier = ctrl || alt || win;
    const bool ctrlOnly = ctrl && !shift && !alt && !win; // 设置里三个“允许按下 Ctrl+…”认的就是这种组合
    const bool pinned = Settings::pinned();

    // 用户自己设的全局快捷键优先：比如呼出快捷键设成 Ctrl+`，不放行的话窗口显示时会被下面当成反引号切换钉住吞掉，
    // 窗口就关不掉了（RegisterHotKey 排在低级钩子后面，钩子吞掉的键它收不到）。
    // 走 HotkeyHook 的方向键、回车快捷键不在此列，它们本来就设计成主窗口显示时让给键盘浏览
    if (window->hotkeys().isSystemHotkey(vk, ctrl, shift, alt, win)) return false;

    const bool isArrow = vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN;
    if (isArrow && pinned && Settings::arrowKeyPassthroughWhenPinned() && !(ctrlOnly && Settings::arrowKeyCtrlWhenPinned())) return false;

    PhraseNavigator *navigator = window->navigator();
    PhraseList *list = window->phraseList();
    switch (vk) {
    case VK_TAB:
        window->enterSearchMode();
        return true;
    case VK_OEM_3: // 反引号：切换钉住。主窗口显示时这个键会被吞掉，打不出反引号
        window->togglePinned();
        return true;
    case VK_ESCAPE:
        window->closeToTray();
        return true;
    case VK_RETURN:
        if (pinned && Settings::enterKeyPassthroughWhenPinned() && !(ctrlOnly && Settings::enterKeyCtrlWhenPinned())) return false;
        if (QListWidgetItem *item = list->currentItem()) {
            window->outputPhrase(item);
            return true;
        }
        return false;
    case VK_LEFT:
        navigator->moveHorizontal(-1);
        return true;
    case VK_RIGHT:
        navigator->moveHorizontal(1);
        return true;
    case VK_UP:
        navigator->moveVertical(-1);
        return true;
    case VK_DOWN:
        navigator->moveVertical(1);
        return true;
    case VK_HOME:
        navigator->selectEdge(1);
        return true;
    case VK_END:
        navigator->selectEdge(-1);
        return true;
    default:
        break;
    }

    int number = -1;
    if (vk >= '1' && vk <= '9') number = vk - '0';
    else if (vk == '0') number = 10;
    else if (vk >= 'A' && vk <= 'Z') number = vk - 'A' + 11;
    if (number < 0) return false;

    // 钉住且角标键放行时，开了“允许 Ctrl+角标”就只剩 Ctrl+角标这一种输出方式；平时角标键不能带修饰键
    const bool badgePassthrough = pinned && Settings::badgeKeyPassthroughWhenPinned();
    const bool ctrlBadge = ctrlOnly && badgePassthrough && Settings::badgeKeyCtrlWhenPinned();
    if (hasSystemModifier && !ctrlBadge) return false;
    if (badgePassthrough && !ctrlBadge) return false;
    if (QListWidgetItem *item = list->visibleItemAt(number)) {
        window->outputPhrase(item);
        return true;
    }
    return false;
}

LRESULT CALLBACK hookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        const auto *info = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if (!KeyInjector::isOwnInjection(info->flags, info->dwExtraInfo) && handleKey(info->vkCode)) return 1;
    }
    return CallNextHookEx(g_hook, code, wParam, lParam);
}

} // namespace

namespace BrowseHook {

void start() {
    MainWindow *window = MainWindow::instance();
    if (g_hook || (window && window->isSearchMode())) return;
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, hookProc, GetModuleHandleW(nullptr), 0);
}

void stop() {
    if (!g_hook) return;
    UnhookWindowsHookEx(g_hook);
    g_hook = nullptr;
}

void reinstallIfActive() {
    if (!g_hook) return;
    stop();
    start();
}

} // namespace BrowseHook
