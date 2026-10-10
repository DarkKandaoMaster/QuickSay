#include "ui/batchedit.h"

#include "core/groupmodel.h"
#include "core/phrase.h"
#include "output/outputrunner.h"
#include "ui/grouppanel.h"
#include "ui/mainwindow.h"
#include "ui/phraselist.h"
#include "ui/popups.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>

namespace {

const QByteArray kUtf8Bom = QByteArray::fromHex("EFBBBF");

// Yes 在右、No 在左（交换了按钮角色），回车默认 Yes，Esc 是 No
bool confirm(QWidget *parent, const QString &title, const QString &text) {
    QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::NoButton, parent);
    QPushButton *yes = box.addButton("Yes", QMessageBox::RejectRole);
    QPushButton *no = box.addButton("No", QMessageBox::AcceptRole);
    box.setDefaultButton(yes);
    box.setEscapeButton(no);
    box.exec();
    return box.clickedButton() == yes;
}

QVector<QListWidgetItem *> itemsInGroup(PhraseList *list, const QString &group) {
    QVector<QListWidgetItem *> items;
    for (int i = 0; i < list->count(); ++i) {
        if (PhraseItem::group(list->item(i)) == group) items.append(list->item(i));
    }
    return items;
}

QString describe(const QListWidgetItem *item) {
    QString name = PhraseItem::remark(item);
    if (name.isEmpty()) name = PhraseItem::text(item).section('\n', 0, 0);
    if (name.size() > 30) name = name.left(30) + "…";
    if (!PhraseItem::hotkey(item).isEmpty()) name += "（快捷键 " + PhraseItem::hotkey(item) + "）";
    return name;
}

// 实测（Win11 记事本 11.2405）：不管设成在新窗口还是新标签页打开，启动的那个进程都一直活到这个文件的标签被关掉，
// 切标签、保存都不会让它退出，所以等它退出就是等用户改完
struct Session {
    MainWindow *mainWindow = nullptr;
    QString group;
    bool adding = false; // true：批量添加（空文件、每行一条、加在分组末尾），false：批量编辑
    QString separator;
    QByteArray written;
    QTemporaryDir *tempDir = nullptr;
    QString filePath;
    QProcess *process = nullptr;
};

Session *g_session = nullptr;

// 内容读回内存后立刻删掉整个临时目录，不在临时文件夹里留下短语正文
void endSession() {
    Session *session = g_session;
    if (!session) return;
    g_session = nullptr;
    delete session->tempDir;
    session->process->disconnect();
    session->process->deleteLater();
    session->mainWindow->setAllWindowsEnabled(true);
    delete session;
}

// 按正文原样对回原来的短语：没改过的原条目放回去，备注和快捷键自然都在；
// 改过正文或删掉的旧条目连同备注、快捷键一起丢掉，新正文建成新短语
void applyTexts(MainWindow *mainWindow, const QString &group, const QStringList &texts, const QVector<int> &match, const QVector<QListWidgetItem *> &oldItems) {
    PhraseList *list = mainWindow->phraseList();
    // 旧条目按列表顺序排，第一条的行号最小，摘掉其余的不会挪动它
    const int row = oldItems.isEmpty() ? list->count() : list->row(oldItems.first());
    for (QListWidgetItem *item : oldItems)
        list->takeItem(list->row(item));
    QVector<bool> kept(oldItems.size(), false);
    for (int i = 0; i < texts.size(); ++i) {
        QListWidgetItem *item = nullptr;
        if (match[i] >= 0) {
            item = oldItems[match[i]];
            kept[match[i]] = true;
        } else {
            item = PhraseItem::create(texts[i], QString(), group, QString());
        }
        list->insertItem(row + i, item);
    }
    for (int i = 0; i < oldItems.size(); ++i) {
        if (!kept[i]) mainWindow->discardPhrase(oldItems[i]);
    }
    mainWindow->commitPhraseChanges();
}

