// 不碰系统的界面逻辑：分组增删移动时“当前分组”的变化（照搬 QTabBar 的规则）、角标编号、方向键浏览。
// 和 tst_core.cpp 分成两个文件，由同一个测试程序跑（见 main 函数）。
#include "core/groupmodel.h"
#include "core/phrase.h"
#include "core/settings.h"
#include "ui/grouppanel.h"
#include "ui/phraselist.h"

#include <QSignalSpy>
#include <QTest>

// PhraseNavigator 会调分组面板的这两个函数；测试里没有面板（指针为空），只是让链接通过，不用把整个主窗口拉进来
void GroupPanel::show() {}
void GroupPanel::hideIfIdle() {}

class UiTest : public QObject {
    Q_OBJECT

  private:
    // 分组 A：3 条单列；B：5 条 3 列（“0 1 2 / 3 4”）；C：2 条单列。当前在 B
    struct Fixture {
        QWidget window;
        GroupModel groups;
        PhraseList *list = nullptr;
        PhraseNavigator *navigator = nullptr;

        Fixture() {
            window.resize(360, 500);
            list = new PhraseList(&window);
            list->resize(360, 460);
            groups.insert(0, {"A", 40, 1});
            groups.insert(1, {"B", 40, 3});
            groups.insert(2, {"C", 40, 1});
            const QList<QPair<QString, int>> content = {{"A", 3}, {"B", 5}, {"C", 2}};
            for (const auto &group : content)
                for (int i = 0; i < group.second; ++i)
                    list->addItem(PhraseItem::create(QString("%1%2").arg(group.first).arg(i), "", group.first, ""));
            // 和 MainWindow::onCurrentGroupChanged 做的一样
            QObject::connect(&groups, &GroupModel::currentChanged, list, [this](int index) {
                list->applyFilter(groups.name(index), "");
                list->applyLayout(groups.itemHeight(index), groups.columns(index));
                list->setCurrentItem(nullptr);
                list->selectFirstVisible();
            });
            navigator = new PhraseNavigator(list, groups, list);
            window.show();
            groups.setCurrentIndex(1);
        }

        void select(const QString &text) {
            for (int i = 0; i < list->count(); ++i) {
                if (PhraseItem::text(list->item(i)) == text) list->setCurrentItem(list->item(i));
            }
        }
        QString current() const {
            return list->currentItem() ? PhraseItem::text(list->currentItem()) : QString();
        }
    };

  private slots:
    void initTestCase() {
        Settings::load(); // 用默认设置（翻组惯性 1.5 秒、切组后选中第一条）
    }

    // ---- GroupModel：当前分组怎么变，什么时候发 currentChanged ----

    void groupInsertIntoEmptySelectsIt() {
        GroupModel groups;
        QSignalSpy spy(&groups, &GroupModel::currentChanged);
        groups.insert(0, {"A", 40, 1});
        QCOMPARE(groups.currentIndex(), 0);
        QCOMPARE(spy.count(), 1);
        groups.insert(0, {"Z", 40, 1}); // 插在当前分组前面：当前分组没变，下标跟着后移，不发信号
        QCOMPARE(groups.currentName(), QString("A"));
        QCOMPARE(spy.count(), 1);
    }

    void groupRemoveRules() {
        GroupModel groups;
        for (const char *name : {"A", "B", "C", "D"})
            groups.insert(groups.count(), {name, 40, 1});
        groups.setCurrentIndex(1); // B
        QSignalSpy spy(&groups, &GroupModel::currentChanged);

        groups.remove(3); // 删后面的：什么都不变
        QCOMPARE(groups.currentName(), QString("B"));
        QCOMPARE(spy.count(), 0);

        groups.remove(0); // 删前面的：分组没变但下标变了，QTabBar 也会发 currentChanged
        QCOMPARE(groups.currentName(), QString("B"));
        QCOMPARE(groups.currentIndex(), 0);
        QCOMPARE(spy.count(), 1);

        groups.remove(0); // 删当前的：选中右边那个
        QCOMPARE(groups.currentName(), QString("C"));
        QCOMPARE(spy.count(), 2);
    }

    void groupRemoveLastCurrentSelectsNewLast() {
        GroupModel groups;
        for (const char *name : {"A", "B"})
            groups.insert(groups.count(), {name, 40, 1});
        groups.setCurrentIndex(1);
        groups.remove(1);
        QCOMPARE(groups.currentName(), QString("A"));
    }

    void groupMoveKeepsCurrentGroup() {
        GroupModel groups;
        for (const char *name : {"A", "B", "C", "D"})
            groups.insert(groups.count(), {name, 40, 1});
        groups.setCurrentIndex(1); // B
        groups.move(1, 3); // 拖当前分组本身
        QCOMPARE(groups.currentName(), QString("B"));
        QCOMPARE(groups.currentIndex(), 3);
        groups.move(0, 3); // 从当前分组前面拖到后面：当前分组前移一格
        QCOMPARE(groups.currentName(), QString("B"));
        QCOMPARE(groups.currentIndex(), 2);
    }

    // ---- 角标 ----

