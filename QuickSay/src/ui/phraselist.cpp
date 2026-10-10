#include "ui/phraselist.h"

#include "core/groupmodel.h"
#include "core/phrase.h"
#include "core/settings.h"
#include "output/outputparser.h"
#include "ui/grouppanel.h"

#include <QApplication>
#include <QBuffer>
#include <QCursor>
#include <QEvent>
#include <QImageReader>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolTip>

#include <limits>

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

const int kToolTipMaxImages = 3;
const QSize kToolTipImageSize(240, 160); // 图片在悬停提示里最大多大，大图等比缩小
const int kToolTipMaxBodyLines = 20; // 和纯文字提示有备注时一样，正文最多 20 行
const int kToolTipMaxRemarkLines = 7;
const int kToolTipCharsPerLine = 50; // 估算行数用，和 PhraseItem 的纯文字提示一致

// 把一段纯文字转成 HTML 接到 html 后面，按行数预算截断，超出时以“……”结尾并返回 false
bool appendTextHtml(QString &html, const QString &text, int &linesLeft) {
    if (text.isEmpty()) return true; // 比如正文以图片结尾，后面没东西了，别凭空冒出个“……”
    const QStringList rows = text.split('\n');
    for (int i = 0; i < rows.size(); ++i) {
        const int rowLines = 1 + rows[i].length() / kToolTipCharsPerLine;
        if (i > 0) html += "<br>";
        if (linesLeft <= 0) {
            html += "……";
            return false;
        }
        if (rowLines > linesLeft) {
            html += rows[i].left(linesLeft * kToolTipCharsPerLine).toHtmlEscaped() + "……";
            linesLeft = 0;
            return false;
        }
        html += rows[i].toHtmlEscaped();
        linesLeft -= rowLines;
    }
    if (!rows.isEmpty()) ++linesLeft; // 最后一行没有换行，跟后面的内容接在同一行
    return true;
}

// 把图片缩到提示里的大小，编成 data: URL；读不出来返回空串。displaySize 返回显示用的逻辑尺寸。
// 不直接写文件路径：QLabel 不认 file:/// 地址，裸路径里有 # % 之类又会被当成网址拆坏；而且只解码缩小后的图，大照片也不卡
QString toolTipImageSrc(const QString &filePath, QSize &displaySize) {
    QImageReader reader(filePath);
    const QSize original = reader.size(); // 只读文件头
    if (!original.isValid()) return QString();
    displaySize = original;
    if (original.width() > kToolTipImageSize.width() || original.height() > kToolTipImageSize.height()) displaySize.scale(kToolTipImageSize, Qt::KeepAspectRatio);
    // 高 DPI 屏按物理像素解码，不然缩放 150% 时图是糊的
    const QSize decodeSize = (QSizeF(displaySize) * qApp->devicePixelRatio()).toSize().boundedTo(original);
    if (decodeSize != original) reader.setScaledSize(decodeSize);
    const QImage image = reader.read();
    if (image.isNull()) return QString();
    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) return QString();
    return "data:image/png;base64," + QString::fromLatin1(png.toBase64());
}

