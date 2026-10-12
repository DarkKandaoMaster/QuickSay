#pragma once

#include <QObject>
#include <QString>
#include <QVector>

struct Group {
    QString name;
    int itemHeight = 0; // <=0 表示跟随默认行高
    int columns = 1; // 列数
};

// 分组列表和“当前分组”。短语靠分组名字符串关联到分组，不是 id。读写文件在 PhraseStore（分组和短语存在同一个文件里）。
// 增删移动时当前分组的变化规则照搬 QTabBar（分组曾经就存在一个隐藏的 QTabBar 里），
// 所以 currentChanged 的触发时机和老版本一致。
class GroupModel : public QObject {
    Q_OBJECT

  public:
    using QObject::QObject;

    int count() const;
    int currentIndex() const;
    QString name(int index) const; // 越界时返回空串
    QString currentName() const;
    int itemHeight(int index) const; // 未设置时取默认行高
    int columns(int index) const; // 限制在 1..10
    int currentColumns() const;
    bool contains(const QString &name) const;

    void setCurrentIndex(int index);
    void insert(int index, const Group &group);
    void remove(int index);
    void move(int from, int to);
    void rename(int index, const QString &name);
    void setLayout(int index, int itemHeight, int columns);
    // 整体换掉（读文件时用）：当前分组回到第一个，不发 currentChanged
    void reset(const QVector<Group> &groups);

  signals:
    void currentChanged(int index);
    void moved(int from, int to);

  private:
    bool isValid(int index) const;

    QVector<Group> groups_;
    int current_ = -1;
};
