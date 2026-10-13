#include "ui/phraseeditor.h"

#include "core/files.h"
#include "core/phrase.h"
#include "core/settings.h"
#include "hotkey/globalhotkey.h"
#include "hotkey/hotkeyinput.h"
#include "ui/mainwindow.h"
#include "ui/phraselist.h"
#include "ui/stylesheets.h"

#include <QApplication>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextOption>
#include <QUrl>
#include <QWidget>

class QWidget;

// “如何更高级地输入？”帮助窗口，在 parent 右下方 30px 处打开，关闭后自动销毁
void showAdvancedInputHelp(QWidget *parent);

PhraseEditorWindow::PhraseEditorWindow(Mode mode, MainWindow *mainWindow, HotkeyRegistry &hotkeys)
    : QWidget(nullptr), mode_(mode), mainWindow_(mainWindow), hotkeys_(hotkeys) {
    const bool adding = mode == Mode::Add;
    setStyleSheet(StyleSheets::kApp);
    setWindowTitle(adding ? "QuickSay-添加" : "QuickSay-修改");
    setWindowIcon(QIcon(Paths::icon("logo.svg")));

    textEdit_ = new QPlainTextEdit(this);
    auto *helpButton = new QPushButton("如何更高级地输入？", this);
    auto *remarkLabel = new QLabel("备注：", this);
    remarkEdit_ = new QPlainTextEdit(this);
    auto *hotkeyLabel = new QLabel("快捷键：", this);
    hotkeyEdit_ = new QKeySequenceEdit(this);
    auto *clearButton = new QPushButton("清空", this);
    cancelButton_ = new QPushButton("取消", this);
    auto *confirmButton = new QPushButton(adding ? "添加" : "修改", this);

    // 固定 500*500，不跟随主窗口大小设置
    auto place = [](QWidget *widget, int x, int y, int width, int height) {
        widget->move(x, y);
        widget->setFixedSize(width, height);
    };
    setFixedSize(500, 500);
    place(textEdit_, 0, 0, 500, 200);
    place(helpButton, 350, 200, 147, 35);
    place(remarkLabel, 10, 230, 50, 20);
    place(remarkEdit_, 0, 250, 500, 125);
    place(hotkeyLabel, 10, 407, 50, 20);
    place(hotkeyEdit_, 70, 400, 200, 38);
    place(clearButton, 270, 400, 57, 38);
    place(cancelButton_, 0, 463, 250, 33);
    place(confirmButton, 250, 463, 250, 33);

    connect(helpButton, &QPushButton::clicked, this, [this]() {
        showAdvancedInputHelp(this);
    });
    connect(clearButton, &QPushButton::clicked, hotkeyEdit_, &QKeySequenceEdit::clear);
    connect(cancelButton_, &QPushButton::clicked, this, &QWidget::close);
    connect(confirmButton, &QPushButton::clicked, this, &PhraseEditorWindow::accept);
    hotkeyEdit_->installEventFilter(new HotkeyEditFilter(hotkeyEdit_, hotkeys_, nullptr, this));

    qApp->installEventFilter(this); // Esc 不管焦点在哪个输入框里都等同于取消
}

PhraseEditorWindow::~PhraseEditorWindow() {
    qApp->removeEventFilter(this);
}

void PhraseEditorWindow::clearFields() {
    textEdit_->clear();
    remarkEdit_->clear();
    hotkeyEdit_->clear();
}

void PhraseEditorWindow::showAtSavedPosition() {
    move(Settings::windowPos(mode_ == Mode::Add ? WindowSlot::AddPhrase : WindowSlot::EditPhrase));
    if (!isVisible()) show();
    activateWindow();
}

void PhraseEditorWindow::openForAdd(const QString &group, int insertAfterRow) {
    clearFields();
    group_ = group;
    insertAfterRow_ = insertAfterRow;
    showAtSavedPosition();
    textEdit_->setFocus();
}

void PhraseEditorWindow::openForEdit(QListWidgetItem *item) {
    clearFields();
    item_ = item;
    textEdit_->setPlainText(PhraseItem::text(item));
    remarkEdit_->setPlainText(PhraseItem::remark(item));
    hotkeyEdit_->setKeySequence(QKeySequence(PhraseItem::hotkey(item)));
    showAtSavedPosition();
    textEdit_->setFocus();
    textEdit_->selectAll();
}

