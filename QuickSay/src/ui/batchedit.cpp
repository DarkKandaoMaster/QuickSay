#include "ui/batchedit.h"

#include "core/groupmodel.h"
#include "core/phrase.h"
#include "output/outputrunner.h"
#include "ui/grouppanel.h"
#include "ui/mainwindow.h"
#include "ui/phraselist.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>

namespace {

bool g_active = false;
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

// 跟着 QProcess 一起销毁
struct Session {
    MainWindow *mainWindow;
    QString group;
    QString separator;
    QTemporaryDir *tempDir;
    QString filePath;
    QProcess *process;
    bool finished = false; // 启动失败和进程退出两条路共用资源，只能清理一次
};

// 内容读回内存后立刻删掉整个临时目录，不在临时文件夹里留下短语正文
void releaseResources(Session *session) {
    delete session->tempDir;
    session->tempDir = nullptr;
    session->process->deleteLater();
}

void unlock(Session &session) {
    g_active = false;
    session.mainWindow->setAllWindowsEnabled(true);
}

void applyEditedText(MainWindow *mainWindow, const QString &group, const QStringList &texts) {
    for (const QString &text : texts) {
        if (text.isEmpty()) {
            QMessageBox::warning(mainWindow, "短语结构无效", "编辑结果中出现了空短语，可能是分隔线被移动或连续放置。为避免破坏数据，本次修改没有应用。  ");
            return;
        }
    }
    const QVector<QListWidgetItem *> items = itemsInGroup(mainWindow->phraseList(), group);
    if (texts.count() != items.count()) {
        QMessageBox::warning(mainWindow, "短语结构已变化", QString("原有 %1 条短语，编辑后识别出 %2 条。可能有分隔线或短语项被增删。\n\n为避免备注和快捷键错位，本次修改没有应用。").arg(items.count()).arg(texts.count()));
        return;
    }
    const QString question = QString("记事本已关闭。确定用编辑后的内容替换分组“%1”下的全部 %2 条短语吗？\n\n备注和快捷键会保留，此操作无法撤销。").arg(group).arg(items.count());
    if (!confirm(mainWindow, "确认替换短语", question)) return;
    for (int i = 0; i < items.count(); ++i) {
        PhraseItem::setText(items[i], texts[i]); // 只按原顺序换正文，备注、分组、快捷键不动
        PhraseItem::refreshDisplay(items[i]);
    }
    mainWindow->commitPhraseChanges();
}

void onNotepadFinished(Session *session, int exitCode, QProcess::ExitStatus exitStatus) {
    if (session->finished) return;
    session->finished = true;
    unlock(*session);
    MainWindow *mainWindow = session->mainWindow;
    const QString group = session->group;
    const QString separator = session->separator;
    const QString filePath = session->filePath;

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        releaseResources(session);
        QMessageBox::warning(mainWindow, "批量编辑未完成", "记事本没有正常退出，未修改任何短语。  ");
        return;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        const QString error = file.errorString();
        releaseResources(session);
        QMessageBox::warning(mainWindow, "批量编辑未完成", QString("无法读取编辑后的临时文件：%1").arg(error));
        return;
    }
    QByteArray data = file.readAll();
    file.close();
    if (data.startsWith(kUtf8Bom)) data.remove(0, 3);
    QString text = QString::fromUtf8(data);
    text.replace("\r\n", "\n");
    text.replace('\r', '\n');
    QStringList texts;
    if (!text.isEmpty()) texts = text.split("\n" + separator + "\n", Qt::KeepEmptyParts);
    releaseResources(session);
    applyEditedText(mainWindow, group, texts);
}

} // namespace

namespace BatchEdit {

bool isActive() {
    return g_active;
}

void start(MainWindow *mainWindow, int groupIndex) {
    GroupModel &groups = mainWindow->groups();
    if (groupIndex < 0 || groupIndex >= groups.count()) return;
    if (g_active) {
        QMessageBox::information(mainWindow, "提示", "已有一个分组正在记事本中编辑，请先保存并关闭该记事本。  ");
        return;
    }
    // 从这里起锁住所有窗口，记事本关闭前这个分组不会被改名或删除
    const QString group = groups.name(groupIndex);
    const QString warning = "即将在记事本中批量编辑该分组下的全部短语。\n\n"
                            "短语之间会插入带有“请勿修改”字样的分隔线。修改、删除或复制分隔线，或者增删短语，都可能破坏短语项的结构。为保护备注和快捷键，QuickSay检测到短语数量变化时不会应用修改。\n\n"
                            "建议先在设置中导出备份。是否继续？";
    if (!confirm(mainWindow, "批量编辑短语", warning)) return;

    // 带上时间戳，免得正文里恰好有同样一行被误拆
    const QString separator = QString("========== QuickSay短语分隔线_%1（请勿修改） ==========").arg(QDateTime::currentMSecsSinceEpoch());
    QStringList texts;
    for (QListWidgetItem *item : itemsInGroup(mainWindow->phraseList(), group))
        texts.append(PhraseItem::text(item));

    auto *tempDir = new QTemporaryDir(QDir::tempPath() + "/QuickSay-editor-XXXXXX");
    if (!tempDir->isValid()) {
        delete tempDir;
        QMessageBox::warning(mainWindow, "无法批量编辑", "无法创建临时编辑文件，请稍后重试。  ");
        return;
    }
    const QString filePath = tempDir->filePath("QuickSay批量编辑短语.txt");
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        const QString error = file.errorString();
        delete tempDir;
        QMessageBox::warning(mainWindow, "无法批量编辑", QString("无法写入临时编辑文件：%1").arg(error));
        return;
    }
    const QByteArray content = kUtf8Bom + texts.join("\n" + separator + "\n").toUtf8(); // 带 BOM，Win10/11 记事本都能稳定识别成 UTF-8
    if (file.write(content) != content.size()) {
        const QString error = file.errorString();
        file.close();
        delete tempDir;
        QMessageBox::warning(mainWindow, "无法批量编辑", QString("无法完整写入临时编辑文件：%1").arg(error));
        return;
    }
    file.close();

    auto *session = new Session{mainWindow, group, separator, tempDir, filePath, new QProcess(qApp)};
    QObject::connect(session->process, &QObject::destroyed, [session]() {
        delete session;
    });
    OutputRunner::stop(); // 上一段高级输入还没结束的话，绝不能让它继续粘进记事本
    g_active = true;
    mainWindow->setAllWindowsEnabled(false);
    mainWindow->groupPanel()->hide();
    // 必须在 start 之前连好，记事本秒退时才不会漏掉信号、让窗口永久锁死
    QObject::connect(session->process, &QProcess::finished, session->process, [session](int exitCode, QProcess::ExitStatus exitStatus) {
        onNotepadFinished(session, exitCode, exitStatus);
    });
    session->process->start("notepad.exe", QStringList() << QDir::toNativeSeparators(filePath));
    if (session->process->waitForStarted(3000)) return;

    if (session->finished) return; // 进程已经秒退，退出处理里清理过了
    session->finished = true;
    const QString error = session->process->errorString();
    unlock(*session);
    releaseResources(session);
    QMessageBox::warning(mainWindow, "无法批量编辑", QString("无法启动记事本：%1").arg(error));
}

} // namespace BatchEdit
