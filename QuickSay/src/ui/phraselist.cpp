#include "ui/phraselist.h"

#include "core/groupmodel.h"
#include "core/phrase.h"
#include "core/settings.h"
#include "ui/grouppanel.h"

#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolTip>

namespace {

class BadgeDelegate : public QStyledItemDelegate {
  public:
    explicit BadgeDelegate(PhraseList *list) : QStyledItemDelegate(list), list_(list) {}

    // 宽度不用默认值：默认 sizeHint 会按最后一条短语估算内容宽度，一长串没有断点的文字会把所有条目撑宽、
    // 不再自动换行还冒出水平滚动条。单列时给 0（宽度交给列表），多列时给算好的格宽。
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setWidth(list_->cellWidth());
        return size;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt(option);
        const QVariant foreground = index.data(Qt::ForegroundRole);
        if ((opt.state & QStyle::State_Selected) && foreground.canConvert<QBrush>()) {
            opt.palette.setBrush(QPalette::HighlightedText, foreground.value<QBrush>()); // 选中时仍用条目自己的字色
        }
        QStyledItemDelegate::paint(painter, opt, index);

        const QString badge = index.data(PhraseRole::Badge).toString();
        if (badge.isEmpty()) return;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        QFont font = painter->font();
        font.setPixelSize(10);
        font.setBold(true);
        painter->setFont(font);
        painter->setBrush(QColor("#0078d4"));
        painter->setPen(Qt::NoPen);
        const QRect rect = Settings::badgeTopLeft()
            ? QRect(option.rect.left() + 4, option.rect.top() + 4, 18, 18)
            : QRect(option.rect.right() - 18 - 5, option.rect.top() + 5, 18, 18);
        painter->drawRoundedRect(rect, 5, 5);
        painter->setPen(Qt::white);
        painter->drawText(rect, Qt::AlignCenter, badge);
        painter->restore();
    }

  private:
    PhraseList *list_;
};

// 主窗口不抢焦点，Qt 默认的条目悬停提示不会触发，所以自己处理：停留 250ms 后显示
class ToolTipFilter : public QObject {
  public:
    explicit ToolTipFilter(QListWidget *list) : QObject(list), list_(list) {
        timer_.setSingleShot(true);
        timer_.setInterval(250);
        connect(&timer_, &QTimer::timeout, this, [this]() {
            showForLastItem();
        });
    }

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched != list_->viewport()) return QObject::eventFilter(watched, event);
        switch (event->type()) {
        case QEvent::MouseMove: {
            const QPoint pos = static_cast<QMouseEvent *>(event)->pos();
            QListWidgetItem *item = list_->itemAt(pos);
            if (!item || item->isHidden() || item->toolTip().isEmpty()) {
                hide();
                return false;
            }
            if (item != lastItem_ || pos != lastViewportPos_) {
                QToolTip::hideText();
                lastItem_ = item;
                lastViewportPos_ = pos;
                lastGlobalPos_ = list_->viewport()->mapToGlobal(pos);
                timer_.start();
            }
            return false;
        }
        case QEvent::ToolTip:
            if (!showForLastItem()) hide();
            return true;
        case QEvent::Leave:
        case QEvent::Wheel:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
            hide();
            break;
        default:
            break;
        }
        return QObject::eventFilter(watched, event);
    }

  private:
    bool showForLastItem() {
        if (!lastItem_ || lastItem_->isHidden() || lastItem_->toolTip().isEmpty()) return false;
        const QRect rect = list_->visualItemRect(lastItem_);
        if (!rect.contains(lastViewportPos_)) return false;
        QToolTip::showText(lastGlobalPos_, lastItem_->toolTip(), list_->viewport(), rect);
        return true;
    }

    void hide() {
        timer_.stop();
        lastItem_ = nullptr;
        QToolTip::hideText();
    }

    QListWidget *list_;
    QTimer timer_;
    QPoint lastViewportPos_;
    QPoint lastGlobalPos_;
    QListWidgetItem *lastItem_ = nullptr;
};

} // namespace

