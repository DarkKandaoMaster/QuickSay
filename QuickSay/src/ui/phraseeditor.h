#pragma once

#include <QWidget>

class HotkeyRegistry;
class MainWindow;
class QKeySequenceEdit;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;

// 添加/修改短语窗口。两种模式各一个实例，各自记住窗口位置。Esc 等同于取消。
class PhraseEditorWindow : public QWidget {
  public:
    enum class Mode {
        Add,
        Edit,
    };

    PhraseEditorWindow(Mode mode, MainWindow *mainWindow, HotkeyRegistry &hotkeys);
    ~PhraseEditorWindow() override;

    // insertAfterRow < 0 时加到列表末尾
    void openForAdd(const QString &group, int insertAfterRow);
    void openForEdit(QListWidgetItem *item);
    // 条目要被删除时调用，免得修改窗口还拿着野指针
    void forgetItem(QListWidgetItem *item);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

  private:
    void clearFields();
    void showAtSavedPosition();
    void accept();
    void acceptAdd();
    void acceptEdit();

    Mode mode_;
    MainWindow *mainWindow_;
    HotkeyRegistry &hotkeys_;
    QPlainTextEdit *textEdit_;
    QPlainTextEdit *remarkEdit_;
    QKeySequenceEdit *hotkeyEdit_;
    QPushButton *cancelButton_;

    QString group_; // 添加模式：新短语所属分组
    int insertAfterRow_ = -1; // 添加模式
    QListWidgetItem *item_ = nullptr; // 修改模式：正在修改的条目
};