void PhraseEditorWindow::forgetItem(QListWidgetItem *item) {
    if (item_ == item) item_ = nullptr;
}

void PhraseEditorWindow::accept() {
    if (mode_ == Mode::Add) acceptAdd();
    else acceptEdit();
}

void PhraseEditorWindow::acceptAdd() {
    const QKeySequence sequence = hotkeyEdit_->keySequence();
    if (!sequence.isEmpty() && !validateHotkey(sequence, hotkeys_, hotkeyEdit_)) {
        hotkeyEdit_->clear(); // 窗口不关，让用户重新录
        return;
    }
    const QString text = textEdit_->toPlainText();
    if (!text.isEmpty()) {
        PhraseList *list = mainWindow_->phraseList();
        QListWidgetItem *item = PhraseItem::create(text, remarkEdit_->toPlainText(), group_, sequence.toString());
        if (insertAfterRow_ >= 0 && insertAfterRow_ < list->count()) list->insertItem(insertAfterRow_ + 1, item);
        else list->addItem(item);
        mainWindow_->commitPhraseChanges();
    }
    close();
}

void PhraseEditorWindow::acceptEdit() {
    if (!item_) {
        close();
        return;
    }
    const QKeySequence sequence = hotkeyEdit_->keySequence();
    const GlobalHotkey *ownHotkey = hotkeys_.phraseHotkeyAt(mainWindow_->phraseList()->row(item_)); // 和自己原来的快捷键不算冲突
    if (!sequence.isEmpty() && !validateHotkey(sequence, hotkeys_, hotkeyEdit_, ownHotkey)) {
        hotkeyEdit_->setKeySequence(QKeySequence(PhraseItem::hotkey(item_)));
        return;
    }
    PhraseItem::setHotkey(item_, sequence.toString());
    const QString text = textEdit_->toPlainText();
    if (!text.isEmpty()) PhraseItem::setText(item_, text);
    PhraseItem::setRemark(item_, remarkEdit_->toPlainText());
    PhraseItem::refreshDisplay(item_);
    mainWindow_->commitPhraseChanges();
    close();
}

bool PhraseEditorWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() != QEvent::KeyPress || !isActiveWindow()) return QWidget::eventFilter(watched, event);
    const auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Escape) {
        cancelButton_->click();
        return true;
    }
    // Ctrl+回车等同于点“添加/修改”。快捷键输入框里不拦：Ctrl+Return 本身就能录成短语快捷键
    const bool isEnter = keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter;
    if (isEnter && (keyEvent->modifiers() & ~Qt::KeypadModifier) == Qt::ControlModifier && !hotkeyEdit_->hasFocus()) {
        accept();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void PhraseEditorWindow::moveEvent(QMoveEvent *event) {
    QWidget::moveEvent(event);
    // 只记用户拖动的位置；打开前程序自己 move 时窗口还没激活
    if (Settings::writesFrozen() || !isActiveWindow()) return;
    Settings::setWindowPos(mode_ == Mode::Add ? WindowSlot::AddPhrase : WindowSlot::EditPhrase, pos());
    Settings::saveLater();
}

namespace {

// 帮助窗口里按 Esc 不关闭窗口
class NoEscWidget : public QWidget {
  public:
    using QWidget::QWidget;

  protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape) {
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }
};

const char *const kHelpStyle = R"(
    <style>
      body{
        color:#222222;
        font-family: "Inter","Segoe UI","Microsoft YaHei UI","Microsoft YaHei","Source Han Sans SC","PingFang SC","Helvetica Neue",Arial,sans-serif,"Segoe UI Symbol","Euphemia","Gadugi";
        font-size:14px;
        line-height:1.55;
      }
      h2{
        margin:0 0 10px 0;
        font-size:20px;
      }
      h3{
        margin:18px 0 8px 0;
        font-size:16px;
      }
      p{
        margin:6px 0;
      }
      table{
        border-collapse:collapse;
        width:100%;
      }
      th,td{
        border:1px solid #dddddd;
        padding:6px 8px;
        text-align:left;
        vertical-align:top;
      }
      th{
        background:#f3f5f7;
      }
      code{
        color:#9a3412;
        background:#fff3e8;
        padding:1px 4px;
        border-radius:3px;
        font-family:"Consolas","Microsoft YaHei",monospace;
      }
      ol{
        margin-top:6px;
        padding-left:22px;
      }
    </style>
)";

