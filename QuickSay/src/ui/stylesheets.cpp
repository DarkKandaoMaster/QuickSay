#include "ui/stylesheets.h"

namespace StyleSheets {

const char *const kApp = R"(
QWidget{
    background-color: #f3f3f3;
    font-family: "Inter","Segoe UI","Microsoft YaHei UI","Microsoft YaHei","Source Han Sans SC","PingFang SC","Helvetica Neue",Arial,sans-serif,"Segoe UI Symbol","Euphemia","Gadugi";
    font-size: 9pt;
    font-weight: 400;
    color: #323130;
    selection-background-color: #0078d4;
    selection-color: #ffffff;
}

/* 主窗口右上角的设置、添加、图钉按钮 */
QPushButton#iconButton{
    background-color: transparent;
    border: none;
    border-radius: 4px;
    padding: 0px;
    outline: none;
}
QPushButton#iconButton:hover{
    background-color: #f9f9f9;
    border: none;
}
QPushButton#iconButton:pressed{
    background-color: #f7f7f7;
    border: none;
}
QPushButton#iconButton:focus{
    border: none;
    outline: none;
}

QPushButton{
    background-color: #ffffff;
    border: 1px solid #d1d1d1;
    border-radius: 4px;
    padding: 6px 12px;
    color: #323130;
    outline: none;
}
QPushButton:hover{
    background-color: #f9f9f9;
    border-color: #c8c8c8;
}
QPushButton:pressed{
    background-color: #e5f3ff;
    border-color: #0078d4;
    color: #005a9e;
}
QPushButton:focus{
    border-color: #0078d4;
    outline: none;
}

QListWidget{
    background-color: #fcfcfc;
    border: 1px solid #e5e5e5;
    border-radius: 6px;
    padding: 4px;
    outline: none;
    selection-background-color: transparent;
    alternate-background-color: transparent;
}
/* 这里不能设字体颜色：短语和备注颜色不同，是在条目上单独设的 */
QListWidget::item{
    background-color: #ffffff;
    border: 1px solid #e5e5e5;
    border-radius: 4px;
    margin: 2px 2px;
}
QListWidget::item:hover{
    background-color: #f9f9f9;
    border-color: #d1d1d1;
}
QListWidget::item:selected{
    background-color: #e5f3ff;
    border-color: #0078d4;
}
QListWidget::item:selected:hover{
    background-color: #cce7ff;
    border-color: #106ebe;
}

/* 主窗口左上角“箭头+当前分组名”按钮，配色和未选中的短语项一致 */
QPushButton#groupButton{
    background-color: #ffffff;
    border: 1px solid #e5e5e5;
    border-radius: 4px;
    color: #323130;
    text-align: left;
    padding: 0px 2px 0px 20px; /* 左边 20px 留给箭头，改它要连着改 GroupButton::paintEvent 里箭头的横坐标 */
    outline: none;
}
QPushButton#groupButton:hover{
    background-color: #f9f9f9;
    border-color: #d1d1d1;
}

QLineEdit,QPlainTextEdit,QTextEdit{
    background-color: #ffffff;
    border: 2px solid #e5e5e5;
    border-radius: 4px;
    padding: 8px 12px;
    color: #323130;
    selection-background-color: #0078d4;
    selection-color: #ffffff;
}
QLineEdit:focus,QPlainTextEdit:focus,QTextEdit:focus{
    outline: none;
}

QSpinBox{
    background-color: #ffffff;
    border: 2px solid #e5e5e5;
    border-radius: 4px;
    padding: 6px 8px;
    color: #323130;
}
QSpinBox:focus{
    border-color: #0078d4;
    outline: none;
}
QSpinBox::up-button,QSpinBox::down-button{
    width: 0px;
    height: 0px;
    border: none;
    background: none;
}

QKeySequenceEdit QLineEdit{
    background-color: #ffffff;
    border: 2px solid #e5e5e5;
    border-radius: 4px;
    padding: 8px 12px;
    color: #323130;
}
QKeySequenceEdit QLineEdit:focus{
    border-color: #0078d4;
    outline: none;
}