// 正文里有有效的 <img 路径> 时，悬停提示把图片直接画出来（主窗口里仍然只显示标签文字）。
// 没有图片就用条目上现成的纯文字提示，行为和以前一样。只在提示真正弹出时调用，所以每次都现读图片文件
QString phraseToolTip(QListWidgetItem *item) {
    const QString body = PhraseItem::text(item);
    const QVector<ImageTag> tags = findImageTags(body);
    if (tags.isEmpty()) return item->toolTip();

    QString html;
    int linesLeft = kToolTipMaxBodyLines;
    int imagesShown = 0;
    int position = 0;
    QSize size;
    bool complete = true;
    for (const ImageTag &tag : tags) {
        if (!appendTextHtml(html, body.mid(position, tag.start - position), linesLeft)) {
            complete = false;
            break;
        }
        position = tag.start + tag.length;
        const QString imageSrc = imagesShown < kToolTipMaxImages ? toolTipImageSrc(tag.filePath, size) : QString();
        if (imageSrc.isEmpty()) { // 不是图片、读不出来或者已经放够了：原样显示标签文字
            if (!appendTextHtml(html, body.mid(tag.start, tag.length), linesLeft)) {
                complete = false;
                break;
            }
            continue;
        }
        // 图片独占一行；紧挨着的两张图之间不要多出一个空行
        if (!html.isEmpty() && !html.endsWith("<br>")) html += "<br>";
        html += QString("<img src=\"%1\" width=\"%2\" height=\"%3\"><br>").arg(imageSrc).arg(size.width()).arg(size.height());
        ++imagesShown;
        linesLeft -= size.height() / 16 + 1; // 图片也占篇幅，按大约一行 16px 折算
    }
    if (complete) appendTextHtml(html, body.mid(position), linesLeft);

    const QString remark = PhraseItem::remark(item);
    if (!remark.isEmpty()) {
        int remarkLines = kToolTipMaxRemarkLines;
        html += "<br>────────────────────────<br>";
        appendTextHtml(html, remark, remarkLines);
    }
    // pre-wrap：保留正文里连续的空格；<html> 开头让 QToolTip 按富文本显示
    return "<html><body style=\"white-space:pre-wrap\">" + html + "</body></html>";
}

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
    installListToolTip(this, phraseToolTip);
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
    const QString sheet = QString("QListWidget::item{ height: %1px; padding: %2px %3px; }")
                              .arg(itemHeight)
                              .arg(Settings::itemPaddingVertical())
                              .arg(Settings::itemPaddingHorizontal());
    // 用方向键翻组时这里是在低级键盘钩子的回调里同步跑的，回调太慢按键会漏给前台程序、钩子还可能被系统摘掉。
    // setStyleSheet 会把整个列表重新 polish 一遍，行高内边距没变（大多数分组都一样）就别重设
    if (styleSheet() != sheet) setStyleSheet(sheet);
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

