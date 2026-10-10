#include "core/backup.h"
#include "core/groupmodel.h"
#include "core/phrase.h"
#include "core/settings.h"
#include "core/version.h"
#include "output/outputparser.h"

#include <QApplication>
#include <QDir>
#include <QJsonObject>
#include <QListWidget>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include <windows.h>

class CoreTest : public QObject {
    Q_OBJECT

  private:
    QTemporaryDir tempDir_;
    QString imagePath_;

    static QVector<OutputAction> parse(const QString &text) {
        return parseOutputActions(text);
    }

    static void writeConfig(const QByteArray &content) {
        QFile file(QCoreApplication::applicationDirPath() + "/config.json");
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(content);
    }

  private slots:
    void initTestCase() {
        QVERIFY(tempDir_.isValid());
        imagePath_ = tempDir_.filePath("pic.png");
        QFile file(imagePath_);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not really a png"); // <img> 只要求文件存在
    }

    void plainTextIsOneAction() {
        const auto actions = parse("你好 world");
        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions[0].type, OutputAction::Type::Text);
        QCOMPARE(actions[0].text, QString("你好 world"));
    }

    void shorthandKeyIsCaseInsensitive() {
        const auto actions = parse("a<enter>b<Tab>");
        QCOMPARE(actions.size(), 4);
        QCOMPARE(actions[1].type, OutputAction::Type::Press);
        QCOMPARE(actions[1].key, ushort(VK_RETURN));
        QCOMPARE(actions[3].key, ushort(VK_TAB));
    }

    void escapedTagStaysText() {
        const auto actions = parse("\\<Enter>");
        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions[0].text, QString("<Enter>"));
    }

    void invalidTagsStayText_data() {
        QTest::addColumn<QString>("text");
        QTest::newRow("combo needs press") << "<Ctrl+C>";
        QTest::newRow("two primary keys") << "<press Ctrl+A+B>";
        QTest::newRow("duplicate modifier") << "<press Ctrl+Ctrl+A>";
        QTest::newRow("empty part") << "<press Ctrl+>";
        QTest::newRow("shifted punctuation") << "<press !>";
        QTest::newRow("punctuation shorthand") << "<=>";
        QTest::newRow("fractional ms") << "<sleep 100.5ms>";
        QTest::newRow("negative sleep") << "<sleep -1>";
        QTest::newRow("missing file") << "<img D:\\definitely\\not\\here.png>";
        QTest::newRow("relative file") << "<img pic.png>";
        QTest::newRow("unknown tag") << "<b>";
        QTest::newRow("unclosed") << "<Enter";
    }
    void invalidTagsStayText() {
        QFETCH(QString, text);
        const auto actions = parse(text);
        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions[0].type, OutputAction::Type::Text);
        QCOMPARE(actions[0].text, text);
    }

    void pressCombo() {
        const auto actions = parse("<press Shift+ctrl+c>");
        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions[0].type, OutputAction::Type::Press);
        QCOMPARE(actions[0].modifiers, (QVector<ushort>{VK_LCONTROL, VK_LSHIFT})); // 按固定顺序按下，和书写顺序无关
        QCOMPARE(actions[0].key, ushort('C'));
    }

    void pressPunctuation_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("vk");
        // 按当前键盘布局取键，测试机是美式/中文布局
        QTest::newRow("=") << "<press Ctrl+Shift+=>" << int(VK_OEM_PLUS);
        QTest::newRow("-") << "<press Ctrl+->" << int(VK_OEM_MINUS);
        QTest::newRow("[") << "<press [>" << int(VK_OEM_4);
        QTest::newRow("backslash") << "<press Ctrl+\\>" << int(VK_OEM_5);
        QTest::newRow("`") << "<press Alt+`>" << int(VK_OEM_3);
        QTest::newRow("F24") << "<press F24>" << int(VK_F24);
    }
    void pressPunctuation() {
        QFETCH(QString, text);
        QFETCH(int, vk);
        const auto actions = parse(text);
        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions[0].type, OutputAction::Type::Press);
        QCOMPARE(int(actions[0].key), vk);
    }

    void sleepDurations_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("ms");
        QTest::newRow("default") << "<sleep>" << 1000;
        QTest::newRow("ms") << "<sleep 500ms>" << 500;
        QTest::newRow("seconds") << "<sleep 1.5s>" << 1500;
        QTest::newRow("bare number is seconds") << "<sleep 2>" << 2000;
        QTest::newRow("rounded") << "<sleep 0.1005s>" << 101;
        QTest::newRow("case") << "<SLEEP 3S>" << 3000;
    }
    void sleepDurations() {
        QFETCH(QString, text);
        QFETCH(int, ms);
        const auto actions = parse(text);
        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions[0].type, OutputAction::Type::Sleep);
        QCOMPARE(actions[0].sleepMs, ms);
    }

    void imageAndFileTags() {
        const auto actions = parse(QString("a<img %1>b<file \"%1\">").arg(imagePath_));
        QCOMPARE(actions.size(), 4);
        QCOMPARE(actions[1].type, OutputAction::Type::File);
        QCOMPARE(QFileInfo(actions[1].filePath), QFileInfo(imagePath_));
        QCOMPARE(actions[3].type, OutputAction::Type::File);
    }

    void findImageTagsMatchesOutputRules() {
        const QString text = QString("x<img %1>y\\<img %1><file %1><IMG  %1 >").arg(imagePath_);
        const QVector<ImageTag> tags = findImageTags(text);
        QCOMPARE(tags.size(), 2); // 转义的、<file> 都不算
        QCOMPARE(text.mid(tags[0].start, tags[0].length), QString("<img %1>").arg(imagePath_));
        QVERIFY(text.mid(tags[1].start, tags[1].length).startsWith("<IMG"));
    }

    void batchSeparatorAvoidsPhraseLines() {
        QCOMPARE(PhraseBatch::chooseSeparator({"abc", "---", " -----"}), QString("-----"));
        QCOMPARE(PhraseBatch::chooseSeparator({"a\n-----\nb", "------"}), QString("-------"));
    }

    void batchJoinSplitRoundTrip() {
        const QStringList texts = {"a", "\n开头空行", "结尾空行\n", "多行\n\n中间空行", "-----x", "------"};
        const QString separator = PhraseBatch::chooseSeparator(texts);
        QCOMPARE(PhraseBatch::split(PhraseBatch::join(texts, separator), separator), texts);
    }

    void batchSplitIsLenient() {
        // 首尾、连续的分隔线和只有空白的段都丢掉；CRLF 照认；不是正好 5 个 - 的行算正文
        const QString text = "-----\r\n-----\r\na\r\n-----\r\n  \r\n-----\r\nb\r\n ----- \r\n------\r\n-----\r\n";
        QCOMPARE(PhraseBatch::split(text, "-----"), QStringList({"a", "b\n ----- \n------"}));
        QCOMPARE(PhraseBatch::split("", "-----"), QStringList());
    }

    void batchSplitLines() {
        // 空行和只有空白的行丢掉，行首行尾的空白保留；CRLF、单独的 CR 都认；分隔线在这里只是普通的一行
        QCOMPARE(PhraseBatch::splitLines("a\r\n\r\n  \r\n b \rc\n-----\n"), QStringList({"a", " b ", "c", "-----"}));
        QCOMPARE(PhraseBatch::splitLines(""), QStringList());
    }

    void batchMatchByText() {
        // 正文相同的按出现顺序一对一认领，多出来的算新增，没被认领的旧短语（这里的 b 被改成了 c）算删掉
        const QVector<int> match = PhraseBatch::matchByText({"a", "b", "a"}, {"a", "c", "a", "a"});
        QCOMPARE(match, QVector<int>({0, -1, 2, -1}));
        QCOMPARE(PhraseBatch::matchByText({"x", "y"}, {"y", "x"}), QVector<int>({1, 0}));
    }

    void compareVersions_data() {
        QTest::addColumn<QString>("left");
        QTest::addColumn<QString>("right");
        QTest::addColumn<int>("result");
        QTest::addColumn<bool>("known");
        QTest::newRow("numeric not lexical") << "2.0.10" << "2.0.9" << 1 << true;
        QTest::newRow("missing parts are zero") << "2.0" << "2.0.0" << 0 << true;
        QTest::newRow("older") << "1.8.1" << "2.0.2" << -1 << true;
        QTest::newRow("not a version") << "v2" << "2.0" << 0 << false;
    }
    void compareVersions() {
        QFETCH(QString, left);
        QFETCH(QString, right);
        QFETCH(int, result);
        QFETCH(bool, known);
        bool actualKnown = !known;
        QCOMPARE(::compareVersions(left, right, actualKnown), result);
        QCOMPARE(actualKnown, known);
    }

    void backupRoundTrip() {
        Settings::load(); // 第一次跑会在测试程序旁边写一份默认 config.json
        GroupModel groups;
        groups.insert(0, {"A", 40, 1});
        groups.insert(1, {"B&C", 55, 3});
        QListWidget list;
        list.addItem(PhraseItem::create("第一条<Enter>", "备注", "A", "Ctrl+Num+4"));
        list.addItem(PhraseItem::create("second", "", "B&C", ""));

        const QByteArray file = Backup::create(list, groups);
        Backup::Data data;
        QString error;
        QVERIFY2(Backup::parse(file, data, error), qPrintable(error));
        QCOMPARE(data.groups, groups.toJson());
        QCOMPARE(data.phrases.size(), 2);
        QCOMPARE(data.phrases[0].toObject()["text"].toString(), QString("第一条<Enter>"));
        QCOMPARE(data.phrases[0].toObject()["hotkey"].toString(), QString("Ctrl+Num+4"));
        QCOMPARE(data.appVersion, kAppVersion);

        // Base64 正文里改一个字符（文件头固定 6 行，正文从第 7 行开始）：必须是校验和拦下来，而不是别的检查
        QByteArray tampered = file;
        int body = 0;
        for (int line = 0; line < 6; ++line)
            body = tampered.indexOf('\n', body) + 1;
        body += 10;
        tampered[body] = tampered[body] == 'A' ? 'B' : 'A';
        QVERIFY(!Backup::parse(tampered, data, error));
        QVERIFY2(error.contains("完整性校验失败"), qPrintable(error));

        // Windows 换行和末尾多余空行要能接受
        QByteArray crlf = file;
        crlf.replace("\n", "\r\n");
        QVERIFY2(Backup::parse(crlf + "\r\n", data, error), qPrintable(error));
    }

    // config.json 的键名是磁盘格式，老版本的配置必须原样迁移过来
    void settingsMigrateLegacyKeys() {
        writeConfig(R"({"hotkey":"Ctrl+Q","ziqidong_guanliyuan":true,"anquanqu_hangshu":2,"default_item_height":30,
                        "badge_key_input_phrase_when_pinned":true,"enter_key_input_phrase_when_pinned":false,"my_unknown_key":123})");
        QVERIFY(Settings::readRunAsAdminFromFile()); // 还没有 QApplication 时直接读文件，也要认老键
        Settings::load();
        QCOMPARE(Settings::hotkey(), QString("Ctrl+Q"));
        QVERIFY(Settings::runAsAdmin()); // ziqidong_guanliyuan → guanliyuan
        QCOMPARE(Settings::scrollSafePixels(), 60); // 2 行 × 默认行高 30
        QVERIFY(!Settings::badgeKeyPassthroughWhenPinned()); // 旧开关含义相反
        QVERIFY(Settings::enterKeyPassthroughWhenPinned());
        QCOMPARE(Settings::captureHotkey(), QString()); // 新设置项补默认值
        QCOMPARE(Settings::toolTipDelayMs(), 0);
        QVERIFY(!Settings::searchOnSummon());
        QVERIFY(Settings::stopOutputOnWindowSwitch());
        const QJsonObject json = Settings::json();
        QVERIFY(!json.contains("ziqidong_guanliyuan"));
        QVERIFY(!json.contains("anquanqu_hangshu"));
        QVERIFY(!json.contains("badge_key_input_phrase_when_pinned"));
        QCOMPARE(json.value("my_unknown_key").toInt(), 123); // 不认识的键原样保留
    }

    void settingsOldDefaultSafeRowsBecomesNewDefault() {
        writeConfig(R"({"anquanqu_hangshu":1,"default_item_height":30})");
        Settings::load();
        QCOMPARE(Settings::scrollSafePixels(), 60); // 老默认 1 行不按行高折算，直接换成新默认
    }

    void settingsBrokenFileIsKeptAside() {
        writeConfig("{ this is not json");
        Settings::load();
        QCOMPARE(Settings::hotkey(), QString("Ctrl+Shift+V")); // 换成了默认设置
        const QStringList broken = QDir(QCoreApplication::applicationDirPath()).entryList({"config.json.broken.*"});
        QCOMPARE(broken.size(), 1); // 原文件改名留底，没被覆盖
        QFile file(QDir(QCoreApplication::applicationDirPath()).filePath(broken.first()));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("{ this is not json"));
        file.close();
        file.remove();
    }

    void backupRejectsPhraseInMissingGroup() {
        GroupModel groups;
        groups.insert(0, {"A", 40, 1});
        QListWidget list;
        list.addItem(PhraseItem::create("orphan", "", "不存在的分组", ""));
        Backup::Data data;
        QString error;
        QVERIFY(!Backup::parse(Backup::create(list, groups), data, error));
    }
};

QObject *createUiTest(); // tst_ui.cpp

// 两组测试跑在同一个程序里。窗口子系统的 exe 在命令行里看不到输出，所以默认各写一个结果文件到 build/
int main(int argc, char **argv) {
    QApplication app(argc, argv); // 会吃掉 -platform offscreen 之类的 Qt 参数
    const QStringList args = QCoreApplication::arguments();
    auto run = [&args](QObject *test, const QString &name) {
        QStringList testArgs = args;
        if (!testArgs.contains("-o")) testArgs << "-o" << QString("%1/build/%2.txt,txt").arg(QCoreApplication::applicationDirPath(), name);
        return QTest::qExec(test, testArgs);
    };
    CoreTest core;
    int failures = run(&core, "result_core");
    std::unique_ptr<QObject> ui(createUiTest());
    failures += run(ui.get(), "result_ui");
    return failures;
}

#include "tst_core.moc"