// 可视区宽度一变（改窗口大小、滚动条出现/消失）就重算格宽。不会来回抖：一行几个是固定的，总高度不随格宽变。
class PhraseListResizeFilter : public QObject {
  public:
    explicit PhraseListResizeFilter(PhraseList *list) : QObject(list), list_(list) {}

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == list_->viewport() && event->type() == QEvent::Resize) list_->updateColumns(list_->columns_);
        return QObject::eventFilter(watched, event);
    }

  private:
    PhraseList *list_;
};

PhraseList::PhraseList(QWidget *parent) : QListWidget(parent) {
    setWordWrap(true);
    setUniformItemSizes(true); // 所有条目同高才能正常折行，也省掉逐条量高度
    setItemDelegate(new BadgeDelegate(this));
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    viewport()->installEventFilter(new ToolTipFilter(this));
    viewport()->installEventFilter(new PhraseListResizeFilter(this));

    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropMode(QAbstractItemView::InternalMove);
}

void PhraseList::applyFilter(const QString &group, const QString &keyword) {
    int visibleCount = 0;
    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *it = item(i);
        const bool matchesKeyword = keyword.isEmpty() || PhraseItem::text(it).contains(keyword, Qt::CaseInsensitive) || PhraseItem::remark(it).contains(keyword, Qt::CaseInsensitive);
        if (PhraseItem::group(it) != group || !matchesKeyword) {
            it->setHidden(true);
            it->setData(PhraseRole::Badge, "");
            continue;
        }
        it->setHidden(false);
        ++visibleCount;
        QString badge;
        if (visibleCount <= 9) badge = QString::number(visibleCount);
        else if (visibleCount == 10) badge = "0";
        else if (visibleCount <= 36) badge = QChar('A' + visibleCount - 11);
        it->setData(PhraseRole::Badge, badge);
    }
}

void PhraseList::applyLayout(int itemHeight, int columns) {
    setStyleSheet(QString("QListWidget::item{ height: %1px; padding: %2px %3px; }")
            .arg(itemHeight)
            .arg(Settings::itemPaddingVertical())
            .arg(Settings::itemPaddingHorizontal()));
    updateColumns(columns);
}

int PhraseList::cellWidth() const {
    return cellWidth_;
}

// 多列：改成从左到右排并允许换行，再让每格宽度正好是可视宽度/每行个数，排满一行自然换到下一行。
// 不能用 setGridSize()：网格只管位置，条目大小还是问委托要，结果宽度为 0 什么都画不出来。
void PhraseList::updateColumns(int columns) {
    columns_ = columns;
    int width = 0;
    if (columns > 1) {
        // 列表判断“这一行还放不放得下”用的是 maximumViewportSize() 减滚动条宽度（QListViewPrivate::prepareItemsLayout），
        // 不管滚动条此刻显不显示都会扣掉，所以滚动条出现/消失不影响一行排几个。
        // 再多减 4px：宁可右边留条缝，也不能多算而挤掉一整列。
        const int available = maximumViewportSize().width() - verticalScrollBar()->sizeHint().width() - 4;
        width = (available - 1) / columns; // 列表用 >= 判断放不放得下，正好除尽时最后一个会被挤下去
        if (width < 1) width = 1; // 窗口还没显示时宽度可能是 0
    }
    if (cellWidth_ == width && isWrapping() == (columns > 1)) return;
    cellWidth_ = width;
    setFlow(columns > 1 ? QListView::LeftToRight : QListView::TopToBottom);
    setWrapping(columns > 1);
    doItemsLayout(); // setUniformItemSizes 会缓存条目尺寸，必须强制重新布局
}