const char *const kBasicTutorial = R"(
    <h2>什么是高级输入？</h2>
    <p>在短语中插入标签，让QuickSay除了输入文字，还能按键、停顿、粘贴图片/文件。</p>

    <h3>常用写法</h3>
    <table>
      <tr><th>写法</th><th>效果</th></tr>
      <tr><td><code>&lt;img 图片路径&gt;</code></td><td>粘贴图片</td></tr>
      <tr><td><code>&lt;file 文件路径&gt;</code></td><td>粘贴文件</td></tr>
      <tr><td><code>&lt;Enter&gt;</code></td><td>按回车。相当于您在键盘按下回车键</td></tr>
      <tr><td><code>&lt;sleep&gt;</code></td><td>停顿1秒</td></tr>
      <tr><td><code>&lt;sleep 2s&gt;</code></td><td>停顿2秒</td></tr>
      <tr><td><code>&lt;press A&gt;</code></td><td>按单个按键</td></tr>
      <tr><td><code>&lt;press Ctrl+Shift+A&gt;</code></td><td>按组合快捷键</td></tr>
    </table>

    <h3>使用示例（以微信为例）</h3>
    <p>写法1：<br>
      <code>感谢使用QuickSay！&lt;Enter&gt;如果觉得好用记得点个Star！&lt;Enter&gt;</code>
    </p>
    <p>效果：连发两条消息，</p>
    <ol>
      <li>第一条为“感谢使用QuickSay！”。</li>
      <li>第二条为“如果觉得好用记得点个Star！”。</li>
    </ol>
    <p>写法2：<br>
      <code>输入图片&lt;Enter&gt;&lt;img C:\图片.png&gt;&lt;Enter&gt;&lt;sleep&gt;输入结束&lt;Enter&gt;</code>
    </p>
    <p>效果：连发三条消息，</p>
    <ol>
      <li>第一条为“输入图片”。</li>
      <li>第二条为图片。</li>
      <li>停顿1秒。微信上传图片需要时间，因此可以使用<code>&lt;sleep&gt;</code>手动指定停顿时间，等待图片上传完成后再执行后续操作，防止文字与图片顺序错乱。</li>
      <li>第三条为“输入结束”。</li>
    </ol>

    <h3>补充规则</h3>
    <ol>
      <li>如果标签不符合规则，会原样输入。</li>
      <li>如果想原样输入<code>&lt;Enter&gt;</code>，请写成<code>\&lt;Enter&gt;</code>。</li>
      <li>标签不区分大小写。也就是说<code>&lt;Enter&gt;</code>可以写成<code>&lt;enter&gt;</code>。</li>
      <li>每执行一次高级输入，会按照设置里的“高级输入间隔”停顿一下。</li>
    </ol>

    <p><a href="quicksay://full-tutorial">查看更完整的教程</a></p>
)";

