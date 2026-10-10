#pragma once

#include <QListWidget>
#include <QPushButton>
#include <QString>
#include <QTimer>

class GroupModel;
class MainWindow;
class QVariantAnimation;

// 主窗口左上角“箭头+当前分组名”的按钮。鼠标移上去弹出分组面板，箭头转 90° 指向面板（面板通常在左边，主窗口贴着屏幕左边时在右边）。
class GroupButton : public QPushButton {
    Q_OBJECT

  public:
    explicit GroupButton(QWidget *parent);
    // 0° 朝下（面板收着），90° 朝左、-90° 朝右（面板弹出）
    void rotateArrowTo(qreal degrees);

  signals:
    void hovered();
    void wheelScrolled(int delta);

  protected:
    void enterEvent(QEnterEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

  private:
    QVariantAnimation *animation_ = nullptr;
    qreal angle_ = 0;
};

// 贴在主窗口外侧、竖着列出所有分组的独立顶层窗口。
// 必须是普通窗口而不能是 Qt::Popup：弹出窗口会让键盘钩子整个放行，方向键、角标键、Esc 就都失灵了。
class GroupPanelWindow : public QListWidget {
    Q_OBJECT

  public:
    GroupPanelWindow();
    // 给窗口偷偷加上标题栏样式（再在 WM_NCCALCSIZE 里把标题栏吃掉），收起时才有系统原生的关闭动画
    void prepareNativeFrame();

  signals:
    void wheelScrolled(int delta);

  protected:
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void wheelEvent(QWheelEvent *event) override;
};

// 分组按钮和分组面板的行为：悬停弹出、移开延时收起、跟随当前分组、拖动排序、右键菜单。
// 面板该不该留着只看两条：鼠标悬在按钮或面板上，或者翻组惯性还没过。
class GroupPanel : public QObject {
    Q_OBJECT

  public:
    GroupPanel(MainWindow *mainWindow, GroupModel &groups, GroupButton *button);
    ~GroupPanel() override;

    void show();
    void hide();
    void hideIfIdle();
    // 让面板跟上“始终显示分组面板”开关
    void applyAlwaysVisible();
    // 按分组列表重建面板内容（分组增删、改名、排序之后调用）
    void rebuild();
    // 把当前分组标成选中，并按安全区规则滚动面板；只挪选中标记，不重建
    void syncSelection();
    void refreshButtonText();

    bool isVisible() const;
    int rowCount() const;
    static int buttonMaxWidth();
    // 只查不建：面板还没建出系统窗口时返回 0
    WId existingWinId() const;

  signals:
    void newGroupRequested(int position); // position < 0 表示加在末尾
    void editGroupRequested(int index);
    void batchEditRequested(int index);
    void batchAddRequested(int index);
    void deleteGroupRequested(int index);

  private:
    void switchByWheel(int delta);
    void pollHover();
    void startHoverPolling();
    void showContextMenu(QWidget *anchor, const QPoint &pos, int index, bool insertAfterIndex);
    void onRowsMoved(int start, int row);

    MainWindow *mainWindow_;
    GroupModel &groups_;
    GroupButton *button_;
    GroupPanelWindow *window_;
    QTimer hoverPoll_;
    QTimer hideTimer_;
    bool hovering_ = false;
    bool reordering_ = false; // 拖动排序进行中，不许重建列表
};

class QWidget;

namespace GroupDialog {

// 新建/修改分组对话框：分组名称、行高、列数。点“确定”返回 true 并写回三个参数。
bool exec(QWidget *parent, const QString &title, QString &name, int &itemHeight, int &columns);

} // namespace GroupDialog
