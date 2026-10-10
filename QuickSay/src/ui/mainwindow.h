#pragma once

#include <QList>
#include <QMenu>
#include <QPointer>
#include <QWidget>

class GroupButton;
class GroupModel;
class GroupPanel;
class HotkeyRegistry;
class PhraseEditorWindow;
class PhraseList;
class PhraseNavigator;
class QLineEdit;
class QListWidgetItem;
class QPushButton;

// 主窗口。默认不抢前台焦点（WS_EX_NOACTIVATE + SW_SHOWNOACTIVATE），点短语就能直接粘进前台程序；
// 代价是收不到正常的键盘事件，键盘浏览靠低级键盘钩子（见 BrowseHook）。
// 搜索框是唯一例外：进入搜索模式时摘掉 NOACTIVATE、卸掉钩子、真正激活窗口，退出时反向恢复。
// 以后再加需要真正打字的控件也得走这一套。
class MainWindow : public QWidget {
    Q_OBJECT

  public:
    MainWindow(GroupModel &groups, HotkeyRegistry &hotkeys);
    ~MainWindow() override;

    static MainWindow *instance();

    PhraseList *phraseList() const;
    GroupModel &groups() const;
    GroupPanel *groupPanel() const;
    PhraseNavigator *navigator() const;
    QLineEdit *searchBox() const;
    HotkeyRegistry &hotkeys() const;

    // 移到记住的位置并显示（不激活）
    void summon();
    void showWithoutActivating();
    // 呼出快捷键：没显示或被盖住就呼出，已在最前就收起
    void toggleFromHotkey();
    // 关闭可能开着的右键菜单，再收起到托盘
    void closeToTray();
    void outputPhrase(QListWidgetItem *item);
    void togglePinned();

    bool isSearchMode() const;
    void enterSearchMode();
    void leaveSearchMode(bool restorePreviousFocus = true);

    // 设置、添加、修改窗口：它们开着时键盘钩子要放行按键，批量编辑时要一起锁住。添加/修改窗口在构造时已经加进来了
    void addAuxiliaryWindow(QWidget *window);
    // 有模态框、弹出菜单、原生托盘菜单、辅助窗口开着，或者正在模拟按键/批量编辑时，键盘钩子放行一切按键
    bool hasBlockingWindow() const;
    void setAllWindowsEnabled(bool enabled);

    // 增删改短语之后统一收尾：落盘、重建短语快捷键、重新过滤
    void commitPhraseChanges();
    // 在当前分组末尾加一条只有正文的短语，返回分组名
    QString addPhraseToCurrentGroup(const QString &text);
    void refreshFilter();

    // 设置窗口应用设置时用
    void applySize();
    void applyScrollStep();
    void reapplyPhraseLayout();
    void applyAlwaysOnTop();

  signals:
    void settingsRequested();

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;

  private:
    void createIconButtons();
    void connectGroupSignals();
    void onCurrentGroupChanged(int index);
    void showPhraseContextMenu(const QPoint &pos);
    void createGroup(int position);
    void editGroup(int index);
    void deleteGroup(int index);
    void updatePinButton();
    bool handleKeyPress(QKeyEvent *event);

    static MainWindow *instance_;

    GroupModel &groups_;
    HotkeyRegistry &hotkeys_;
    PhraseList *list_ = nullptr;
    PhraseNavigator *navigator_ = nullptr;
    GroupButton *groupButton_ = nullptr;
    GroupPanel *groupPanel_ = nullptr;
    QLineEdit *search_ = nullptr;
    QPushButton *settingsButton_ = nullptr;
    QPushButton *addButton_ = nullptr;
    QPushButton *pinButton_ = nullptr;
    PhraseEditorWindow *addWindow_ = nullptr;
    PhraseEditorWindow *editWindow_ = nullptr;
    QMenu phraseMenu_;
    QAction *editAction_ = nullptr;
    QAction *addAfterAction_ = nullptr;
    QAction *deleteAction_ = nullptr;
    QList<QPointer<QWidget>> auxiliaryWindows_;

    bool searchMode_ = false;
    void *foregroundBeforeSearch_ = nullptr; // HWND
};

// 主窗口显示时用低级键盘钩子做键盘浏览（主窗口不抢焦点，收不到正常按键）：
// 角标键输出对应短语，↑↓←→ 浏览，Enter 输出选中项，Esc 收起，` 切换钉住，Tab 进入搜索。
// 钉住窗口时角标键/回车/方向键可以按设置放行给前台程序。
namespace BrowseHook {

// 搜索模式下不装
void start();
void stop();
// 用户快捷键钩子重装后跟着重装，重新排回它前面（低级钩子后装的先执行）。本来没装就保持不装。
void reinstallIfActive();

} // namespace BrowseHook