const char *const kFullTutorial = R"(
    <p><a href="quicksay://basic-tutorial">↩返回基础教程</a></p>

    <h3>&lt;press&gt;相关</h3>
    <p>可以写成<code>&lt;Enter&gt;</code>这种简写的按键：</p>
    <table>
      <tr><th>按键</th><th>别名</th><th>说明</th></tr>
      <tr><td><code>Enter</code></td><td><code>Return</code></td><td>回车键</td></tr>
      <tr><td><code>Space</code></td><td>无</td><td>空格键</td></tr>
      <tr><td><code>Tab</code></td><td>无</td><td>Tab键</td></tr>
      <tr><td><code>Esc</code></td><td><code>Escape</code></td><td>Esc键</td></tr>
      <tr><td><code>Backspace</code></td><td>无</td><td>退格键</td></tr>
      <tr><td><code>Insert</code></td><td><code>Ins</code></td><td>Insert键</td></tr>
      <tr><td><code>Left</code></td><td>无</td><td>左方向键</td></tr>
      <tr><td><code>Right</code></td><td>无</td><td>右方向键</td></tr>
      <tr><td><code>Up</code></td><td>无</td><td>上方向键</td></tr>
      <tr><td><code>Down</code></td><td>无</td><td>下方向键</td></tr>
      <tr><td><code>Ctrl</code></td><td><code>Control</code></td><td>Ctrl键</td></tr>
      <tr><td><code>Shift</code></td><td>无</td><td>Shift键</td></tr>
      <tr><td><code>Alt</code></td><td>无</td><td>Alt键</td></tr>
      <tr><td><code>Win</code></td><td><code>Meta</code>、<code>Windows</code></td><td>Windows键</td></tr>
    </table><br>
    <p>只能写在<code>&lt;press ...&gt;</code>里的按键：</p>
    <table>
      <tr><th>按键</th><th>说明</th></tr>
      <tr><td><code>A</code>~<code>Z</code></td><td>无</td></tr>
      <tr><td><code>0</code>~<code>9</code></td><td>无</td></tr>
      <tr><td><code>F1</code>~<code>F24</code></td><td>无</td></tr>
      <tr><td><code>=</code> <code>-</code> <code>[</code> <code>]</code> <code>;</code> <code>'</code> <code>,</code> <code>.</code> <code>/</code> <code>\</code> <code>`</code></td><td>不用按Shift就能打出来的标点键。例如<code>&lt;press Ctrl+Shift+=&gt;</code></td></tr>
      <tr><td>组合快捷键</td><td>例如<code>&lt;press Ctrl+Shift+A&gt;</code></td></tr>
      <tr><td>上面提到的可以简写的所有按键</td><td>例如<code>&lt;press Enter&gt;</code></td></tr>
    </table><br>
    <p>组合快捷键说明：</p>
    <ol>
      <li>修饰键可以写<code>Ctrl</code>（可以写成<code>Control</code>）、<code>Shift</code>、<code>Alt</code>、<code>Win</code>（可以写成<code>Meta</code>或<code>Windows</code>）。</li>
      <li>修饰键的顺序无所谓。</li>
      <li>每个组合快捷键最多只能有一个主键。例如<code>&lt;press Ctrl+A&gt;</code>可以，<code>&lt;press Ctrl+A+B&gt;</code>不可以。</li>
    </ol>

    <h3>&lt;sleep&gt;相关</h3>
    <ol>
      <li><code>&lt;sleep 2s&gt;</code>可以写成<code>&lt;sleep 2&gt;</code>。</li>
      <li>sleep最小停顿时间是毫秒。例如<code>&lt;sleep 100ms&gt;</code>可以，<code>&lt;sleep 100.5ms&gt;</code>不可以。</li>
      <li><code>&lt;sleep 0.1005s&gt;</code>可以，此时它会四舍五入，相当于<code>&lt;sleep 0.101s&gt;</code>。</li>
      <li>如果停顿时间不符合规则，会原样输入。</li>
    </ol>

    <h3>&lt;img&gt;、&lt;file&gt;相关</h3>
    <p>图片、文件路径说明：</p>
    <ol>
      <li>只能写绝对路径。</li>
      <li>不能写文件夹路径。</li>
      <li>可以加引号。例如<code>&lt;img "C:\图片.png"&gt;</code>。</li>
      <li>如果路径不存在，会原样输入。</li>
    </ol>
)";

QString helpPage(const char *body) {
    return QString("<html><head>%1</head><body>%2</body></html>").arg(QString::fromUtf8(kHelpStyle), QString::fromUtf8(body));
}

} // namespace

void showAdvancedInputHelp(QWidget *parent) {
    auto *window = new NoEscWidget();
    window->setStyleSheet(StyleSheets::kApp);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setWindowTitle("如何更高级地输入？");
    window->setWindowIcon(parent->windowIcon());
    window->setFixedSize(500, 500);

    auto *browser = new QTextBrowser(window);
    browser->setReadOnly(true);
    browser->setOpenLinks(false);
    browser->setGeometry(0, 0, 500, 500);
    browser->setHtml(helpPage(kBasicTutorial));
    browser->setLineWrapMode(QTextEdit::WidgetWidth);
    browser->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    QObject::connect(browser, &QTextBrowser::anchorClicked, window, [browser](const QUrl &url) {
        if (url == QUrl("quicksay://full-tutorial")) browser->setHtml(helpPage(kFullTutorial));
        else if (url == QUrl("quicksay://basic-tutorial")) browser->setHtml(helpPage(kBasicTutorial));
    });

    window->move(parent->x() + 30, parent->y() + 30);
    window->show();
    window->raise();
    window->activateWindow();
}
