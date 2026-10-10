#pragma once

#include <QListWidget>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

#include <functional>

// 主窗口的短语列表。条目本身就是短语数据（见 phraseitem.h），这里负责显示：
// 按分组和搜索词过滤并编角标、按分组设置排成一列或多列、画角标、悬停提示。
class PhraseList : public QListWidget {
  public:
    explicit PhraseList(QWidget *parent);

    // 过滤/编角标的唯一入口。任何会改变可见条目集合的操作（切分组、搜索、拖动、增删改）之后都必须调一次，否则角标错位。
    // 可见条目依次编为 1~9、0、A~Z，超过 36 条不显示角标。
    void applyFilter(const QString &group, const QString &keyword);

    // 应用某个分组的行高和列数
    void applyLayout(int itemHeight, int columns);

    // 多列时每格的宽度；0 表示单列，宽度交给列表自己撑满
    int cellWidth() const;

    // 按界面顺序（从左到右、从上到下）排列的可见条目
    QVector<QListWidgetItem *> visibleItems() const;
    // number 从 1 开始，对应角标 1~9、0（第 10 条）、A~Z（第 11~36 条）
    QListWidgetItem *visibleItemAt(int number) const;
    void selectFirstVisible();

  private:
    friend class PhraseListResizeFilter;
    void updateColumns(int columns);

    int cellWidth_ = 0;
    int columns_ = 1;
};

// 选中 item，只在它越过上下安全区（各 safePixels 像素）时才滚动列表，不每次都强制居中。
// direction：-1 往上、1 往下。短语列表的上下浏览和分组面板翻组共用这一套，安全区大小分开设。
void selectWithSafeArea(QListWidget *list, QListWidgetItem *item, int direction, int safePixels);

class GroupModel;
class GroupPanel;
class PhraseList;

// 方向键浏览短语。短语可以排成多列，所以“上下左右”按可见条目的序号换算成（行，列）来算。
//
// 翻组惯性：左右键在行首/行尾切过一次分组后，接下来左右键一律切分组，不再在行内移动；
// 停手 Settings::groupPanelDelayMs() 后自动消失，按上下键立刻消失。停在第一个或最后一个分组时
// 惯性挂起但不清除（那两个分组只有一边能翻，按正常行内移动处理），切回中间分组还能接着连续翻。
class PhraseNavigator : public QObject {
  public:
    PhraseNavigator(PhraseList *list, GroupModel &groups, QObject *parent);
    void setGroupPanel(GroupPanel *panel);

    // direction：-1 是 ↑/←，1 是 ↓/→
    void moveVertical(int direction);
    void moveHorizontal(int direction);
    // Home/End：选中第一个/最后一个可见条目
    void selectEdge(int direction);

    bool groupInertiaActive() const;
    void clearGroupInertia();

  private:
    void switchGroup(int direction);
    void selectVisibleWithSafeArea(int index, int direction);

    PhraseList *list_;
    GroupModel &groups_;
    GroupPanel *panel_ = nullptr;
    bool groupInertia_ = false;
    QTimer inertiaTimer_;
};

class QListWidget;
class QListWidgetItem;

// 给不抢焦点的列表（主窗口短语列表、分组面板）装上条目悬停提示。
// 这两个窗口都带 WS_EX_NOACTIVATE，Qt 默认只给激活窗口发 ToolTip 事件，所以自己盯鼠标移动：
// 停在同一个条目上 Settings::toolTipDelayMs() 毫秒后显示该条目的 toolTip()，为空就不显示。
// textFor 不为空时，真正显示前改用它算出提示内容（只在显示那一刻调一次，可以做读文件这类慢一点的事）；
// 条目的 toolTip() 仍决定“有没有提示”。
void installListToolTip(QListWidget *list, std::function<QString(QListWidgetItem *)> textFor = {});