// 方向键浏览一律走安全区规则。左右键以前用 PositionAtCenter，同一行里挪一格也会把这一行硬拽到正中间
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
    // 已经在第一行/最后一行：不动。以前多列时会顺势横着滑到第一个/最后一个，看着像按了←→
    const int columns = groups_.currentColumns();
    if (direction < 0 || current / columns == (items.size() - 1) / columns) return;
    // 下面还有一行、只是这一列排不满（3 列 “1 2 3 / 4 5” 在 3 上按 ↓）：落到最后一个，也就是视觉上正下方最近的那个
    selectVisibleWithSafeArea(items.size() - 1, direction);
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
        selectVisibleWithSafeArea(current - 1, direction);
        return;
    }
    if (direction > 0 && column < columns - 1) {
        if (current + 1 < items.size()) {
            selectVisibleWithSafeArea(current + 1, direction);
            return;
        }
        // 右边一格是空的（最后一行没排满）：跳到上一行同列，也就是视觉上正右方最近的那个。
        // 比如 3 列 “1 2 3 / 4 5” 在 5 上按 → 跳到 3，而不是切分组
        if (current + 1 - columns >= 0) {
            selectVisibleWithSafeArea(current + 1 - columns, -1); // 实际是往上挪了一行
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
        selectVisibleWithSafeArea(qMin(groups_.currentColumns(), int(items.size())) - 1, -1);
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

namespace {

// 盯着“提示还该不该留着”的间隔。列表在鼠标不动时也会变（键盘滚动、翻组、搜索过滤），这时收不到鼠标事件，只能自己查
const int kHoverCheckMs = 100;

// QToolTip::hideText() 只是让提示 300ms 后才收起（Qt 自己移出 rect 时也一样），这里要鼠标一离开条目就收。
// 提示框是 Qt 内部的 QTipLabel，只能按它固定的 objectName 找；用 close() 而不是 hide()，才能停掉系统开着提示淡入时的动画
void hideToolTipNow() {
    if (!QToolTip::isVisible()) return;
    QToolTip::hideText();
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (widget->objectName() == QLatin1String("qtooltip_label") && widget->isVisible()) widget->close();
    }
}

class ToolTipFilter : public QObject {
  public:
    ToolTipFilter(QListWidget *list, std::function<QString(QListWidgetItem *)> textFor) : QObject(list), list_(list), textFor_(std::move(textFor)) {
        timer_.setSingleShot(true);
        connect(&timer_, &QTimer::timeout, this, [this]() {
            showForLastItem();
        });
        hoverCheck_.setInterval(kHoverCheckMs);
        connect(&hoverCheck_, &QTimer::timeout, this, [this]() {
            // 鼠标底下已经换了条目（或者窗口藏起来了）就收起；提示被 Qt 自己收掉了（点了别处之类）就不再盯，等下次鼠标移动重新计时
            if (itemUnderCursor() != lastItem_) hide();
            else if (!QToolTip::isVisible()) hoverCheck_.stop();
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
            // 提示已经显示着、鼠标还在同一个条目里：不跟着鼠标重新计时。否则延迟设成 0 时提示会追着光标一闪一闪
            if (item == lastItem_ && QToolTip::isVisible() && !timer_.isActive()) return false;
            if (item != lastItem_ || pos != lastViewportPos_) {
                hideToolTipNow();
                hoverCheck_.stop();
                lastItem_ = item;
                lastViewportPos_ = pos;
                timer_.start(Settings::toolTipDelayMs()); // 每次现读，设置里改完立即生效
            }
            return false;
        }
        case QEvent::ToolTip:
            // 只认自己的计时器。Qt 认为窗口激活时（搜索模式下的主窗口；分组面板是没有父窗口的 Qt::Tool，
            // Windows 风格下 Qt 一直当它是激活的）也会发这个事件：鼠标停 700ms 就发，上一个提示收起后 2 秒内
            // 换条目只等 20ms。拿它来显示的话，延迟设成 1000 时新提示会几乎立即弹出来
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
    // 现在光标底下、有提示的条目。不用上次鼠标事件记下的坐标：计时期间列表可能已经滚动、翻组、过滤，窗口也可能被藏起来
    QListWidgetItem *itemUnderCursor() const {
        QWidget *viewport = list_->viewport();
        if (!viewport->isVisible()) return nullptr;
        const QPoint pos = viewport->mapFromGlobal(QCursor::pos());
        if (!viewport->rect().contains(pos)) return nullptr;
        QListWidgetItem *item = list_->itemAt(pos);
        if (!item || item->isHidden() || item->toolTip().isEmpty()) return nullptr;
        return item;
    }

    bool showForLastItem() {
        // 先比指针再解引用：lastItem_ 可能已经随短语删除被释放
        if (!lastItem_ || itemUnderCursor() != lastItem_) return false;
        const QString text = textFor_ ? textFor_(lastItem_) : lastItem_->toolTip();
        if (text.isEmpty()) return false;
        // 带上 rect：鼠标移出这个条目提示就自己消失。显示时长给到最大：Qt 默认十几秒后自动收起，这里只要鼠标还停在条目上就一直留着
        QToolTip::showText(QCursor::pos(), text, list_->viewport(), list_->visualItemRect(lastItem_), std::numeric_limits<int>::max());
        hoverCheck_.start();
        return true;
    }

    void hide() {
        timer_.stop();
        hoverCheck_.stop();
        lastItem_ = nullptr;
        hideToolTipNow();
    }

    QListWidget *list_;
    std::function<QString(QListWidgetItem *)> textFor_;
    QTimer timer_;
    QTimer hoverCheck_;
    QPoint lastViewportPos_;
    QListWidgetItem *lastItem_ = nullptr;
};

} // namespace

void installListToolTip(QListWidget *list, std::function<QString(QListWidgetItem *)> textFor) {
    list->setMouseTracking(true);
    list->viewport()->setMouseTracking(true);
    list->viewport()->installEventFilter(new ToolTipFilter(list, std::move(textFor)));
}