QVector<QListWidgetItem *> PhraseList::visibleItems() const {
    QVector<QListWidgetItem *> items;
    for (int i = 0; i < count(); ++i) {
        if (!item(i)->isHidden()) items << item(i);
    }
    return items;
}

QListWidgetItem *PhraseList::visibleItemAt(int number) const {
    int visibleCount = 0;
    for (int i = 0; i < count(); ++i) {
        if (item(i)->isHidden()) continue;
        if (++visibleCount == number) return item(i);
    }
    return nullptr;
}

void PhraseList::selectFirstVisible() {
    for (int i = 0; i < count(); ++i) {
        if (!item(i)->isHidden()) {
            setCurrentItem(item(i));
            return;
        }
    }
}

void selectWithSafeArea(QListWidget *list, QListWidgetItem *item, int direction) {
    if (!list || !item) return;
    QScrollBar *scrollBar = list->verticalScrollBar();
    const int oldValue = scrollBar->value(); // 某些样式下 setCurrentItem 会顺手滚动，先记住，完全按安全区规则决定滚不滚
    list->setCurrentItem(item);
    if (scrollBar->value() != oldValue) scrollBar->setValue(oldValue);

    // 多列时同一行各项的 top/bottom 相同，所以安全区天然按“行”算
    const QRect rect = list->visualItemRect(item);
    const int itemHeight = rect.height();
    const int viewportHeight = list->viewport()->height();
    if (!rect.isValid() || itemHeight <= 0 || viewportHeight <= 0) { // 布局还没完成
        list->scrollToItem(item, QAbstractItemView::EnsureVisible);
        return;
    }

    // 视口不够高时安全区跟着缩小，至少给选中项自己留出位置
    const int safeHeight = qMin(Settings::scrollSafePixels(), qMax(0, (viewportHeight - itemHeight) / 2));
    const int safeTop = safeHeight;
    const int safeBottom = viewportHeight - safeHeight; // 右开区间，和 bottom()+1 比
    const bool crossedTop = rect.top() < safeTop;
    const bool crossedBottom = rect.bottom() + 1 > safeBottom;

    int newValue = oldValue;
    if (crossedTop && crossedBottom) {
        // 单项比可视区还高：只按移动方向对齐一边，免得在顶/底反复按键时滚动条来回跳
        newValue += direction < 0 ? rect.top() - safeTop : rect.bottom() + 1 - safeBottom;
    } else if (crossedTop) {
        newValue += rect.top() - safeTop;
    } else if (crossedBottom) {
        newValue += rect.bottom() + 1 - safeBottom;
    }
    newValue = qBound(scrollBar->minimum(), newValue, scrollBar->maximum()); // ScrollPerPixel 下滚动值就是像素
    if (newValue != oldValue) scrollBar->setValue(newValue);
}

PhraseNavigator::PhraseNavigator(PhraseList *list, GroupModel &groups, QObject *parent) : QObject(parent), list_(list), groups_(groups) {
    inertiaTimer_.setSingleShot(true);
    connect(&inertiaTimer_, &QTimer::timeout, this, &PhraseNavigator::clearGroupInertia);
}

void PhraseNavigator::setGroupPanel(GroupPanel *panel) {
    panel_ = panel;
}

bool PhraseNavigator::groupInertiaActive() const {
    return groupInertia_;
}

// 惯性一没，分组面板就该收起来（除非鼠标还悬在上面）
void PhraseNavigator::clearGroupInertia() {
    groupInertia_ = false;
    if (panel_) panel_->hideIfIdle();
}

void PhraseNavigator::selectVisibleAt(int index) {
    const QVector<QListWidgetItem *> items = list_->visibleItems();
    if (index < 0 || index >= items.size()) return;
    list_->setCurrentItem(items[index]);
    list_->scrollToItem(items[index], QAbstractItemView::PositionAtCenter);
}

