#pragma once

#include <QSize>
#include <QWidget>

class HotkeyRegistry;
class MainWindow;
class QCheckBox;
class QKeySequenceEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QuickSayApp;

// 设置窗口，外观照着 TrafficMonitor 的设置对话框做：四个页签、每页独立滚动、底部固定“确定/取消/应用”。
// 用普通 QWidget 而不是 QDialog：QDialog 自带回车=确定、Esc=取消，这里要求任何按键都不触发这三个按钮。
// 界面上的修改要点“应用”或“确定”才生效；“取消”或关闭窗口会丢弃未应用的修改。
class SettingsWindow : public QWidget {
  public:
    SettingsWindow(QuickSayApp *app, MainWindow *mainWindow, HotkeyRegistry &hotkeys);

    // 重复打开只把窗口拉到前台，不重新填值，免得冲掉还没应用的修改
    void open();

  protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

  private:
    void buildMainWindowPage(QTabWidget *tabs);
    void buildInputPage(QTabWidget *tabs);
    void buildGeneralPage(QTabWidget *tabs);
    void buildAboutPage(QTabWidget *tabs);
    void connectChangeTracking();

    void loadValues();
    bool hasUnappliedChanges() const;
    void updateApplyButton();
    bool apply();
    void updateGroupPanelDelayVisibility();

    void exportBackup();
    void importBackup();

    // 窗口外框（含标题栏和透明拖拽边）的逻辑像素尺寸
    QSize outerSize();
    // 按窗口样式和 DPI 算出来的外框比客户区多出的尺寸
    QSize computedBorder();
    void fitToTargetSize(const QSize &border);

    QuickSayApp *app_;
    MainWindow *mainWindow_;
    HotkeyRegistry &hotkeys_;

    QSpinBox *widthSpin_ = nullptr;
    QSpinBox *heightSpin_ = nullptr;
    QCheckBox *alwaysOnTopCheck_ = nullptr;
    QSpinBox *toolTipDelaySpin_ = nullptr;
    QSpinBox *itemHeightSpin_ = nullptr;
    QSpinBox *paddingHorizontalSpin_ = nullptr;
    QSpinBox *paddingVerticalSpin_ = nullptr;
    QCheckBox *badgeTopLeftCheck_ = nullptr;
    QCheckBox *groupPanelAlwaysVisibleCheck_ = nullptr;
    QWidget *groupPanelDelayLabel_ = nullptr;
    QSpinBox *groupPanelDelaySpin_ = nullptr;
    QCheckBox *selectFirstCheck_ = nullptr;
    QSpinBox *scrollStepSpin_ = nullptr;
    QSpinBox *safeAreaSpin_ = nullptr;
    QSpinBox *outputDelaySpin_ = nullptr;
    QCheckBox *badgePassthroughCheck_ = nullptr;
    QCheckBox *badgeCtrlCheck_ = nullptr;
    QWidget *badgeCtrlRow_ = nullptr;
    QCheckBox *enterPassthroughCheck_ = nullptr;
    QCheckBox *enterCtrlCheck_ = nullptr;
    QWidget *enterCtrlRow_ = nullptr;
    QCheckBox *arrowPassthroughCheck_ = nullptr;
    QCheckBox *arrowCtrlCheck_ = nullptr;
    QWidget *arrowCtrlRow_ = nullptr;
    QKeySequenceEdit *hotkeyEdit_ = nullptr;
    QCheckBox *autoStartCheck_ = nullptr;
    QCheckBox *runAsAdminCheck_ = nullptr;
    QCheckBox *checkUpdatesCheck_ = nullptr;
    QPushButton *applyButton_ = nullptr;

    QObject *wheelFilter_ = nullptr;
    bool sizeInitialized_ = false;
    QSize targetOuterSize_;
};