QMenu {
    background-color: #fefefe;
    border: 1px solid #e5e5e5;
    padding: 3px 3px;
}
QMenu::item {
    padding: 8px 16px;
    color: #222222;
    min-width: 60px;
}
QMenu::item:selected {
    background-color: #e5f3ff;
}
QMenu::item:pressed {
    background-color: #cce7ff;
}

QLabel{
    color: #605e5c;
    padding: 2px 0px;
}

/* 滑块宽 8-1-1=6px，圆角超过宽度的一半 Qt 就不画圆角了，所以最大只能是 3 */
QScrollBar:vertical{
    background-color: #f3f3f3;
    width: 8px;
    border-radius: 4px;
    margin: 0px;
    border: none;
}
QScrollBar::handle:vertical{
    background-color: #c7c7c7;
    border-radius: 3px;
    min-height: 20px;
    margin: 2px 1px;
}
QScrollBar::handle:vertical:hover{
    background-color: #a6a6a6;
}
QScrollBar::handle:vertical:pressed{
    background-color: #8a8a8a;
}
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{
    height: 0px;
}
QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{
    background-color: transparent;
}
QScrollBar:horizontal{
    background-color: #f3f3f3;
    height: 8px;
    border-radius: 4px;
    margin: 0px;
    border: none;
}
QScrollBar::handle:horizontal{
    background-color: #c7c7c7;
    border-radius: 3px;
    min-width: 20px;
    margin: 1px 2px;
}
QScrollBar::handle:horizontal:hover{
    background-color: #a6a6a6;
}
QScrollBar::handle:horizontal:pressed{
    background-color: #8a8a8a;
}
QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{
    width: 0px;
}
QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal{
    background-color: transparent;
}

QToolTip{
    background-color: #ffffff;
    color: #222222;
    padding: 2px 1px;
}
)";

const char *const kGroupPanel = R"(
/* 无边框顶层窗口画不出圆角，四角会露出桌面，所以改回直角；分组四周的留白靠分组自己的 2px 外边距 */
QListWidget#groupPanel{
    border-radius: 0px;
    border: none;
    padding: 0px;
}
QListWidget#groupPanel::item{
    color: #323130;
}
QListWidget#groupPanel::item:selected{
    color: #005a9e;
}
)";

const char *const kSettings = R"(
QWidget#settingsWindow{
    background-color:#f0f0f0;
}
QWidget#settingsWindow QTabWidget::pane{
    background-color:#f9f9f9;
    border:1px solid #e5e5e5;
    top:-1px; /* 和页签底边压在一起，选中页签就和配置区域连成一体 */
}
QWidget#settingsWindow QTabWidget::tab-bar{
    left:0px;
}
QWidget#settingsWindow QTabBar{
    background-color:transparent;
    border:none;
}
QWidget#settingsWindow QTabBar::tab{
    background-color:#f0f0f0;
    border:1px solid #e5e5e5;
    color:#000000;
    height:24px;
    padding:0px 8px;
    margin-top:2px;   /* 选中后向上长高 2px，复现原生页签的凸起效果 */
    margin-right:-1px; /* 相邻页签共用一条竖边框 */
}
QWidget#settingsWindow QTabBar::tab:hover{
    background-color:#f5f5f5;
}
QWidget#settingsWindow QTabBar::tab:selected{
    background-color:#f9f9f9;
    border-bottom-color:#f9f9f9;
    height:26px;
    margin-top:0px;
}
/* 只点名滚动内容和视口；写成“QScrollArea 下所有控件”会连滚动条一起套上规则，原生外观就没了 */
QWidget#settingsWindow QWidget#settingsPage,QWidget#settingsWindow QWidget#settingsViewport{
    background-color:#f9f9f9;
}
QWidget#settingsWindow QLabel#settingsHint{
    color:#606060;
}
)";

} // namespace StyleSheets