void onNotepadFinished() {
    Session *session = g_session;
    if (!session) return;
    MainWindow *mainWindow = session->mainWindow;
    const QString group = session->group;
    const bool adding = session->adding;
    const QString separator = session->separator;
    const QByteArray written = session->written;
    QFile file(session->filePath);
    const bool opened = file.open(QIODevice::ReadOnly);
    QByteArray data = opened ? file.readAll() : QByteArray();
    const QString error = file.errorString();
    file.close();
    endSession(); // 先解锁：主窗口禁用着，以它为父的提示框也会跟着被禁用
    if (!opened) {
        QMessageBox::warning(mainWindow, "批量编辑短语", QString("无法读取临时文件，未修改任何短语：%1").arg(error));
        return;
    }
    if (data == written) return; // 没保存就关了
    if (data.startsWith(kUtf8Bom)) data.remove(0, 3);
    if (adding) {
        // 只往末尾加、不碰原有短语，什么都不会丢，所以不用确认
        const QStringList lines = PhraseBatch::splitLines(QString::fromUtf8(data));
        if (lines.isEmpty()) return;
        mainWindow->appendPhrasesToGroup(group, lines);
        showCursorHint(QString("已添加 %1 条短语到分组“%2”").arg(lines.size()).arg(group));
        return;
    }
    const QStringList texts = PhraseBatch::split(QString::fromUtf8(data), separator);

    const QVector<QListWidgetItem *> oldItems = itemsInGroup(mainWindow->phraseList(), group);
    QStringList oldTexts;
    for (QListWidgetItem *item : oldItems)
        oldTexts.append(PhraseItem::text(item));
    if (texts == oldTexts) return;
    const QVector<int> match = PhraseBatch::matchByText(oldTexts, texts);
    QVector<bool> kept(oldItems.size(), false);
    for (int index : match) {
        if (index >= 0) kept[index] = true;
    }
    QStringList lost;
    for (int i = 0; i < oldItems.size(); ++i) {
        if (!kept[i] && (!PhraseItem::remark(oldItems[i]).isEmpty() || !PhraseItem::hotkey(oldItems[i]).isEmpty())) lost.append("· " + describe(oldItems[i]));
    }
    if (!lost.isEmpty()) {
        const int total = lost.size();
        if (total > 10) {
            lost = lost.mid(0, 10);
            lost.append(QString("……共 %1 条").arg(total));
        }
        const QString question = "以下短语删掉了或改了正文，它们的备注和快捷键会一起丢掉：\n\n" + lost.join('\n') + "\n\n确定应用吗？选 No 放弃本次全部修改。";
        if (!confirm(mainWindow, "批量编辑短语", question)) return;
    }
    applyTexts(mainWindow, group, texts, match, oldItems);
}

void openInNotepad(MainWindow *mainWindow, const QString &group, bool adding, const QString &separator, const QString &text, const QString &fileName, const QString &errorTitle) {
    if (g_session) return; // 窗口全锁着，正常走不到这里
    auto *tempDir = new QTemporaryDir(QDir::tempPath() + "/QuickSay-editor-XXXXXX");
    if (!tempDir->isValid()) {
        delete tempDir;
        QMessageBox::warning(mainWindow, errorTitle, "无法创建临时编辑文件，请稍后重试。  ");
        return;
    }
    const QString filePath = tempDir->filePath(fileName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        const QString error = file.errorString();
        delete tempDir;
        QMessageBox::warning(mainWindow, errorTitle, QString("无法写入临时编辑文件：%1").arg(error));
        return;
    }
    const QByteArray content = kUtf8Bom + text.toUtf8(); // 带 BOM，Win10/11 记事本都能稳定识别成 UTF-8
    if (file.write(content) != content.size()) {
        const QString error = file.errorString();
        file.close();
        delete tempDir;
        QMessageBox::warning(mainWindow, errorTitle, QString("无法完整写入临时编辑文件：%1").arg(error));
        return;
    }
    file.close();

    OutputRunner::stop(); // 上一段高级输入还没结束的话，绝不能让它继续粘进记事本
    // 从这里起锁住所有窗口，记事本关掉这个文件之前这个分组不会被改名或删除
    g_session = new Session{mainWindow, group, adding, separator, content, tempDir, filePath, new QProcess(qApp)};
    mainWindow->setAllWindowsEnabled(false);
    mainWindow->groupPanel()->hide();
    QProcess *process = g_session->process;
    // 必须在 start 之前连好，记事本秒退时才不会漏掉信号、让窗口永久锁死。退出码不管，只要退了就读文件
    QObject::connect(process, &QProcess::finished, process, &onNotepadFinished);
    process->start("notepad.exe", QStringList() << QDir::toNativeSeparators(filePath));
    if (process->waitForStarted(3000)) return;
    if (!g_session) return; // 已经秒退，退出处理里收过尾了
    const QString error = process->errorString();
    endSession();
    QMessageBox::warning(mainWindow, errorTitle, QString("无法启动记事本：%1").arg(error));
}

} // namespace

namespace BatchEdit {

bool isActive() {
    return g_session != nullptr;
}

void start(MainWindow *mainWindow, int groupIndex) {
    GroupModel &groups = mainWindow->groups();
    if (groupIndex < 0 || groupIndex >= groups.count()) return;
    const QString group = groups.name(groupIndex);
    QStringList texts;
    for (QListWidgetItem *item : itemsInGroup(mainWindow->phraseList(), group))
        texts.append(PhraseItem::text(item));
    const QString separator = PhraseBatch::chooseSeparator(texts);
    openInNotepad(mainWindow, group, false, separator, PhraseBatch::join(texts, separator), "QuickSay批量编辑短语.txt", "无法批量编辑");
}

void startAdd(MainWindow *mainWindow, int groupIndex) {
    GroupModel &groups = mainWindow->groups();
    if (groupIndex < 0 || groupIndex >= groups.count()) return;
    // 文件里写不了说明（会被当成短语加进去），用法放在文件名里，记事本标签页上看得到
    openInNotepad(mainWindow, groups.name(groupIndex), true, QString(), QString(), "（每行一条）QuickSay批量添加短语.txt", "无法批量添加");
}

} // namespace BatchEdit