void PhraseNavigator::selectVisibleWithSafeArea(int index, int direction) {
    const QVector<QListWidgetItem *> items = list_->visibleItems();
    if (index < 0 || index >= items.size()) return;
    selectWithSafeArea(list_, items[index], direction);
}

void PhraseNavigator::moveVertical(int direction) {
    clearGroupInertia(); // 不管按键来自键盘钩子还是搜索模式里的 Qt 事件，上下浏览都立刻清掉翻组惯性
    const QVector<QListWidgetItem *> items = list_->visibleItems();
    if (items.isEmpty()) return;
    const int current = items.indexOf(list_->currentItem());
    if (current < 0) { // 没选中（比如刚从空分组切过来）：↓ 选第一个，↑ 选最后一个
        selectVisibleWithSafeArea(direction > 0 ? 0 : items.size() - 1, direction);
        return;
    }
    const int target = current + direction * groups_.currentColumns(); // 同一列的上/下一行
    if (target >= 0 && target < items.size()) {
        selectVisibleWithSafeArea(target, direction);
        return;
    }
    // 已经在第一行，或者最后一行这一列排不满：↑ 跳到第一个，↓ 跳到最后一个（本来就在那儿就不动）
    selectVisibleWithSafeArea(direction > 0 ? items.size() - 1 : 0, direction);
}

void PhraseNavigator::moveHorizontal(int direction) {
    const int groupIndex = groups_.currentIndex();
    const bool atEdgeGroup = groupIndex == 0 || groupIndex == groups_.count() - 1;
    if (groupInertia_ && !atEdgeGroup) {
        switchGroup(direction);
        return;
    }
    const QVector<QListWidgetItem *> items = list_->visibleItems();
    const int current = items.indexOf(list_->currentItem());
    if (current < 0) { // 空分组或没选中：直接切分组，空分组里连按也能一直切下去
        switchGroup(direction);
        return;
    }
    const int columns = groups_.currentColumns();
    const int column = current % columns;
    if (direction < 0 && column > 0) {
        selectVisibleAt(current - 1);
        return;
    }
    if (direction > 0 && column < columns - 1) {
        if (current + 1 < items.size()) {
            selectVisibleAt(current + 1);
            return;
        }
        // 右边一格是空的（最后一行没排满）：跳到上一行同列，也就是视觉上正右方最近的那个。
        // 比如 3 列 “1 2 3 / 4 5” 在 5 上按 → 跳到 3，而不是切分组
        if (current + 1 - columns >= 0) {
            selectVisibleAt(current + 1 - columns);
            return;
        }
    }
    switchGroup(direction); // 行首按 ←、行尾按 →
}

void PhraseNavigator::switchGroup(int direction) {
    const int index = groups_.currentIndex() + direction;
    if (index < 0 || index >= groups_.count()) return;
    groupInertia_ = true; // 切到第一个/最后一个分组也照样置位，只是在那里不生效
    inertiaTimer_.start(Settings::groupPanelDelayMs());
    groups_.setCurrentIndex(index); // 切换分组的处理会选中新分组第一项，正好是往右切要的结果
    if (panel_) panel_->show(); // 翻组时弹出面板，让用户看见翻到哪儿了
    // 关闭“始终选中第一条”时，往左切沿用老行为：选中第一行最后一个
    if (direction < 0 && !Settings::selectFirstOnGroupSwitch()) {
        const QVector<QListWidgetItem *> items = list_->visibleItems();
        if (items.isEmpty()) return;
        selectVisibleAt(qMin(groups_.currentColumns(), int(items.size())) - 1);
    }
}

void PhraseNavigator::selectEdge(int direction) {
    const int count = list_->count();
    if (count <= 0) return;
    for (int row = direction > 0 ? 0 : count - 1; row >= 0 && row < count; row += direction) {
        QListWidgetItem *item = list_->item(row);
        if (item && !item->isHidden()) {
            list_->setCurrentItem(item);
            list_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            return;
        }
    }
}