    void badgesAreOneToNineZeroThenLetters() {
        QWidget window;
        PhraseList list(&window);
        for (int i = 0; i < 40; ++i)
            list.addItem(PhraseItem::create(QString::number(i), "", i % 2 ? "other" : "g", "")); // 隔一条混进别的分组
        for (int i = 0; i < 20; ++i)
            list.addItem(PhraseItem::create(QString("x%1").arg(i), "", "g", ""));
        list.applyFilter("g", "");
        const QVector<QListWidgetItem *> visible = list.visibleItems();
        QCOMPARE(visible.size(), 40);
        QCOMPARE(visible[0]->data(PhraseRole::Badge).toString(), QString("1"));
        QCOMPARE(visible[8]->data(PhraseRole::Badge).toString(), QString("9"));
        QCOMPARE(visible[9]->data(PhraseRole::Badge).toString(), QString("0"));
        QCOMPARE(visible[10]->data(PhraseRole::Badge).toString(), QString("A"));
        QCOMPARE(visible[35]->data(PhraseRole::Badge).toString(), QString("Z"));
        QCOMPARE(visible[36]->data(PhraseRole::Badge).toString(), QString()); // 超过 36 条不编角标
        QCOMPARE(list.visibleItemAt(10), visible[9]);
        QCOMPARE(list.item(1)->data(PhraseRole::Badge).toString(), QString()); // 别的分组的条目隐藏且没有角标
    }

    void searchFiltersTextAndRemark() {
        QWidget window;
        PhraseList list(&window);
        list.addItem(PhraseItem::create("hello", "", "g", ""));
        list.addItem(PhraseItem::create("abc", "备注里有HELLO", "g", ""));
        list.addItem(PhraseItem::create("zzz", "", "g", ""));
        list.applyFilter("g", "Hello"); // 不区分大小写，正文和备注都搜
        QCOMPARE(list.visibleItems().size(), 2);
    }

    // ---- 方向键浏览 ----

    void verticalMovesByColumns() {
        Fixture f;
        QCOMPARE(f.current(), QString("B0"));
        f.navigator->moveVertical(1);
        QCOMPARE(f.current(), QString("B3"));
        f.select("B2");
        f.navigator->moveVertical(1); // 正下方是空格，落到最后一条（视觉上最近的那个）
        QCOMPARE(f.current(), QString("B4"));
    }

    void verticalStopsAtTopAndBottomRow() {
        Fixture f;
        f.select("B1");
        f.navigator->moveVertical(-1); // 第一行按 ↑ 不横着滑到 B0
        QCOMPARE(f.current(), QString("B1"));
        f.select("B4");
        f.navigator->moveVertical(1); // 最后一行按 ↓ 不动
        QCOMPARE(f.current(), QString("B4"));
        f.select("B3");
        f.navigator->moveVertical(1);
        QCOMPARE(f.current(), QString("B3"));
    }

    void horizontalWithinRowAndIntoEmptyCell() {
        Fixture f;
        f.select("B1");
        f.navigator->moveHorizontal(-1);
        QCOMPARE(f.current(), QString("B0"));
        f.select("B4");
        f.navigator->moveHorizontal(1); // 右边一格是空的：跳到上一行同列
        QCOMPARE(f.current(), QString("B2"));
        QCOMPARE(f.groups.currentName(), QString("B"));
    }

    void groupInertia() {
        Fixture f;
        f.select("B3"); // 行首
        f.navigator->moveHorizontal(-1); // 行首按 ← 切到 A，置上翻组惯性
        QCOMPARE(f.groups.currentName(), QString("A"));
        QVERIFY(f.navigator->groupInertiaActive());
        QCOMPARE(f.current(), QString("A0")); // 默认切组后选中第一条

        f.navigator->moveHorizontal(-1); // A 是第一个分组：惯性挂起，按正常移动处理，左边没分组了就不动
        QCOMPARE(f.groups.currentName(), QString("A"));
        QVERIFY(f.navigator->groupInertiaActive()); // 挂起但不清除

        f.navigator->moveHorizontal(1); // 回到中间分组 B
        QCOMPARE(f.groups.currentName(), QString("B"));
        f.navigator->moveHorizontal(1); // B 是中间分组且惯性还在：不在行内移动，直接翻到 C
        QCOMPARE(f.groups.currentName(), QString("C"));

        f.navigator->moveVertical(1); // 上下键立刻清掉惯性
        QVERIFY(!f.navigator->groupInertiaActive());
    }

    void inertiaExpires() {
        Settings::setGroupPanelDelayMs(100);
        Fixture f;
        f.select("B3");
        f.navigator->moveHorizontal(-1);
        QVERIFY(f.navigator->groupInertiaActive());
        QTRY_VERIFY_WITH_TIMEOUT(!f.navigator->groupInertiaActive(), 2000);
        Settings::setGroupPanelDelayMs(1500);
    }

    void homeEnd() {
        Fixture f;
        f.navigator->selectEdge(-1);
        QCOMPARE(f.current(), QString("B4"));
        f.navigator->selectEdge(1);
        QCOMPARE(f.current(), QString("B0"));
    }
};

QObject *createUiTest() {
    return new UiTest;
}

#include "tst_ui.moc"
