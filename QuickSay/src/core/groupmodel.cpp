#include "core/groupmodel.h"

#include "core/settings.h"

bool GroupModel::isValid(int index) const {
    return index >= 0 && index < groups_.size();
}

int GroupModel::count() const {
    return groups_.size();
}

int GroupModel::currentIndex() const {
    return current_;
}

QString GroupModel::name(int index) const {
    return isValid(index) ? groups_[index].name : QString();
}

QString GroupModel::currentName() const {
    return name(current_);
}

int GroupModel::itemHeight(int index) const {
    const int height = isValid(index) ? groups_[index].itemHeight : 0;
    return height > 0 ? height : Settings::defaultItemHeight();
}

int GroupModel::columns(int index) const {
    const int columns = isValid(index) ? groups_[index].columns : 0;
    return qBound(1, columns, 10); // 上限和新建/修改分组对话框的输入范围一致
}

int GroupModel::currentColumns() const {
    return current_ < 0 ? 1 : columns(current_);
}

bool GroupModel::contains(const QString &name) const {
    for (const Group &group : groups_) {
        if (group.name == name) return true;
    }
    return false;
}

void GroupModel::setCurrentIndex(int index) {
    if (current_ == index || !isValid(index)) return;
    current_ = index;
    emit currentChanged(index);
}

void GroupModel::insert(int index, const Group &group) {
    groups_.insert(index, group);
    if (groups_.size() == 1) setCurrentIndex(index);
    else if (index <= current_) ++current_;
}

void GroupModel::remove(int index) {
    if (!isValid(index)) return;
    groups_.removeAt(index);
    if (index == current_) {
        current_ = -1;
        if (groups_.isEmpty()) emit currentChanged(-1);
        else setCurrentIndex(qBound(0, index, int(groups_.size()) - 1)); // 删掉当前分组时选中右边那个
    } else if (index < current_) {
        // QTabBar 在这种情况下也会发 currentChanged（下标变了，分组没变）
        current_ = current_ - 1;
        emit currentChanged(current_);
    }
}

void GroupModel::move(int from, int to) {
    if (from == to || !isValid(from) || !isValid(to)) return;
    groups_.move(from, to);
    const int previous = current_;
    if (current_ == from) {
        current_ = to;
    } else if (current_ >= qMin(from, to) && current_ <= qMax(from, to)) {
        current_ += from < to ? -1 : 1;
    }
    emit moved(from, to);
    if (previous != current_) emit currentChanged(current_);
}

void GroupModel::rename(int index, const QString &name) {
    if (isValid(index)) groups_[index].name = name;
}

void GroupModel::setLayout(int index, int itemHeight, int columns) {
    if (!isValid(index)) return;
    groups_[index].itemHeight = itemHeight;
    groups_[index].columns = columns;
}

void GroupModel::reset(const QVector<Group> &groups) {
    groups_ = groups;
    current_ = groups_.isEmpty() ? -1 : 0;
}
