// 版本：1.8.0
// 更新内容：
// 1. 修改分组页面新增“每行短语数”选项。
// 2. 新增以“管理员权限启动”设置选项。勾选后就能允许QuickSay在任何地方输入。
// 3. 修复在设置里调整滚动条滚动速度后不能立即生效的问题。
// 4. 新增高级输入停止机制：锁屏、注销、会话断开、睡眠或休眠立即停止；某个输入动作失败也会停止。
// 5. 新增导入导出备份功能。
// 6. “窗口大小”更名为“主窗口大小”，该设置不再影响其他窗口。
// 7. 重新设计设置窗口，新增分页布局以及“确定”“取消”“应用”操作。
// 8. 重新设计托盘右键菜单。
// 9. 优化方向键浏览短语时的滚动体验。
// 10. 新增检查更新功能。放心，是检查更新不是自动更新，并且这个可以在设置里关掉，并且我（会尽力）保证QuickSay新版本只会比旧版本更好用。

#include <QApplication>
#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QClipboard>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QMenu>
#include <QHotkey>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QTextOption>
#include <QJsonArray>
#include <QJsonObject>
#include <QXmlStreamReader>
#include <QJsonDocument>
#include <QFile>
#include <QFileDialog>
#include <QSaveFile>
#include <QDateTime>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QScreen>
#include <QSet>
#include <QKeyEvent>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QCheckBox>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QWinEventNotifier>
#include <QTimer>
#include <QVector>
#include <QTabBar>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QInputDialog>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStylePainter>
#include <QStyleOptionTab>
#include <QScrollBar>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWindow>
#include <QTabWidget>
#include <QGroupBox>
#include <QGridLayout>
#include <QComboBox>
#include <QAbstractItemView>
#include <QImage>
#include <QPixmap>
#include <functional>
#include <QDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QAbstractNativeEventFilter>
#include <QToolTip>
#include <QFontDatabase>
#include <climits>
#include <windows.h>
#include <sddl.h>
#include <shellapi.h>
#include <shldisp.h>
#include <taskschd.h>
#include <wtsapi32.h>
#include <winhttp.h>
#include <thread>
#pragma comment(lib, "user32.lib")

QJsonObject config; // 全局对象，用于保存程序的设置
static const QString g_quickSayVersion = "1.8.0"; // 当前QuickSay版本。备份元数据和设置里的版本显示都从这里读取，避免两处忘记同步
static const QString g_backupFormatVersion = "1.0.0"; // 备份格式版本只在文件结构发生不兼容变化时才修改，不能跟着QuickSay版本一起改
bool g_zhengzaiDaoruChongqi = false; // 导入成功后直到旧进程退出都保持true，挡住旧设置窗口、焦点事件和窗口移动事件再次覆盖刚写入的备份

bool g_yuanshengCaidanZhankai = false; // 托盘的Windows原生右键菜单弹出期间为true。原生菜单不是Qt的弹出控件，QApplication::activePopupWidget()看不见它，所以要靠这个标志让键盘钩子放行按键

QWidget *pchuangkou = nullptr;
QWidget *g_shezhichuangkou = nullptr;
QWidget *g_tianjiachuangkou = nullptr;
QWidget *g_xiugaichuangkou = nullptr;
QListWidget *g_liebiao = nullptr;
QTabBar *g_tabBar = nullptr;
QLineEdit *g_search = nullptr;
QPushButton *g_tuding = nullptr; // 指向主窗口右上角的图钉按钮，用于让键盘钩子能触发“切换钉住”
HHOOK g_keyboardHook = nullptr;
int g_quickSayPressBlockCount = 0;
int g_phraseCellWidth = 0; // 多列排列时每个短语项该有的宽度，由updatePhraseListColumns算出来、由BadgeDelegate::sizeHint报给列表。0表示单列，此时短语项宽度交给列表自己撑满
bool g_quickSayIsOutputting = false;
class QuickSayOutputRunner;
QuickSayOutputRunner *g_quickSayOutputRunner = nullptr; // 当前正在执行高级输入的对象。锁屏、会话断开、睡眠等系统事件靠它立即停止整段输出
bool g_searchMode = false;
HWND g_lastForegroundBeforeSearch = nullptr;

void saveConfig(const QString &configPath) { // 写入程序设置到config.json
    if (g_zhengzaiDaoruChongqi) return; // 导入后的旧全局config已经过期，重启结束前绝不能再拿它覆盖刚导入的config.json
    QJsonDocument doc(config); // 把全局对象config转换成JSON文档
    QFile file(configPath); // 打开指定路径文件config.json
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) { // 如果config.json成功打开（写模式）
        file.write(doc.toJson()); // 写入JSON文档到本地
        file.close(); // 关闭文件
    }
}

void loadConfig(const QString &configPath) { // 读取config.json到程序设置。如果config.json不存在，那么读取默认设置，同时写入默认设置到config.json
    QFile file(configPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) { // 如果config.json成功打开（读模式）
        QByteArray data = file.readAll(); // 把config.json所有内容读取到data
        file.close(); // 关闭文件
        QJsonDocument doc = QJsonDocument::fromJson(data); // 把读取到的数据转换成JSON文档
        if (doc.isObject()) { // 确认文档是一个对象
            config = doc.object(); // 取出JSON对象，把这个对象赋值给全局对象config
            // 兼容旧版本
            if (!config.contains("delay")) config["delay"] = 200; // 如果config里没有delay，那么默认高级输入间隔200毫秒
            if (!config.contains("default_item_height")) config["default_item_height"] = 40; // 如果config里没有default_item_height，那么默认短语项高度40
            if (!config.contains("phrase_item_padding_horizontal")) config["phrase_item_padding_horizontal"] = 16; // 如果config里没有phrase_item_padding_horizontal，那么默认短语项左右内边距16
            if (!config.contains("phrase_item_padding_vertical")) config["phrase_item_padding_vertical"] = 12; // 如果config里没有phrase_item_padding_vertical，那么默认短语项上下内边距12
            if (!config.contains("gundong")) config["gundong"] = 10; // 如果config里没有gundong，那么默认滚动条滚动速度10
            if (!config.contains("badge_key_input_phrase_when_pinned")) config["badge_key_input_phrase_when_pinned"] = false; // 如果config里没有badge_key_input_phrase_when_pinned，那么默认钉住窗口时按下短语项对应角标不输入短语
            if (!config.contains("enter_key_input_phrase_when_pinned")) config["enter_key_input_phrase_when_pinned"] = false; // 如果config里没有enter_key_input_phrase_when_pinned，那么默认钉住窗口时按下回车键不输入短语
            if (!config.contains("guanliyuan")) config["guanliyuan"] = config["ziqidong_guanliyuan"].toBool(false); // 如果config里没有guanliyuan，那么沿用老版本里“以管理员权限开机自启”的值（1.8.0以前这两件事是绑在一起的，现在拆成了独立选项）
            if (!config.contains("shezhichuangkou_w")) config["shezhichuangkou_w"] = 620; // 如果config里没有shezhichuangkou_w，那么默认设置窗口外框宽度620（和TrafficMonitor中文设置窗口一致）
            if (!config.contains("shezhichuangkou_h")) config["shezhichuangkou_h"] = 576; // 如果config里没有shezhichuangkou_h，那么默认设置窗口外框高度576
            if (!config.contains("qidong_jiancha_gengxin")) config["qidong_jiancha_gengxin"] = true; // 如果config里没有qidong_jiancha_gengxin，那么默认启动时检查更新
            if (!config.contains("hulve_banben")) config["hulve_banben"] = ""; // 如果config里没有hulve_banben，那么默认一个版本都没忽略过
            config.remove("ziqidong_guanliyuan"); // 老键名读过一次就清掉，免得两个键一起留在config.json里让人分不清哪个在起作用
        }
    } else { // 如果config.json不存在
        config["hotkey"] = "Ctrl+Shift+V"; // 默认全局快捷键【【【注：想修改默认设置在这里修改】】】
        config["zhiding"] = true; // 默认主窗口始终置顶
        // config["clipboard"]=true;//默认输入时也将短语复制到剪贴板
        config["delay"] = 200; // 默认高级输入间隔200毫秒
        config["width"] = 500; // 默认主窗口宽度
        config["height"] = 500; // 默认主窗口高度
        config["default_item_height"] = 40; // 默认短语项高度40
        config["phrase_item_padding_horizontal"] = 16; // 默认短语项左右内边距16
        config["phrase_item_padding_vertical"] = 12; // 默认短语项上下内边距12
        config["gundong"] = 10; // 默认滚动条滚动速度10
        config["jiaobiao"] = false; // 默认角标放在右上角
        config["badge_key_input_phrase_when_pinned"] = false; // 默认钉住窗口时按下短语项对应角标不输入短语
        config["enter_key_input_phrase_when_pinned"] = false; // 默认钉住窗口时按下回车键不输入短语
        config["ziqidong"] = true; // 默认开机自启动
        config["guanliyuan"] = false; // 默认不以管理员权限启动
        config["tudingflag"] = true; // 默认钉住窗口
        config["chuangkou_x"] = (QGuiApplication::primaryScreen()->geometry().width() - 500) / 2; // chuangkou默认显示位置 //获取屏幕的宽高，然后 (屏幕宽度-窗口宽度)/2 ，于是就获得了能让窗口在x轴上居中显示的位置
        config["chuangkou_y"] = (QGuiApplication::primaryScreen()->geometry().height() - 500) / 2;
        config["shezhichuangkou_x"] = (QGuiApplication::primaryScreen()->geometry().width() - 500) / 2 + 501; // shezhichuangkou默认显示位置。加上501是为了不让它和主窗口重叠
        config["shezhichuangkou_y"] = (QGuiApplication::primaryScreen()->geometry().height() - 500) / 2;
        config["shezhichuangkou_w"] = 620; // 设置窗口默认外框宽度，和TrafficMonitor中文设置窗口一致
        config["shezhichuangkou_h"] = 576; // 设置窗口默认外框高度
        config["qidong_jiancha_gengxin"] = true; // 默认启动时检查更新
        config["hulve_banben"] = ""; // 默认一个版本都没忽略过。用户在更新弹窗上点了“忽略该版本”，这里才会记下那个版本号
        config["tianjiachuangkou_x"] = (QGuiApplication::primaryScreen()->geometry().width() - 500) / 2 + 501; // tianjiachuangkou默认显示位置
        config["tianjiachuangkou_y"] = (QGuiApplication::primaryScreen()->geometry().height() - 500) / 2;
        config["xiugaichuangkou_x"] = (QGuiApplication::primaryScreen()->geometry().width() - 500) / 2 + 501; // xiugaichuangkou默认显示位置
        config["xiugaichuangkou_y"] = (QGuiApplication::primaryScreen()->geometry().height() - 500) / 2;
        saveConfig(configPath); // 写入默认设置到config.json
    }
}

//====================开机自启动====================
// 普通权限自启走注册表Run项，管理员权限自启走计划任务。之所以用计划任务，是因为只有它能做到“登录Windows时直接以最高权限启动程序、而且不弹UAC”——UAC只在创建任务的那一次弹
// 注册表项和计划任务必须互斥：两个同时留着的话，开机会启动两个QuickSay实例
// config里那两个开关是互相独立的：ziqidong管开机自不自启，guanliyuan管以不以管理员权限启动。两个都开着才走计划任务；只开guanliyuan的话开机不自启，手动启动时自己提权（见下面“以管理员权限启动”那一节）
static const QString g_ziqidongRegPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"; // 注册表Run项路径
static const QString g_ziqidongRegName = "QuickSay"; // 注册表Run项里用的键名
static const wchar_t *g_ziqidongRenwuMing = L"QuickSay 管理员自启"; // 计划任务名，直接建在「任务计划程序库」的根目录下

QString ziqidongExePath() { // 可执行文件的完整路径（Windows风格的反斜杠）
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH); // 这里不用QCoreApplication::applicationFilePath()，因为专门去创建计划任务的那个提权实例还没有创建QApplication对象
    return QString::fromWCharArray(buf);
}

QString ziqidongRegValue() { // 注册表Run项里要写的值：带引号的exe路径 + 后台启动标记
    return QString("\"%1\" --autostart").arg(ziqidongExePath());
}

bool isProcessElevated() { // 当前进程是不是以管理员权限运行的
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation = {0};
    DWORD len = 0;
    bool result = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &len) && elevation.TokenIsElevated;
    CloseHandle(token);
    return result;
}

QString ziqidongYonghuMing() { // 取「域\用户名」。计划任务的登录触发器和运行身份都要用它，这样任务才只在这个用户登录时触发
    wchar_t name[256] = {0};
    wchar_t domain[256] = {0};
    GetEnvironmentVariableW(L"USERNAME", name, 256);
    GetEnvironmentVariableW(L"USERDOMAIN", domain, 256);
    return QString::fromWCharArray(domain) + "\\" + QString::fromWCharArray(name);
}

QString ziqidongRenwuSddl() { // 计划任务的安全描述符：System和管理员组完全控制，当前用户也给完全控制
    // 给当前用户完全控制是关键：这样没提权的QuickSay自己就能删掉/改掉这个任务，关闭管理员自启时不用再弹一次UAC。抄自PowerToys的auto_start_helper.cpp
    QString sddl = "D:(A;;FA;;;SY)(A;;FA;;;BA)";
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        BYTE info[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE] = {0};
        DWORD len = 0;
        if (GetTokenInformation(token, TokenUser, info, sizeof(info), &len)) {
            LPWSTR sid = nullptr;
            if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(info)->User.Sid, &sid)) {
                sddl += QString("(A;;FA;;;%1)").arg(QString::fromWCharArray(sid));
                LocalFree(sid);
            }
        }
        CloseHandle(token);
    }
    return sddl;
}

static ITaskFolder *openRenwuGenmulu(ITaskService **service) { // 连上计划任务服务，拿到任务库根目录。失败返回nullptr；成功的话*service和返回值这两个指针用完都要Release
    *service = nullptr;
    ITaskService *svc = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, reinterpret_cast<void **>(&svc)))) return nullptr;
    VARIANT empty;
    VariantInit(&empty); // 四个空VARIANT表示「连本机、用当前用户身份」
    if (FAILED(svc->Connect(empty, empty, empty, empty))) {
        svc->Release();
        return nullptr;
    }
    ITaskFolder *folder = nullptr;
    BSTR root = SysAllocString(L"\\");
    HRESULT hr = svc->GetFolder(root, &folder);
    SysFreeString(root);
    if (FAILED(hr)) {
        svc->Release();
        return nullptr;
    }
    *service = svc;
    return folder;
}

bool queryAdminRenwu(QString *exePath) { // 查询管理员自启计划任务。任务存在且处于启用状态就返回true，同时把任务里记着的exe路径写进exePath（用来发现程序被挪过位置）
    if (exePath) exePath->clear();
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); // Qt在主线程已经初始化过COM了，这里会返回S_FALSE或者RPC_E_CHANGED_MODE，都不影响用
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) return false;
    ITaskService *svc = nullptr;
    ITaskFolder *folder = openRenwuGenmulu(&svc);
    bool qiyong = false;
    if (folder) {
        IRegisteredTask *task = nullptr;
        BSTR name = SysAllocString(g_ziqidongRenwuMing);
        HRESULT hr = folder->GetTask(name, &task);
        SysFreeString(name);
        if (SUCCEEDED(hr) && task) {
            VARIANT_BOOL enabled = VARIANT_FALSE;
            if (SUCCEEDED(task->get_Enabled(&enabled)) && enabled == VARIANT_TRUE) qiyong = true;
            BSTR xml = nullptr;
            if (exePath && SUCCEEDED(task->get_Xml(&xml)) && xml) { // 从任务的XML里读出<Command>标签里的exe路径。用XML解析器是为了把路径里的&amp;之类实体还原成原字符
                QXmlStreamReader reader(QString::fromWCharArray(xml, SysStringLen(xml)));
                while (!reader.atEnd()) {
                    reader.readNext();
                    if (reader.isStartElement() && reader.name() == QStringLiteral("Command")) {
                        *exePath = reader.readElementText().trimmed();
                        break;
                    }
                }
                SysFreeString(xml);
            }
            task->Release();
        }
        folder->Release();
    }
    if (svc) svc->Release();
    if (SUCCEEDED(init)) CoUninitialize(); // 只有真正初始化成功了才配对反初始化，RPC_E_CHANGED_MODE时不能调
    return qiyong;
}

bool deleteAdminRenwu() { // 删掉管理员自启计划任务。任务本来就不存在也算成功。靠上面那份SDDL，这一步不需要管理员权限
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) return false;
    ITaskService *svc = nullptr;
    ITaskFolder *folder = openRenwuGenmulu(&svc);
    bool ok = false;
    if (folder) {
        BSTR name = SysAllocString(g_ziqidongRenwuMing);
        HRESULT hr = folder->DeleteTask(name, 0);
        SysFreeString(name);
        ok = SUCCEEDED(hr) || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND); // ERROR_FILE_NOT_FOUND就是任务不存在，那正合我意
        folder->Release();
    }
    if (svc) svc->Release();
    if (SUCCEEDED(init)) CoUninitialize();
    return ok;
}

bool createAdminRenwu() { // 创建/更新管理员自启计划任务。必须在提权进程里调用，否则写不进任务库根目录，而且RunLevel会被降级
    QString exe = ziqidongExePath();
    QString yonghu = ziqidongYonghuMing();
    QString exeXml = exe.toHtmlEscaped(); // 路径里万一有&就得转义，不然XML解析不了
    QString gongzuomulu = QFileInfo(exe).absolutePath().replace('/', '\\').toHtmlEscaped();
    // 直接喂一整份任务XML给ITaskFolder::RegisterTask，比一个个去接ITaskDefinition/ITrigger/IPrincipal/IExecAction简单太多（而且MinGW的taskschd.h里压根没有ILogonTrigger）
    QString xml = QString(
        "<?xml version=\"1.0\" encoding=\"UTF-16\"?>"
        "<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">"
        "<RegistrationInfo><Author>%1</Author><Description>QuickSay 以管理员权限开机自启</Description></RegistrationInfo>"
        "<Triggers><LogonTrigger><Enabled>true</Enabled><UserId>%1</UserId><Delay>PT3S</Delay></LogonTrigger></Triggers>" // 延迟3秒再启动：登录那一瞬间explorer还没起来，托盘图标会加不上去
        "<Principals><Principal id=\"Author\"><UserId>%1</UserId><LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel></Principal></Principals>" // HighestAvailable就是「以最高权限运行」，登录时不弹UAC
        "<Settings>"
        "<MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>"
        "<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>" // 笔记本用电池时也要启动。这一项的默认值是true，必须显式改掉
        "<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>" // 切到电池供电时不许把QuickSay掐了。默认值同样是true
        "<AllowHardTerminate>false</AllowHardTerminate>"
        "<StartWhenAvailable>false</StartWhenAvailable>"
        "<AllowStartOnDemand>true</AllowStartOnDemand>"
        "<Enabled>true</Enabled>"
        "<Hidden>false</Hidden>"
        "<RunOnlyIfIdle>false</RunOnlyIfIdle>"
        "<WakeToRun>false</WakeToRun>"
        "<ExecutionTimeLimit>PT0S</ExecutionTimeLimit>" // PT0S表示不限制运行时长，不然开着够久就会被计划任务自动结束掉
        "<Priority>4</Priority>"
        "</Settings>"
        "<Actions Context=\"Author\"><Exec><Command>%2</Command><Arguments>--autostart</Arguments><WorkingDirectory>%3</WorkingDirectory></Exec></Actions>" // 带上--autostart，让开机启动的实例只待在托盘里、不弹主窗口
        "</Task>")
                      .arg(yonghu.toHtmlEscaped())
                      .arg(exeXml)
                      .arg(gongzuomulu);

    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) return false;
    ITaskService *svc = nullptr;
    ITaskFolder *folder = openRenwuGenmulu(&svc);
    bool ok = false;
    if (folder) {
        BSTR name = SysAllocString(g_ziqidongRenwuMing);
        BSTR xmlB = SysAllocString(reinterpret_cast<const wchar_t *>(xml.utf16()));
        VARIANT vUser;
        VariantInit(&vUser);
        vUser.vt = VT_BSTR;
        vUser.bstrVal = SysAllocString(reinterpret_cast<const wchar_t *>(yonghu.utf16()));
        VARIANT vSddl;
        VariantInit(&vSddl);
        vSddl.vt = VT_BSTR;
        vSddl.bstrVal = SysAllocString(reinterpret_cast<const wchar_t *>(ziqidongRenwuSddl().utf16()));
        VARIANT vEmpty;
        VariantInit(&vEmpty); // 密码留空：InteractiveToken方式不需要存密码
        IRegisteredTask *task = nullptr;
        HRESULT hr = folder->RegisterTask(name, xmlB, TASK_CREATE_OR_UPDATE, vUser, vEmpty, TASK_LOGON_INTERACTIVE_TOKEN, vSddl, &task);
        ok = SUCCEEDED(hr);
        if (task) task->Release();
        VariantClear(&vUser);
        VariantClear(&vSddl);
        SysFreeString(xmlB);
        SysFreeString(name);
        folder->Release();
    }
    if (svc) svc->Release();
    if (SUCCEEDED(init)) CoUninitialize();
    return ok;
}

bool createAdminRenwuWithUac() { // 弹一次UAC，用提权后的自身实例去建计划任务。任务建好之后每次开机都由计划任务以管理员权限启动QuickSay，不会再弹UAC
    if (isProcessElevated()) return createAdminRenwu(); // 当前已经是管理员权限了，直接建，不用再弹
    wchar_t exeW[MAX_PATH] = {0};
    ziqidongExePath().toWCharArray(exeW); // 缓冲区是清零过的，所以字符串末尾自带结束符
    SHELLEXECUTEINFOW info = {0};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS; // 要拿到进程句柄，好等它退出、读它的退出码
    info.lpVerb = L"runas"; // 就是这个动词触发UAC提权
    info.lpFile = exeW;
    info.lpParameters = L"--create-admin-task"; // 提权实例看到这个参数就只去建计划任务，建完立刻退出，不会真的启动第二个QuickSay
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info) || !info.hProcess) return false; // 用户在UAC弹窗上点了「否」也走这里
    WaitForSingleObject(info.hProcess, INFINITE); // 等提权实例把任务建完。它不开窗口，几十毫秒就退出了
    DWORD code = 1;
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    return code == 0;
}

bool applyZiqidong(bool bufanrao) { // 按config里的“开机自启动”和“以管理员权限启动”两个开关，把系统里的开机自启状态调成一致。成功返回true，失败返回false
    // bufanrao表示不打扰模式：绝不弹UAC。程序启动时调的那次用它——真要提权，手动启动的那份早在main()开头就提完了，轮不到这里再弹一次；开机自启起来的那份更不能弹
    QSettings reg(g_ziqidongRegPath, QSettings::NativeFormat); // 创建QSettings对象，用于访问注册表Run项
    if (!config["ziqidong"].toBool()) { // 不自启：先确认计划任务删掉，再清注册表项；删除失败就保留原状态，不能假装关闭成功
        if (!deleteAdminRenwu()) return false;
        reg.remove(g_ziqidongRegName);
        return true;
    }
    if (!config["guanliyuan"].toBool()) { // 普通权限自启：先删掉计划任务，再写注册表项；删除失败时绝不能让两条自启路径同时存在
        if (!deleteAdminRenwu()) return false;
        if (reg.value(g_ziqidongRegName).toString() != ziqidongRegValue()) reg.setValue(g_ziqidongRegName, ziqidongRegValue());
        return true;
    }
    // 剩下的是两个开关都开着：走计划任务，这样开机就直接以管理员权限起来，而且不弹UAC
    reg.remove(g_ziqidongRegName); // 互斥：计划任务开着就绝不能同时留着注册表项，否则开机会启动两个实例
    QString renwuExe;
    if (queryAdminRenwu(&renwuExe) && renwuExe.compare(ziqidongExePath(), Qt::CaseInsensitive) == 0) return true; // 任务在、启用着、记的路径也还是程序现在的位置，那什么都不用做
    // 到这里说明：任务不存在、或者被禁用了、或者程序挪过位置导致任务里记的路径过期了。这三种情况都得重建任务
    if ((!bufanrao || isProcessElevated()) && createAdminRenwuWithUac()) return true; // 已经是管理员权限就直接建、不弹UAC；不打扰模式下又没提权，那就先不建
    // 任务没建成（不打扰模式下没提权，或者用户在UAC弹窗上点了“否”）：先退回普通权限开机自启，别让用户以为开了自启结果开机什么都没有
    // “以管理员权限启动”那个开关不动它：下次手动启动照样会提权，那一份提权的实例跑到这里就能不弹UAC地把任务补上
    reg.setValue(g_ziqidongRegName, ziqidongRegValue());
    return false;
}

//====================以管理员权限启动====================
// 只有管理员权限的QuickSay才能往同样是管理员权限的窗口里输入（以管理员权限运行的记事本、任务管理器、注册表编辑器之类），这就是这个选项的意义
// 手动启动（双击exe）时靠这一节：开关开着而自己没有管理员权限，就用runas把自己重新启动一份提权的，然后这一份退出。代价是每次手动启动都要过一次UAC，这是用户勾这个选项时就认下的
// 开机自启不走这里：开机时由上面那个计划任务直接以管理员权限启动，一次UAC都不弹。万一任务没建成、退回了注册表Run项，那也宁可这次以普通权限跑，绝不在开机时弹UAC打扰用户
bool duGuanliyuanKaiguan() { // 从config.json里单独读“以管理员权限启动”这一个开关
    // 提权重启得赶在单实例检测之后、QApplication创建之前，那时候还用不了loadConfig：config.json不存在时它要拿屏幕尺寸算默认窗口位置，而那时还没有QGuiApplication
    QFile file(QFileInfo(ziqidongExePath()).absolutePath() + "/config.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false; // 配置文件还不存在，那就是第一次运行，默认不提权
    QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
    return o["guanliyuan"].toBool(o["ziqidong_guanliyuan"].toBool(false)); // 老版本的config.json里这个开关还叫ziqidong_guanliyuan，兼容一下（loadConfig会把它改成新名字）
}

bool tiquanChongqiZishen() { // 用runas把自己重新启动一份带管理员权限的。成功返回true，调用方随即退出，把活交给新起来的那一份
    wchar_t exeW[MAX_PATH] = {0};
    ziqidongExePath().toWCharArray(exeW); // 缓冲区是清零过的，所以字符串末尾自带结束符
    SHELLEXECUTEINFOW info = {0};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC; // 本进程紧接着就要退出了，得让ShellExecuteEx把该干的事全干完再返回
    info.lpVerb = L"runas"; // 就是这个动词触发UAC提权
    info.lpFile = exeW;
    info.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&info); // 用户在UAC弹窗上点了“否”的话返回false，那就以普通权限接着跑
}

//====================单实例检测====================
// 为什么不用SingleApplication自带的那套：它靠QSharedMemory和QLocalServer来判断「是不是已经有一个实例在跑了」，而这两样都是内核对象。
// 被计划任务以管理员权限拉起来的QuickSay建出来的是高完整性对象，后面手动双击exe启动的那个中完整性实例根本访问不了，检测就失效了，于是开出第二个QuickSay
// 这里自己拿命名互斥体做检测：安全描述符里把对象的完整性标签降到Low，中完整性进程也能访问管理员实例建的对象，提权/非提权谁先起谁后起都能互相发现
static const wchar_t *g_danshiliMutexMing = L"Local\\QuickSay_SingleInstance"; // 命名互斥体：谁第一个建出来，谁就是唯一的那个实例
static const wchar_t *g_danshiliEventMing = L"Local\\QuickSay_ShowWindow"; // 命名事件：后启动的实例用它通知先启动的那个实例把主窗口显示出来
HANDLE g_danshiliMutex = nullptr; // 上面那个命名互斥体的句柄。平时不用管它，只有提权重启自己之前要主动关掉，好让新起来的那份抢得到
HANDLE g_danshiliEvent = nullptr; // 上面那个命名事件的句柄。第一个实例要一直留着它给QWinEventNotifier监听。不用手动关，进程退出时系统自动回收

bool tongzhiYiyouShili() { // 抢单实例互斥体。已经有一个QuickSay在跑就通知它把主窗口显示出来，并返回true（调用方直接退出）
    // D:(A;;GA;;;WD)是「所有人完全放行」，S:(ML;;NW;;;LW)把对象的完整性标签降到Low。后半句才是关键：
    // Windows的强制完整性控制默认不许低完整性进程写高完整性对象，不把标签降下来的话，中完整性实例连管理员实例建的互斥体都打不开，等于没做检测
    PSECURITY_DESCRIPTOR sd = nullptr;
    ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GA;;;WD)S:(ML;;NW;;;LW)", SDDL_REVISION_1, &sd, nullptr);
    SECURITY_ATTRIBUTES sa = {sizeof(sa), sd, FALSE};
    g_danshiliMutex = CreateMutexW(sd ? &sa : nullptr, FALSE, g_danshiliMutexMing); // 互斥体只是个占位的名字，谁先建出来谁就是唯一的那个实例
    bool yiyou = (GetLastError() == ERROR_ALREADY_EXISTS); // 这个名字的互斥体已经被别人建过了，说明QuickSay已经在跑了
    g_danshiliEvent = CreateEventW(sd ? &sa : nullptr, TRUE, FALSE, g_danshiliEventMing); // 手动重置的事件。谁先建谁定这个属性，而先建的一定是第一个实例
    if (sd) LocalFree(sd);
    if (yiyou && g_danshiliEvent) SetEvent(g_danshiliEvent); // 通知第一个实例：有人又启动了一次QuickSay，把窗口显示出来
    return yiyou;
}

void shifangDanshili() { // 把单实例检测占着的两个内核对象放掉。只有提权重启自己之前用得上：不先让出互斥体，新起来的那份会被单实例检测当成“已经有一个在跑了”而直接退出
    if (g_danshiliMutex) {
        CloseHandle(g_danshiliMutex);
        g_danshiliMutex = nullptr;
    }
    if (g_danshiliEvent) {
        CloseHandle(g_danshiliEvent);
        g_danshiliEvent = nullptr;
    }
}

bool yongRenwuChongqiGuanliyuan() { // 用已经授权过的管理员自启计划任务立刻启动一份QuickSay。任务创建时已经过了一次UAC，所以从这里启动不会再弹第二次UAC
    // 只有“开机自启动”和“以管理员权限启动”同时开着时才有这份任务；没有任务或者任务启动失败时，调用方仍会退回runas正常弹一次UAC
    PSECURITY_DESCRIPTOR sd = nullptr;
    ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GA;;;WD)S:(ML;;NW;;;LW)", SDDL_REVISION_1, &sd, nullptr);
    SECURITY_ATTRIBUTES sa = {sizeof(sa), sd, FALSE};
    HANDLE xianshiQingqiu = CreateEventW(sd ? &sa : nullptr, TRUE, TRUE, g_danshiliEventMing); // 任务里的启动参数是--autostart，本来只会进托盘；提前把“显示窗口”事件设为有信号，新实例进入事件循环后就会把主窗口显示出来
    if (sd) LocalFree(sd);
    if (!xianshiQingqiu) return false;

    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) {
        CloseHandle(xianshiQingqiu);
        return false;
    }
    ITaskService *svc = nullptr;
    ITaskFolder *folder = openRenwuGenmulu(&svc);
    bool ok = false;
    if (folder) {
        IRegisteredTask *task = nullptr;
        BSTR name = SysAllocString(g_ziqidongRenwuMing);
        HRESULT hr = folder->GetTask(name, &task);
        SysFreeString(name);
        if (SUCCEEDED(hr) && task) {
            VARIANT empty;
            VariantInit(&empty); // 不另传参数，沿用任务定义里的--autostart；上面的显示窗口事件会把它从“只进托盘”改成“显示主窗口”
            IRunningTask *running = nullptr;
            ok = SUCCEEDED(task->Run(empty, &running)); // 由任务计划程序启动它，直接继承任务的HighestAvailable权限，不会再经过UAC
            if (running) running->Release();
            task->Release();
        }
        folder->Release();
    }
    if (svc) svc->Release();
    if (SUCCEEDED(init)) CoUninitialize();

    if (ok) { // 等新实例抢到单实例互斥体，再放掉上面的显示事件句柄；否则旧进程退得太快，事件对象可能在新实例打开它之前就消失
        for (int i = 0; i < 3000; ++i) { // 最多等30秒。正常只要几十毫秒；任务计划程序偶尔忙时也给它足够时间
            HANDLE newMutex = OpenMutexW(SYNCHRONIZE, FALSE, g_danshiliMutexMing);
            if (newMutex) {
                CloseHandle(newMutex);
                Sleep(50);
                break;
            } // 新实例先建互斥体、紧接着就会打开显示事件，多等50毫秒把这两步之间的小缝补上
            Sleep(10);
        }
    }
    CloseHandle(xianshiQingqiu);
    return ok;
}

QString phraseItemStyle(int itemHeight) { // 根据短语项高度和内边距生成列表项样式
    return QString("QListWidget::item{ height: %1px; padding: %2px %3px; }")
        .arg(itemHeight)
        .arg(config["phrase_item_padding_vertical"].toInt())
        .arg(config["phrase_item_padding_horizontal"].toInt());
}

void updatePhraseListColumns(QListWidget &liebiao, int columns) { // 根据每行短语数，把列表排成一列或者若干列。短语按从左到右、从上到下的顺序排
    // 实现方式：改成从左到右排、允许自动换行，然后让每个短语项的宽度正好是 可视区宽度/每行短语数 。这样列表排满columns个就再也塞不下第columns+1个，自然换到下一行
    // 宽度是通过BadgeDelegate::sizeHint里的g_phraseCellWidth报给列表的。不能用setGridSize()，因为网格只管每一格摆在哪，短语项自己有多大还是问委托要的，结果就是短语项宽度还是0、什么都画不出来
    int cellWidth = 0; // 0表示单列：一项独占一行、宽度撑满列表、文字自动折行，也就是老版本的样子
    if (columns > 1) {
        // 列表判断“这一行还塞不塞得下”用的宽度不是viewport()->width()，而是 maximumViewportSize()-垂直滚动条宽度 （见Qt源码QListViewPrivate::prepareItemsLayout）
        // 横向排列时这个宽度是固定的，不管滚动条当前显不显示都要扣掉滚动条的位置，所以滚动条冒出来/收回去都不会影响一行排几个
        int available = liebiao.maximumViewportSize().width() - liebiao.verticalScrollBar()->sizeHint().width() - 4; // 再多减4像素余量：Qt那边可能还会扣掉边框宽度。宁可少算几像素（右边留条不起眼的缝），也不能多算——多算就会挤掉一整列
        cellWidth = (available - 1) / columns; // 再减1，是因为列表判断塞不塞得下用的是>=，正好除尽时这一行的最后一个会被挤到下一行去
        if (cellWidth < 1) cellWidth = 1; // 窗口还没显示出来时宽度可能是0，兜底一下
    }
    if (g_phraseCellWidth == cellWidth && liebiao.isWrapping() == (columns > 1)) return; // 没变就别再设一遍，省掉一次重新布局
    g_phraseCellWidth = cellWidth;
    liebiao.setFlow((columns > 1) ? QListView::LeftToRight : QListView::TopToBottom);
    liebiao.setWrapping(columns > 1);
    liebiao.doItemsLayout(); // 强制立刻重新布局。必须调，因为开了setUniformItemSizes的列表会把短语项尺寸缓存起来，不重新布局就一直用缓存里的旧宽度
}

void applyPhraseListLayout(QListWidget &liebiao, int itemHeight, int columns) { // 应用某个分组的短语项高度和每行短语数。凡是会改变这两个值的地方都要调它
    liebiao.setStyleSheet(phraseItemStyle(itemHeight)); // 应用短语项高度和内边距
    updatePhraseListColumns(liebiao, columns);
}

QString clampTooltipText(const QString &text) { // 限制鼠标悬停提示的长宽：单行过长则按宽度上限换行（限制宽度），总行数过多则截断并追加省略号（限制高度）
    const int maxCharsPerLine = 50; // 宽度上限：每行最多约50个字符，超出部分自动换到下一行
    const int maxLines = 30; // 高度上限：最多显示15行，超出则截断

    QStringList out;
    bool truncated = false;
    const QStringList rawLines = text.split('\n');
    for (const QString &raw : rawLines) {
        if (raw.length() <= maxCharsPerLine) {
            out << raw;
        } else { // 超过宽度上限的行，按maxCharsPerLine拆成多行
            for (int i = 0; i < raw.length(); i += maxCharsPerLine) {
                out << raw.mid(i, maxCharsPerLine);
            }
        }
        if (out.size() > maxLines) { // 已超过高度上限，截断
            out = out.mid(0, maxLines);
            truncated = true;
            break;
        }
    }
    QString result = out.join('\n');
    if (truncated) result += "\n……"; // 被截断时在末尾追加省略号，提示内容未显示完
    return result;
}

void updateItemDisplay(QListWidgetItem *it) { // 更新对应短语项的显示，如果对应的备注不是空字符串，那么显示备注；否则显示短语
    QString text = it->data(Qt::UserRole).toString(); // 取出短语
    QString remark = it->data(Qt::UserRole + 1).toString(); // 取出备注
    it->setToolTip(clampTooltipText(remark.isEmpty() ? text : (remark + "\n────────────────────────\n" + text))); // 为短语项设置鼠标悬停时的提示文字（已限制长宽）。这样用户就可以通过鼠标悬停查看短语了
    if (!remark.isEmpty()) { // 如果备注不是空字符串
        it->setText(remark); // 设置显示文本为备注
        it->setForeground(QColor("#2E7D32")); // 设置备注字体颜色：蓝色【【【注：想修改备注字体颜色在这里修改】】】
    } else {
        it->setText(text); // 设置显示文本为短语
        it->setForeground(QColor("#323130")); // 设置短语字体颜色：深灰色
    }
}

void saveListToJson(QListWidget &liebiao, const QString &dataPath) { // 写入列表内容到data.json
    if (g_zhengzaiDaoruChongqi) return; // 导入成功后列表仍是旧内容，旧进程退出前禁止它再次覆盖刚导入的data.json
    QJsonArray jsonArray; // 创建一个JSON数组
    for (int i = 0; i < liebiao.count(); i++) { // 遍历列表中的所有项
        QJsonObject obj; // 创建一个JSON对象
        if (liebiao.item(i)->data(Qt::UserRole).toString().isEmpty()) { // 如果当前项的短语为空，那么说明当前项的Qt::UserRole还没设置
            liebiao.item(i)->setData(Qt::UserRole, liebiao.item(i)->text()); // 把当前项的文本存到当前项的Qt::UserRole
        }
        if (liebiao.item(i)->data(Qt::UserRole + 2).toString().isEmpty()) { // 如果当前项的分组为空，那么说明当前项的Qt::UserRole+2还没设置
            liebiao.item(i)->setData(Qt::UserRole + 2, "短语1"); // 把"短语1"存到当前项的Qt::UserRole+2
        }
        obj["text"] = liebiao.item(i)->data(Qt::UserRole).toString(); // 把当前项的短语存到"text"字段
        obj["remark"] = liebiao.item(i)->data(Qt::UserRole + 1).toString(); // 把当前项的备注存到"remark"字段
        obj["tab"] = liebiao.item(i)->data(Qt::UserRole + 2).toString(); // 把当前项的分组存到"tab"字段
        obj["hotkey"] = liebiao.item(i)->data(Qt::UserRole + 3).toString(); // 把当前项的快捷键字符串存到"hotkey"字段，如果没有设置则为空字符串
        jsonArray.append(obj); // 把对象加入数组
    }
    QJsonDocument doc(jsonArray); // 把数组包装成JSON文档
    QFile file(dataPath); // 打开指定路径的文件data.json
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) { // 如果文件成功打开（写模式）
        file.write(doc.toJson()); // 把JSON文档写入文件
        file.close(); // 关闭文件
    }
}

void loadListFromJson(QListWidget &liebiao, const QString &dataPath) { // 读取data.json到列表内容。如果data.json不存在，那么读取默认列表内容（也就是新手教程），同时写入默认列表内容到data.json
    QFile file(dataPath); // 打开指定路径的文件data.json
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) { // 如果文件成功打开（读模式）
        QByteArray data = file.readAll(); // 把文件内容读到内存
        file.close(); // 关闭文件
        QJsonDocument doc = QJsonDocument::fromJson(data); // 把数据解析成JSON文档
        if (doc.isArray()) { // 确认文档是一个数组
            liebiao.clear(); // 清空列表
            QJsonArray jsonArray = doc.array(); // 取出JSON数组
            for (auto value : jsonArray) { // 遍历数组中的每个元素
                if (value.isObject()) { // 确认元素是对象
                    QJsonObject obj = value.toObject(); // 转换为对象
                    QString text = obj["text"].toString(); // 取出"text"字段，即短语
                    QString remarkStr = obj["remark"].toString(); // 取出"remark"字段，即备注
                    QString tabStr = obj["tab"].toString(); // 取出"tab"字段，即分组
                    QString hotkeyStr = obj["hotkey"].toString(); // 取出"hotkey"字段，即短语快捷键
                    // 兼容旧版本【【【【【我能不能把saveListToJson里的兼容旧版本的代码写在loadListFromJson里？
                    QListWidgetItem *it = new QListWidgetItem(); // new一个短语项对象，并把它的地址给it。不直接liebiao.addItem(text)是因为这样就用不了Qt::UserRole了；使用new是因为这样它就不会在当前函数结束时被销毁
                    it->setData(Qt::UserRole, text); // 把短语存到it的Qt::UserRole
                    it->setData(Qt::UserRole + 1, remarkStr); // 把备注存到it的Qt::UserRole+1
                    it->setData(Qt::UserRole + 2, tabStr); // 把分组存到it的Qt::UserRole+2
                    it->setData(Qt::UserRole + 3, hotkeyStr); // 把快捷键字符串存到it的Qt::UserRole+3
                    updateItemDisplay(it); // 更新对应短语项的显示
                    liebiao.addItem(it); // 把it添加到列表。没错直接addItem指针是完全可以的 //it的内存不用释放，因为此时liebiao会接管it的所有权，自动管理其内存
                }
            }
        }
    } else { // 如果data.json不存在
        auto addDefaultItem = [&](const QString &text) { // 定义一个局部函数，用于添加默认短语
            QListWidgetItem *it = new QListWidgetItem();
            it->setData(Qt::UserRole, text);
            it->setData(Qt::UserRole + 1, "");
            it->setData(Qt::UserRole + 2, "短语1");
            it->setData(Qt::UserRole + 3, "");
            updateItemDisplay(it); // 更新对应短语项的显示
            liebiao.addItem(it);
        };
        addDefaultItem("快捷键：按下快捷键（默认Ctrl+Shift+V）呼出QuickSay\n添加短语：右上角加号\n修改/删除：右键短语\n排序：拖动短语"); // 【【【注：想修改默认列表内容（也就是新手教程）在这里修改】】】
        addDefaultItem("点击短语：输入对应短语\n上下方向键↑↓：移动光标\n回车键Enter：输入光标处短语");
        addDefaultItem("右键左上角分组：新建/修改/删除分组\n分组排序：拖动分组\n左右方向键←→：切换分组\n鼠标滚轮：也可以切换分组");
        addDefaultItem("感谢使用QuickSay！\n如果觉得好用记得点个Star！\n让我们开始吧！把这些短语都删掉，然后新建一个短语~");
        saveListToJson(liebiao, dataPath);
    }
}

// 每个分组自己的设置（短语项高度、每行短语数）都塞在QTabBar的tabData里（一个QVariantMap），下面三个函数是它唯一的出入口。以后再加分组设置就往这个map里加键，不要在别处直接读写tabData
int tabItemHeight(const QTabBar &tabBar, int index) { // 取出某个分组的短语项高度
    int itemHeight = tabBar.tabData(index).toMap().value("item_height").toInt();
    if (itemHeight <= 0) itemHeight = config["default_item_height"].toInt(); // 取不到就用默认短语项高度
    return itemHeight;
}

int tabColumns(const QTabBar &tabBar, int index) { // 取出某个分组的每行短语数
    int columns = tabBar.tabData(index).toMap().value("columns").toInt();
    if (columns < 1) columns = 1; // 旧数据没有这个字段，按每行1个短语处理
    if (columns > 10) columns = 10; // 上限10，和新建/修改分组窗口里输入框的范围保持一致
    return columns;
}

void setTabData(QTabBar &tabBar, int index, int itemHeight, int columns) { // 把短语项高度和每行短语数存进某个分组的tabData
    QVariantMap map;
    map["item_height"] = itemHeight;
    map["columns"] = columns;
    tabBar.setTabData(index, map);
}

void saveTabToJson(QTabBar &tabBar, const QString &tabPath) { // 写入分组栏内容到tab.json
    if (g_zhengzaiDaoruChongqi) return; // 导入成功后分组栏仍是旧内容，旧进程退出前禁止它再次覆盖刚导入的tab.json
    QJsonArray jsonArray; // 创建一个JSON数组
    for (int i = 0; i < tabBar.count(); i++) { // 遍历分组栏中的所有分组
        QJsonObject obj; // 创建一个JSON对象
        obj["tab"] = tabBar.tabText(i); // 把当前分组的文本存到"tab"字段
        obj["item_height"] = tabItemHeight(tabBar, i); // 把当前分组的短语项高度存到"item_height"字段
        obj["columns"] = tabColumns(tabBar, i); // 把当前分组的每行短语数存到"columns"字段
        jsonArray.append(obj); // 把对象加入数组
    }
    QJsonDocument doc(jsonArray); // 把数组包装成JSON文档
    QFile file(tabPath); // 打开指定路径的文件tab.json
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) { // 如果文件成功打开（写模式）
        file.write(doc.toJson()); // 把JSON文档写入文件
        file.close(); // 关闭文件
    }
}

void loadTabFromJson(QTabBar &tabBar, const QString &tabPath) { // 读取tab.json到分组栏内容。如果tab.json不存在，那么读取默认分组栏内容，同时写入默认分组栏内容到tab.json
    QFile file(tabPath); // 打开指定路径的文件tab.json
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) { // 如果文件成功打开（读模式）
        QByteArray data = file.readAll(); // 把文件内容读到内存
        file.close(); // 关闭文件
        QJsonDocument doc = QJsonDocument::fromJson(data); // 把数据解析成JSON文档
        if (doc.isArray()) { // 确认文档是一个数组
            while (tabBar.count() > 0) { // 清空分组栏。因为tabBar没有clear()所以我们只能这么做
                tabBar.removeTab(0);
            }
            QJsonArray jsonArray = doc.array(); // 取出JSON数组
            for (auto value : jsonArray) { // 遍历数组中的每个元素
                if (value.isObject()) { // 确认元素是对象
                    QJsonObject obj = value.toObject(); // 转换为对象
                    QString tab = obj["tab"].toString(); // 取出"tab"字段，即分组
                    int item_height = obj["item_height"].toInt(); // 取出"item_height"字段，即短语项高度
                    int columns = obj["columns"].toInt(); // 取出"columns"字段，即每行短语数
                    // 兼容旧版本
                    if (item_height == 0) item_height = config["default_item_height"].toInt(); // 如果"item_height"为0，那么使用默认短语项高度
                    if (columns < 1) columns = 1; // 旧版本的tab.json里没有"columns"字段（toInt会返回0），那么按每行1个短语处理
                    tabBar.addTab(tab); // 把分组新建到分组栏
                    setTabData(tabBar, tabBar.count() - 1, item_height, columns); // 把短语项高度和每行短语数存到对应分组的tabData中
                }
            }
        }
    } else { // 如果tab.json不存在
        tabBar.addTab("短语1"); // 【【【注：想修改默认分组栏内容在这里修改】】】
        setTabData(tabBar, 0, 70, 1);
        tabBar.addTab("短语2");
        setTabData(tabBar, 1, config["default_item_height"].toInt(), 1);
        tabBar.addTab("此处可右键点击");
        setTabData(tabBar, 2, config["default_item_height"].toInt(), 1);
        saveTabToJson(tabBar, tabPath);
    }
}

//====================备份与恢复====================
// 备份只收录config、分组和短语本身。<img>/<file>后面的路径只是短语正文中的普通字符，因此外部文件不会被读取、复制或塞进备份
struct QuickSayBackupData {
    QJsonObject settings; // 完整设置，对应config.json
    QJsonArray groups; // 完整分组，对应tab.json
    QJsonArray phrases; // 完整短语，对应data.json
    QString quickSayVersion; // 导出备份时的QuickSay版本，用于导入成功后的兼容性提醒
    QDateTime exportedAtUtc; // UTC导出时间，旧版或未知版本提示时再转成本地日期时间
};

QJsonArray quickSayGroupsForBackup(const QTabBar &tabBar) { // 把当前分组栏完整转换成备份里的groups数组
    QJsonArray groups;
    for (int i = 0; i < tabBar.count(); ++i) {
        QJsonObject group;
        group["tab"] = tabBar.tabText(i);
        group["item_height"] = tabItemHeight(tabBar, i);
        group["columns"] = tabColumns(tabBar, i);
        groups.append(group);
    }
    return groups;
}

QJsonArray quickSayPhrasesForBackup(const QListWidget &liebiao) { // 把当前短语列表完整转换成备份里的phrases数组，不解析也不打包正文中的外部文件路径
    QJsonArray phrases;
    for (int i = 0; i < liebiao.count(); ++i) {
        const QListWidgetItem *item = liebiao.item(i);
        QJsonObject phrase;
        phrase["text"] = item->data(Qt::UserRole).toString();
        phrase["remark"] = item->data(Qt::UserRole + 1).toString();
        phrase["tab"] = item->data(Qt::UserRole + 2).toString();
        phrase["hotkey"] = item->data(Qt::UserRole + 3).toString();
        phrases.append(phrase);
    }
    return phrases;
}

QByteArray createQuickSayBackupFile(const QListWidget &liebiao, const QTabBar &tabBar) { // 生成一个带可读元数据头、Base64正文和SHA-256完整性校验的单文件备份
    QString exportedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); // 元数据必须固定记录UTC时间，显示给用户时才转换成本地时间
    QJsonObject root;
    root["format_version"] = g_backupFormatVersion;
    root["quicksay_version"] = g_quickSayVersion;
    root["exported_at"] = exportedAt;
    root["settings"] = config;
    root["groups"] = quickSayGroupsForBackup(tabBar);
    root["phrases"] = quickSayPhrasesForBackup(liebiao);

    QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    QByteArray checksum = QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex();
    QByteArray base64 = payload.toBase64();
    QByteArray wrapped;
    for (int i = 0; i < base64.size(); i += 64) {
        wrapped += base64.mid(i, 64);
        wrapped += '\n';
    }
    if (!wrapped.isEmpty()) wrapped.chop(1); // 最后一行后面不额外留空行，使起止标记和参考格式一样紧凑

    QByteArray result;
    result += "------------------- QUICKSAY BACKUP BEGIN -------------------\n";
    result += "- EXPORTED AT:       " + exportedAt.toUtf8() + "\n";
    result += "- FORMAT VERSION:    " + g_backupFormatVersion.toUtf8() + "\n";
    result += "- QUICKSAY VERSION:  " + g_quickSayVersion.toUtf8() + "\n";
    result += "- SHA256:            " + checksum + "\n";
    result += "----------------------------------------------------------------\n";
    result += wrapped + "\n";
    result += "-------------------- QUICKSAY BACKUP END --------------------";
    return result;
}

bool writeQuickSayFileAtomically(const QString &path, const QByteArray &data, QString &error) { // 单个文件先写临时文件再原子替换，导出中途失败时不会留下半截备份
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = file.errorString();
        return false;
    }
    if (file.write(data) != data.size()) {
        error = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

bool parseQuickSayBackupFile(const QByteArray &fileData, QuickSayBackupData &backup, QString &error) { // 先完整校验文件头、校验和、JSON结构和三份数据的字段；此函数绝不碰现有数据
    if (fileData.isEmpty() || fileData.size() > 64 * 1024 * 1024) {
        error = "文件为空或超过64 MB";
        return false;
    }
    for (char ch : fileData) {
        if (static_cast<unsigned char>(ch) > 127) {
            error = "文件包含无效字符"; // 合法文件只有ASCII文件头和Base64正文，中文短语都封装在Base64里
            return false;
        }
    }

    QString text = QString::fromLatin1(fileData);
    text.replace("\r\n", "\n");
    if (text.contains('\r')) {
        error = "文件换行格式无效";
        return false;
    }
    QStringList lines = text.split('\n');
    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast(); // 允许文本编辑器在文件末尾补一个换行
    if (lines.size() < 8 || lines.first() != "------------------- QUICKSAY BACKUP BEGIN -------------------" || lines.last() != "-------------------- QUICKSAY BACKUP END --------------------") {
        error = "文件头或结束标记无效";
        return false;
    }
    if (lines.value(5) != "----------------------------------------------------------------") {
        error = "文件头分隔线无效";
        return false;
    }
    auto headerValue = [&](int index, const QString &prefix, QString &value) -> bool {
        QString line = lines.value(index);
        if (!line.startsWith(prefix)) return false;
        value = line.mid(prefix.size());
        return !value.isEmpty();
    };
    QString exportedAtText, formatVersion, quickSayVersion, checksumText;
    if (!headerValue(1, "- EXPORTED AT:       ", exportedAtText) ||
        !headerValue(2, "- FORMAT VERSION:    ", formatVersion) ||
        !headerValue(3, "- QUICKSAY VERSION:  ", quickSayVersion) ||
        !headerValue(4, "- SHA256:            ", checksumText)) {
        error = "文件头元数据无效";
        return false;
    }
    if (formatVersion != g_backupFormatVersion) {
        error = QString("不支持的备份格式版本：%1").arg(formatVersion);
        return false;
    }
    if (!QRegularExpression("^[A-Za-z0-9._+\\-]{1,40}$").match(quickSayVersion).hasMatch() ||
        !QRegularExpression("^[0-9a-f]{64}$").match(checksumText).hasMatch()) {
        error = "版本号或完整性校验值无效";
        return false;
    }
    QDateTime exportedAt = QDateTime::fromString(exportedAtText, Qt::ISODateWithMs);
    if (!exportedAt.isValid() || !exportedAtText.endsWith('Z')) {
        error = "UTC导出时间无效";
        return false;
    }

    QString base64Text;
    QRegularExpression base64Line("^[A-Za-z0-9+/]+={0,2}$");
    for (int i = 6; i < lines.size() - 1; ++i) {
        if (lines[i].isEmpty() || lines[i].size() > 64 || !base64Line.match(lines[i]).hasMatch()) {
            error = "备份正文格式无效";
            return false;
        }
        base64Text += lines[i];
    }
    if (base64Text.isEmpty() || base64Text.size() % 4 != 0) {
        error = "备份正文长度无效";
        return false;
    }
    QByteArray payload = QByteArray::fromBase64(base64Text.toLatin1());
    if (payload.toBase64() != base64Text.toLatin1()) {
        error = "备份正文Base64编码无效";
        return false;
    }
    QString actualChecksum = QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
    if (actualChecksum != checksumText) {
        error = "文件完整性校验失败，文件可能已损坏";
        return false;
    }

    QJsonParseError jsonError;
    QJsonDocument document = QJsonDocument::fromJson(payload, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !document.isObject()) {
        error = "备份正文不是有效的JSON对象";
        return false;
    }
    QJsonObject root = document.object();
    if (root.value("format_version").toString() != formatVersion || root.value("quicksay_version").toString() != quickSayVersion || root.value("exported_at").toString() != exportedAtText ||
        !root.value("settings").isObject() || !root.value("groups").isArray() || !root.value("phrases").isArray()) {
        error = "文件头与备份正文不一致，或未找到完整数据";
        return false;
    }

    QJsonObject settings = root.value("settings").toObject();
    auto integerInRange = [&](const char *key, int minimum, int maximum) -> bool {
        QJsonValue value = settings.value(key);
        if (!value.isDouble()) return false;
        double number = value.toDouble();
        return qIsFinite(number) && number == qFloor(number) && number >= minimum && number <= maximum;
    };
    auto booleanField = [&](const char *key) -> bool {
        return settings.value(key).isBool();
    };
    const char *boolKeys[] = {"zhiding", "jiaobiao", "badge_key_input_phrase_when_pinned", "enter_key_input_phrase_when_pinned", "ziqidong", "guanliyuan", "tudingflag"};
    for (const char *key : boolKeys) {
        if (!booleanField(key)) {
            error = QString("设置字段 %1 无效").arg(key);
            return false;
        }
    }
    if (!settings.value("hotkey").isString() || settings.value("hotkey").toString().isEmpty() || settings.value("hotkey").toString().size() > 128 ||
        !integerInRange("width", 250, 2000) || !integerInRange("height", 250, 2000) ||
        !integerInRange("default_item_height", 10, 100) || !integerInRange("phrase_item_padding_horizontal", 0, 100) ||
        !integerInRange("phrase_item_padding_vertical", 0, 100) || !integerInRange("gundong", 1, 100) || !integerInRange("delay", 0, 2000)) {
        error = "备份中的基础设置无效";
        return false;
    }
    const char *coordinateKeys[] = {"chuangkou_x", "chuangkou_y", "shezhichuangkou_x", "shezhichuangkou_y", "tianjiachuangkou_x", "tianjiachuangkou_y", "xiugaichuangkou_x", "xiugaichuangkou_y"};
    for (const char *key : coordinateKeys) {
        if (!integerInRange(key, INT_MIN, INT_MAX)) {
            error = QString("窗口坐标字段 %1 无效").arg(key);
            return false;
        }
    }

    QJsonArray groups = root.value("groups").toArray();
    if (groups.isEmpty() || groups.size() > 10000) {
        error = "备份中的分组数量无效";
        return false;
    }
    QSet<QString> groupNames;
    for (const QJsonValue &value : groups) {
        if (!value.isObject()) {
            error = "备份中存在无效分组";
            return false;
        }
        QJsonObject group = value.toObject();
        QString name = group.value("tab").toString();
        double itemHeight = group.value("item_height").toDouble(-1);
        double columns = group.value("columns").toDouble(-1);
        if (!group.value("tab").isString() || name.isEmpty() || groupNames.contains(name) ||
            !group.value("item_height").isDouble() || itemHeight != qFloor(itemHeight) || itemHeight < 10 || itemHeight > 100 ||
            !group.value("columns").isDouble() || columns != qFloor(columns) || columns < 1 || columns > 10) {
            error = "备份中的分组名称或分组设置无效";
            return false;
        }
        groupNames.insert(name);
    }

    QJsonArray phrases = root.value("phrases").toArray();
    if (phrases.size() > 100000) {
        error = "备份中的短语数量过多";
        return false;
    }
    for (const QJsonValue &value : phrases) {
        if (!value.isObject()) {
            error = "备份中存在无效短语";
            return false;
        }
        QJsonObject phrase = value.toObject();
        if (!phrase.value("text").isString() || !phrase.value("remark").isString() || !phrase.value("tab").isString() || !phrase.value("hotkey").isString() ||
            !groupNames.contains(phrase.value("tab").toString()) || phrase.value("hotkey").toString().size() > 128) {
            error = "备份中的短语字段或所属分组无效";
            return false;
        }
    }

    backup.settings = settings;
    backup.groups = groups;
    backup.phrases = phrases;
    backup.quickSayVersion = quickSayVersion;
    backup.exportedAtUtc = exportedAt.toUTC();
    return true;
}

void moveImportedWindowsOntoScreens(QJsonObject &settings) { // 保留仍位于任意屏幕上的原坐标；只有整个窗口已经落在所有屏幕之外时才移到主屏幕中央
    int width = settings.value("width").toInt();
    int height = settings.value("height").toInt();
    QScreen *primary = QGuiApplication::primaryScreen();
    if (!primary) return;
    const QStringList prefixes = {"chuangkou", "shezhichuangkou", "tianjiachuangkou", "xiugaichuangkou"};
    for (const QString &prefix : prefixes) {
        QString xKey = prefix + "_x", yKey = prefix + "_y";
        int windowWidth = prefix == "chuangkou" ? width : 500; // 只有主窗口使用设置里的宽高，其他窗口一直按默认500*500判断位置
        int windowHeight = prefix == "chuangkou" ? height : 500;
        QRect windowRect(settings.value(xKey).toInt(), settings.value(yKey).toInt(), windowWidth, windowHeight);
        bool onAnyScreen = false;
        for (QScreen *screen : QGuiApplication::screens()) {
            if (screen && screen->availableGeometry().intersects(windowRect)) {
                onAnyScreen = true;
                break;
            }
        }
        if (!onAnyScreen) {
            QRect primaryRect = primary->availableGeometry();
            settings[xKey] = primaryRect.x() + (primaryRect.width() - windowWidth) / 2;
            settings[yKey] = primaryRect.y() + (primaryRect.height() - windowHeight) / 2;
        }
    }
}

bool replaceQuickSayDataTransactionally(const QuickSayBackupData &backup, const QString &configPath, const QString &dataPath, const QString &tabPath, QString &error) { // 三份新数据全部准备好后再替换；任一步失败就把已经改动的文件全部原样回滚
    struct FileChange {
        QString path;
        QString stagedPath;
        QString oldPath;
        QByteArray newData;
        bool existed = false;
        bool oldRenamed = false;
        bool installed = false;
    };
    QString suffix = QString(".%1.%2").arg(GetCurrentProcessId()).arg(QDateTime::currentMSecsSinceEpoch());
    QVector<FileChange> files = {
        {configPath, configPath + ".import-new" + suffix, configPath + ".import-old" + suffix, QJsonDocument(backup.settings).toJson()},
        {dataPath, dataPath + ".import-new" + suffix, dataPath + ".import-old" + suffix, QJsonDocument(backup.phrases).toJson()},
        {tabPath, tabPath + ".import-new" + suffix, tabPath + ".import-old" + suffix, QJsonDocument(backup.groups).toJson()}};
    auto cleanupStaged = [&]() {
        for (FileChange &file : files)
            QFile::remove(file.stagedPath);
    };
    auto rollback = [&]() -> bool {
        bool ok = true;
        for (FileChange &file : files) {
            if (file.installed && QFile::exists(file.path) && !QFile::remove(file.path)) ok = false;
        }
        for (FileChange &file : files) {
            if (file.oldRenamed && !QFile::rename(file.oldPath, file.path)) ok = false;
        }
        cleanupStaged();
        return ok;
    };

    for (FileChange &file : files) {
        file.existed = QFile::exists(file.path);
        QString writeError;
        if (!writeQuickSayFileAtomically(file.stagedPath, file.newData, writeError)) {
            cleanupStaged();
            error = QString("无法准备新数据：%1").arg(writeError);
            return false;
        }
    }
    for (FileChange &file : files) {
        if (file.existed) {
            if (!QFile::rename(file.path, file.oldPath)) {
                bool restored = rollback();
                error = restored ? "无法暂存现有数据，现有数据未改变" : "无法暂存现有数据，而且回滚失败，请不要关闭QuickSay并立即检查数据文件";
                return false;
            }
            file.oldRenamed = true;
        }
    }
    for (FileChange &file : files) {
        if (!QFile::rename(file.stagedPath, file.path)) {
            bool restored = rollback();
            error = restored ? "无法安装新数据，现有数据已恢复" : "无法安装新数据，而且回滚失败，请不要关闭QuickSay并立即检查数据文件";
            return false;
        }
        file.installed = true;
    }
    for (FileChange &file : files) {
        if (file.oldRenamed) QFile::remove(file.oldPath); // 全部替换成功后才删除旧文件，至此导入事务完成
    }
    return true;
}

int compareQuickSayVersions(const QString &left, const QString &right, bool &known) { // 按最多四段数字比较版本；任何非标准版本号都归为“未知”
    QRegularExpression expression("^\\d+(?:\\.\\d+){1,3}$");
    if (!expression.match(left).hasMatch() || !expression.match(right).hasMatch()) {
        known = false;
        return 0;
    }
    known = true;
    QStringList a = left.split('.'), b = right.split('.');
    int count = qMax(a.size(), b.size());
    for (int i = 0; i < count; ++i) {
        int av = (i < a.size()) ? a[i].toInt() : 0;
        int bv = (i < b.size()) ? b[i].toInt() : 0;
        if (av > bv) return 1;
        if (av < bv) return -1;
    }
    return 0;
}

bool startQuickSayThroughExplorer(QString &error) { // 让桌面Explorer进程执行新实例，即使旧QuickSay是管理员权限，新实例也会先以普通权限启动并自行读取导入后的管理员设置
    QString exePath = ziqidongExePath(); // 这里只传exe路径，不传--autostart；新实例必须按一次正常手动启动完整处理提权和开机自启动
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool shouldUninitialize = SUCCEEDED(init);
    IShellDispatch2 *shell = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_Shell, nullptr, CLSCTX_LOCAL_SERVER, IID_IShellDispatch2, reinterpret_cast<void **>(&shell));
    if (SUCCEEDED(hr) && shell) {
        BSTR file = SysAllocString(reinterpret_cast<const OLECHAR *>(exePath.utf16()));
        VARIANT empty, show;
        VariantInit(&empty);
        VariantInit(&show);
        show.vt = VT_I4;
        show.lVal = SW_SHOWNORMAL;
        hr = shell->ShellExecute(file, empty, empty, empty, show); // Shell.Application实际位于普通权限的Windows Explorer中，由它启动就不会继承旧进程的管理员令牌
        SysFreeString(file);
        shell->Release();
    }
    if (shouldUninitialize) CoUninitialize();
    if (SUCCEEDED(hr)) return true;

    QString parameters = QString("\"%1\"").arg(QDir::toNativeSeparators(exePath));
    HINSTANCE result = ShellExecuteW(nullptr, L"open", L"explorer.exe", reinterpret_cast<const wchar_t *>(parameters.utf16()), nullptr, SW_SHOWNORMAL); // COM外壳极少数情况下不可用时，仍然明确把启动请求交给Explorer
    if (reinterpret_cast<INT_PTR>(result) > 32) return true;
    error = QString("Windows Explorer 启动失败（错误 %1）").arg(reinterpret_cast<INT_PTR>(result));
    return false;
}

void filterListByTab(QListWidget &liebiao, const QString &currentTab, const QString &searchKeyword) { // 根据当前选中分组和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
    int visibleCount = 0; // 用于计数当前遍历到的可见项
    for (int i = 0; i < liebiao.count(); i++) { // 遍历列表中的所有项
        QListWidgetItem *item = liebiao.item(i);
        bool isTextMatch = true; // 判断搜索框文字是否匹配
        if (!searchKeyword.isEmpty()) { // 如果搜索框里有字
            isTextMatch = (item->data(Qt::UserRole).toString()).contains(searchKeyword, Qt::CaseInsensitive) || (item->data(Qt::UserRole + 1).toString()).contains(searchKeyword, Qt::CaseInsensitive); // 如果短语包含了搜索框文字，或者备注包含了搜索框文字（Qt::CaseInsensitive表示不区分大小写），那么返回true
        }
        if (item->data(Qt::UserRole + 2).toString() == currentTab && isTextMatch) { // 如果该短语项的分组名称等于当前选中分组的名称，并且搜索框文字也匹配
            item->setHidden(false); // 显示该短语项
            visibleCount++;
            QString badgeStr = ""; // 用于临时存放角标字符
            if (visibleCount >= 1 && visibleCount <= 9) { // 如果是第1~9条短语
                badgeStr = QString::number(visibleCount); // 直接显示数字
            } else if (visibleCount == 10) { // 如果是第10条短语
                badgeStr = "0"; // 显示0
            } else if (visibleCount >= 11 && visibleCount <= 36) { // 如果是第11~36条短语
                badgeStr = QChar('A' + visibleCount - 11); // 显示A~Z
            }
            // 如果超过36条那么badgeStr直接为空字符串，即不显示角标
            item->setData(Qt::UserRole + 4, badgeStr); // 把badgeStr存到该短语项的Qt::UserRole+4
        } else {
            item->setHidden(true); // 隐藏该短语项
            item->setData(Qt::UserRole + 4, ""); // 清除该短语项的Qt::UserRole+4
        }
    }
}

bool isTabNameDuplicate(QTabBar &tabBar, const QString &tabName) { // 判断新建/修改分组时用户给出的分组名称是否重复
    for (int i = 0; i < tabBar.count(); i++) { // 遍历分组栏中的所有分组
        if (tabBar.tabText(i) == tabName) return true; // 如果名称重复，那么返回true
    }
    return false;
}

bool showTabDialog(QWidget &parent, const QString &title, QString &tabName, int &itemHeight, int &columns) { // 用于弹出一个同时包含“分组名称”“短语项高度”“每行短语数”三个输入框的对话框【【【【【待处理，比如改下该窗口的UI
    QDialog dialog(&parent); // 创建一个局部对话框对象，将传入的 parent 作为父窗口，对话框关闭时内存会自动回收
    dialog.setWindowTitle(title); // 设置对话框的标题文字
    dialog.setFixedSize(280, 167); // 设置固定的窗口大小（宽280，高167），防止窗口被拉伸变形

    QFormLayout layout(&dialog); // 创建一个表单布局，用于整齐地将标签文字和输入框左右排列

    QLineEdit nameEdit(&dialog); // 创建一个单行文本输入框，用于让用户输入分组名称
    nameEdit.setText(tabName); // 将初始的分组名称（如旧名称或空字符串）放入输入框中
    layout.addRow("分组名称：", &nameEdit); // 把提示文字和名称输入框作为一行添加到表单布局中

    QSpinBox heightSpin(&dialog); // 创建一个数字输入框，用于让用户输入短语项高度
    heightSpin.setRange(10, 100); // 设置数字输入框的可输入范围为10到100像素【【【【【
    heightSpin.setValue(itemHeight); // 将初始的高度值（如默认高度或已有高度）放入输入框中
    layout.addRow("短语项高度：", &heightSpin); // 把提示文字和高度输入框作为一行添加到表单布局中

    QSpinBox columnsSpin(&dialog); // 创建一个数字输入框，用于让用户输入每行短语数
    columnsSpin.setRange(1, 10); // 设置数字输入框的可输入范围为1到10个【【【【【
    columnsSpin.setValue(columns); // 将初始的每行短语数（新建分组是1，修改分组是该分组已有的值）放入输入框中
    layout.addRow("每行短语数：", &columnsSpin); // 把提示文字和每行短语数输入框作为一行添加到表单布局中

    QHBoxLayout btnLayout; // 创建一个水平布局，用于放置“确定”和“取消”按钮
    QPushButton btnCancel("取消", &dialog); // 创建一个文本为“取消”的按钮
    QPushButton btnOk("确定", &dialog); // 创建一个文本为“确定”的按钮
    btnOk.setDefault(true); // 把“确定”设为默认按钮，这样对话框里按回车时行为和点击“确定”一致
    btnLayout.addStretch(); // 在按钮左侧添加一个弹簧，把两个按钮挤到窗口的右下角，看起来更美观
    btnLayout.addWidget(&btnCancel); // 把“取消”按钮放入水平布局
    btnLayout.addWidget(&btnOk); // 把“确定”按钮放入水平布局
    layout.addRow(&btnLayout); // 把包含了两个按钮的水平布局添加到整个表单布局的最后一行

    // 当点击“确定”按钮时，触发对话框的 accept 槽函数（表示用户确认，准备关闭窗口）
    QObject::connect(&btnOk, &QPushButton::clicked, &dialog, &QDialog::accept);
    // 当点击“取消”按钮时，触发对话框的 reject 槽函数（表示用户取消，准备关闭窗口）
    QObject::connect(&btnCancel, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) { // 弹出对话框并阻塞程序执行。如果用户点击了“确定”（返回值是 Accepted）
        tabName = nameEdit.text(); // 获取名称输入框里的文本，存入传入的 tabName 引用变量中
        itemHeight = heightSpin.value(); // 获取高度输入框里的数字，存入传入的 itemHeight 引用变量中
        columns = columnsSpin.value(); // 获取每行短语数输入框里的数字，存入传入的 columns 引用变量中
        return true; // 返回 true，告诉调用者用户确认了输入
    }
    return false; // 如果用户点击了“取消”或按右上角关闭了窗口，返回 false
}

bool moniCtrlV() { // 模拟Ctrl+V。SendInput能可靠报告实际发送了几个事件，所以没有完整发送时返回false，让高级输入立即停止
    INPUT inputs[4] = {}; // 定义一个长度为4的数组，存放：左Ctrl键按下、V键按下、V键抬起、左Ctrl键抬起
    // 左Ctrl键按下
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_LCONTROL;
    // V键按下
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';
    // V键抬起
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    // 左Ctrl键抬起
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_LCONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    // 一次性发送这4个事件
    UINT sent = SendInput(4, inputs, sizeof(INPUT));
    if (sent == 4) return true;

    INPUT releases[2] = {}; // SendInput可能只发送了前半截；失败后补发V和Ctrl的抬起，避免留下按下状态
    releases[0].type = INPUT_KEYBOARD;
    releases[0].ki.wVk = 'V';
    releases[0].ki.dwFlags = KEYEVENTF_KEYUP;
    releases[1].type = INPUT_KEYBOARD;
    releases[1].ki.wVk = VK_LCONTROL;
    releases[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, releases, sizeof(INPUT)); // 这是失败后的尽力清理；无论清理结果如何，调用方都会停止，绝不继续猜测性重试输出
    return false;
}

struct QuickSayDropFiles {
    DWORD pFiles;
    POINT pt;
    BOOL fNC;
    BOOL fWide;
};

enum class QuickSayOutputActionType {
    Text,
    Press,
    Sleep,
    Image
};

struct QuickSayOutputAction {
    QuickSayOutputActionType type = QuickSayOutputActionType::Text;
    QString text;
    QVector<WORD> modifiers;
    WORD key = 0;
    int sleepMs = 0;
    QString imagePath;
};

bool setClipboardFileWin32(const QString &filePath) {
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) return false;

    QString nativePath = QDir::toNativeSeparators(info.absoluteFilePath());
    int pathBytes = (nativePath.size() + 2) * sizeof(wchar_t);
    SIZE_T totalSize = sizeof(QuickSayDropFiles) + pathBytes;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, totalSize);
    if (!hMem) return false;

    BYTE *ptr = (BYTE *)GlobalLock(hMem);
    if (!ptr) {
        GlobalFree(hMem);
        return false;
    }

    QuickSayDropFiles *dropFiles = (QuickSayDropFiles *)ptr;
    dropFiles->pFiles = sizeof(QuickSayDropFiles);
    dropFiles->fWide = TRUE;
    memcpy(ptr + sizeof(QuickSayDropFiles), nativePath.utf16(), pathBytes - sizeof(wchar_t));
    GlobalUnlock(hMem);

    if (!OpenClipboard(nullptr)) {
        GlobalFree(hMem);
        return false;
    }

    if (!EmptyClipboard()) {
        CloseClipboard();
        GlobalFree(hMem);
        return false;
    }
    bool ok = SetClipboardData(CF_HDROP, hMem) != nullptr;
    bool closed = CloseClipboard() != FALSE;
    if (!ok) GlobalFree(hMem); // SetClipboardData成功后内存所有权已经交给系统，即使CloseClipboard失败也不能再释放
    return ok && closed;
}

bool setClipboardImageFileWin32(const QuickSayOutputAction &action) {
    return !action.imagePath.isEmpty() && setClipboardFileWin32(action.imagePath);
}

QString cleanQuickSayPath(QString path) {
    const ushort hiddenChars[] = {
        0x200E, 0x200F,
        0x202A, 0x202B, 0x202C, 0x202D, 0x202E,
        0x2066, 0x2067, 0x2068, 0x2069};
    for (ushort code : hiddenChars) {
        path.remove(QChar(code));
    }
    return path.trimmed();
}

struct QuickSayModifierSnapshot {
    bool lCtrl = false;
    bool rCtrl = false;
    bool lAlt = false;
    bool rAlt = false;
    bool lShift = false;
    bool rShift = false;
    bool lMeta = false;
    bool rMeta = false;
};

bool isExtendedVirtualKey(WORD vk) {
    switch (vk) {
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_INSERT:
    case VK_DELETE:
    case VK_LWIN:
    case VK_RWIN:
        return true;
    default:
        return false;
    }
}

bool sendVirtualKey(WORD vk, bool keyUp = false) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    if (keyUp) input.ki.dwFlags |= KEYEVENTF_KEYUP;
    if (isExtendedVirtualKey(vk)) input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    return SendInput(1, &input, sizeof(INPUT)) == 1; // SendInput的返回值就是实际成功插入的事件数，可以可靠判断这个按键有没有发出去
}

bool releasePressedPhysicalModifiers() {
    QuickSayModifierSnapshot snapshot;
    snapshot.lCtrl = (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0;
    snapshot.rCtrl = (GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0;
    snapshot.lAlt = (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0;
    snapshot.rAlt = (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;
    snapshot.lShift = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0;
    snapshot.rShift = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
    snapshot.lMeta = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0;
    snapshot.rMeta = (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;

    if (snapshot.lCtrl && !sendVirtualKey(VK_LCONTROL, true)) return false;
    if (snapshot.rCtrl && !sendVirtualKey(VK_RCONTROL, true)) return false;
    if (snapshot.lAlt && !sendVirtualKey(VK_LMENU, true)) return false;
    if (snapshot.rAlt && !sendVirtualKey(VK_RMENU, true)) return false;
    if (snapshot.lShift && !sendVirtualKey(VK_LSHIFT, true)) return false;
    if (snapshot.rShift && !sendVirtualKey(VK_RSHIFT, true)) return false;
    if (snapshot.lMeta && !sendVirtualKey(VK_LWIN, true)) return false;
    if (snapshot.rMeta && !sendVirtualKey(VK_RWIN, true)) return false;
    return true;
}

void beginQuickSayPressBlock() {
    g_quickSayPressBlockCount++;
}

void endQuickSayPressBlock() {
    if (g_quickSayPressBlockCount > 0) g_quickSayPressBlockCount--;
}

void appendTextOutputAction(QVector<QuickSayOutputAction> &actions, const QString &text) {
    if (text.isEmpty()) return;
    QuickSayOutputAction action;
    action.type = QuickSayOutputActionType::Text;
    action.text = text;
    actions.append(action);
}

bool virtualKeyFromName(const QString &name, WORD &vk) {
    QString key = name.trimmed().toLower();
    if (key == "enter" || key == "return") {
        vk = VK_RETURN;
        return true;
    }
    if (key == "tab") {
        vk = VK_TAB;
        return true;
    }
    if (key == "space") {
        vk = VK_SPACE;
        return true;
    }
    if (key == "esc" || key == "escape") {
        vk = VK_ESCAPE;
        return true;
    }
    if (key == "backspace") {
        vk = VK_BACK;
        return true;
    }
    if (key == "left") {
        vk = VK_LEFT;
        return true;
    }
    if (key == "right") {
        vk = VK_RIGHT;
        return true;
    }
    if (key == "up") {
        vk = VK_UP;
        return true;
    }
    if (key == "down") {
        vk = VK_DOWN;
        return true;
    }
    if (key == "ins" || key == "insert") {
        vk = VK_INSERT;
        return true;
    }
    if (key.size() == 1) {
        QChar c = key.at(0).toUpper();
        ushort u = c.unicode();
        if (u >= 'A' && u <= 'Z') {
            vk = static_cast<WORD>(u);
            return true;
        }
        if (u >= '0' && u <= '9') {
            vk = static_cast<WORD>(u);
            return true;
        }
    }
    if (key.size() >= 2 && key.at(0) == 'f') {
        bool ok = false;
        int n = key.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 24) {
            vk = static_cast<WORD>(VK_F1 + n - 1);
            return true;
        }
    }
    return false;
}

bool parsePressCombo(const QString &combo, QuickSayOutputAction &action) {
    QStringList parts = combo.split('+', Qt::KeepEmptyParts);
    if (parts.isEmpty()) return false;

    bool hasCtrl = false;
    bool hasAlt = false;
    bool hasShift = false;
    bool hasMeta = false;
    bool hasPrimary = false;
    WORD primaryKey = 0;

    for (const QString &rawPart : parts) {
        QString part = rawPart.trimmed();
        if (part.isEmpty()) return false;
        QString lower = part.toLower();
        if (lower == "ctrl" || lower == "control") {
            if (hasCtrl) return false;
            hasCtrl = true;
            continue;
        }
        if (lower == "alt") {
            if (hasAlt) return false;
            hasAlt = true;
            continue;
        }
        if (lower == "shift") {
            if (hasShift) return false;
            hasShift = true;
            continue;
        }
        if (lower == "meta" || lower == "win" || lower == "windows") {
            if (hasMeta) return false;
            hasMeta = true;
            continue;
        }

        if (hasPrimary) return false;
        if (!virtualKeyFromName(part, primaryKey)) return false;
        hasPrimary = true;
    }

    if (!hasPrimary && !hasCtrl && !hasAlt && !hasShift && !hasMeta) return false;
    action.type = QuickSayOutputActionType::Press;
    if (hasCtrl) action.modifiers.append(VK_LCONTROL);
    if (hasAlt) action.modifiers.append(VK_LMENU);
    if (hasShift) action.modifiers.append(VK_LSHIFT);
    if (hasMeta) action.modifiers.append(VK_LWIN);
    action.key = primaryKey;
    return true;
}

bool parseSleepDurationMs(const QString &text, int &sleepMs) {
    QString duration = text.trimmed();
    QString unit;
    if (duration.endsWith("ms", Qt::CaseInsensitive)) {
        unit = "ms";
        duration.chop(2);
    } else if (duration.endsWith("s", Qt::CaseInsensitive)) {
        unit = "s";
        duration.chop(1);
    } else {
        unit = "s";
    }

    if (duration.isEmpty()) return false;
    bool ok = false;
    int value = 0;
    if (unit == "ms") {
        value = duration.toInt(&ok);
        if (!ok || value < 0) return false;
    } else {
        double seconds = duration.toDouble(&ok);
        if (!ok || seconds < 0 || seconds > INT_MAX / 1000.0) return false;
        value = static_cast<int>(seconds * 1000.0 + 0.5); // 四舍五入
    }
    sleepMs = value;
    return true;
}

bool parseQuickSayTag(const QString &tag, QuickSayOutputAction &action) {
    QString trimmed = tag.trimmed();
    if (trimmed.isEmpty()) return false;

    QString lower = trimmed.toLower();
    if (lower == "sleep") {
        action.type = QuickSayOutputActionType::Sleep;
        action.sleepMs = 1000;
        return true;
    }

    QStringList parts = trimmed.split(' ', Qt::SkipEmptyParts);
    if (parts.size() == 2 && parts.at(0).compare("sleep", Qt::CaseInsensitive) == 0) {
        int sleepMs = 0;
        if (!parseSleepDurationMs(parts.at(1), sleepMs)) return false;
        action.type = QuickSayOutputActionType::Sleep;
        action.sleepMs = sleepMs;
        return true;
    }

    if (parts.size() == 2 && parts.at(0).compare("press", Qt::CaseInsensitive) == 0) {
        return parsePressCombo(parts.at(1), action);
    }

    bool isImgTag = trimmed.size() > 3 &&
        trimmed.left(3).compare("img", Qt::CaseInsensitive) == 0 &&
        trimmed.at(3).isSpace();
    bool isFileTag = trimmed.size() > 4 &&
        trimmed.left(4).compare("file", Qt::CaseInsensitive) == 0 &&
        trimmed.at(4).isSpace();
    if (isImgTag || isFileTag) {
        QString path = cleanQuickSayPath(trimmed.mid(isImgTag ? 4 : 5));
        if ((path.startsWith('"') && path.endsWith('"')) ||
            (path.startsWith('\'') && path.endsWith('\''))) {
            path = cleanQuickSayPath(path.mid(1, path.size() - 2));
        }
        QFileInfo info(path);
        if (!info.isAbsolute() || !info.exists() || !info.isFile()) return false;
        action.type = QuickSayOutputActionType::Image;
        action.imagePath = info.absoluteFilePath();
        return true;
    }

    if (lower == "enter" || lower == "return" || lower == "tab" || lower == "space" ||
        lower == "esc" || lower == "escape" || lower == "backspace" ||
        lower == "left" || lower == "right" || lower == "up" || lower == "down" ||
        lower == "insert" || lower == "ins" ||
        lower == "ctrl" || lower == "control" || lower == "shift" || lower == "alt" ||
        lower == "win" || lower == "meta" || lower == "windows") {
        return parsePressCombo(trimmed, action);
    }

    return false;
}

bool parseQuickSayOutputActions(const QString &text, QVector<QuickSayOutputAction> &actions) {
    QString buffer;
    for (int i = 0; i < text.size();) {
        if (text.at(i) == '\\' && i + 1 < text.size() && text.at(i + 1) == '<') {
            buffer.append('<');
            i += 2;
            continue;
        }

        if (text.at(i) == '<') {
            int closeIndex = text.indexOf('>', i + 1);
            if (closeIndex < 0) {
                buffer.append('<');
                i++;
                continue;
            }
            QuickSayOutputAction action;
            if (!parseQuickSayTag(text.mid(i + 1, closeIndex - i - 1), action)) {
                buffer.append(text.mid(i, closeIndex - i + 1));
                i = closeIndex + 1;
                continue;
            }
            appendTextOutputAction(actions, buffer);
            buffer.clear();
            actions.append(action);
            i = closeIndex + 1;
            continue;
        }

        buffer.append(text.at(i));
        i++;
    }
    appendTextOutputAction(actions, buffer);
    return true;
}

bool sendPressAction(const QuickSayOutputAction &action) {
    QVector<INPUT> inputs; // 一个<press>动作的按下和抬起放进同一次SendInput，既减少中途留下按下状态的窗口，也能用返回数量可靠判断是否完整发送
    auto appendKey = [&](WORD vk, bool keyUp) {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        if (keyUp) input.ki.dwFlags |= KEYEVENTF_KEYUP;
        if (isExtendedVirtualKey(vk)) input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        inputs.append(input);
    };
    for (WORD modifier : action.modifiers)
        appendKey(modifier, false);
    if (action.key != 0) {
        appendKey(action.key, false);
        appendKey(action.key, true);
    }
    for (int i = action.modifiers.size() - 1; i >= 0; i--)
        appendKey(action.modifiers.at(i), true);

    UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    if (sent == static_cast<UINT>(inputs.size())) return true;

    QVector<INPUT> releases; // 只发送了一部分时，补发本动作涉及的所有抬起事件，避免主键或修饰键永久卡在按下状态
    auto appendRelease = [&](WORD vk) {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        if (isExtendedVirtualKey(vk)) input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        releases.append(input);
    };
    if (action.key != 0) appendRelease(action.key);
    for (int i = action.modifiers.size() - 1; i >= 0; i--)
        appendRelease(action.modifiers.at(i));
    if (!releases.isEmpty()) SendInput(static_cast<UINT>(releases.size()), releases.data(), sizeof(INPUT)); // 失败后的尽力清理不算作继续输出，清理完立即停止整段高级输入
    return false;
}

class QuickSayOutputRunner : public QObject {
  private:
    enum class WaitingStep { // 整段输出永远只有一个成员QTimer在等待；停止它就能一次性让所有尚未执行的步骤失效
        None,
        NextAction,
        PasteText,
        WriteImageClipboard,
        PasteImage,
        ReleasePressBlock,
        FinishSleep
    };
    QVector<QuickSayOutputAction> actions_;
    int index_ = 0;
    QTimer timer_;
    WaitingStep waitingStep_ = WaitingStep::None;
    QuickSayOutputAction waitingImageAction_;
    bool pressBlockHeld_ = false;
    bool ended_ = false;

  public:
    QuickSayOutputRunner(const QVector<QuickSayOutputAction> &actions, QObject *parent = nullptr) : QObject(parent), actions_(actions), timer_(this) {
        timer_.setSingleShot(true);
        QObject::connect(&timer_, &QTimer::timeout, this, [this]() {
            runWaitingStep();
        });
    }

    ~QuickSayOutputRunner() override {
        timer_.stop(); // 正常情况下stop()早已清理过；这里再兜底，保证随QApplication销毁时也不会留下计时器和按键拦截计数
        if (pressBlockHeld_) {
            endQuickSayPressBlock();
            pressBlockHeld_ = false;
        }
        if (g_quickSayOutputRunner == this) {
            g_quickSayOutputRunner = nullptr;
            g_quickSayIsOutputting = false;
        }
    }

    void start() {
        if (!releasePressedPhysicalModifiers()) { // 开始输出时，一次性抬起用户当前按着的修饰键（典型情况就是触发短语用的快捷键，比如Ctrl+1里的Ctrl），整段输出期间保持抬起，避免它干扰moniCtrlV()的粘贴或模拟按键。此处只“抬起”、绝不再“按回去”：因为输出过程中用户随时可能松开快捷键，若结束时再注入一次按下，物理键已松开就会把修饰键永久卡在按下状态。彻底不注入down，就根除了卡键。
            stop(); // SendInput没有完整发送时有可靠的失败结果，立即停止，不再执行第一个动作
            return;
        }
        runCurrentAction();
    }

    void stop() { // 系统事件或可靠的输出失败都走这里：停表、作废等待步骤、恢复全局状态和按键拦截计数，再安全销毁自己
        if (ended_) return;
        ended_ = true;
        timer_.stop();
        waitingStep_ = WaitingStep::None;
        if (pressBlockHeld_) {
            endQuickSayPressBlock();
            pressBlockHeld_ = false;
        }
        if (g_quickSayOutputRunner == this) g_quickSayOutputRunner = nullptr;
        g_quickSayIsOutputting = false;
        deleteLater();
    }

  private:
    void waitFor(int ms, WaitingStep step) {
        if (ended_) return;
        waitingStep_ = step;
        timer_.start(qMax(0, ms));
    }

    void finishOutput() {
        stop(); // 正常结束和中途停止需要恢复的是同一组状态；统一收口可避免漏掉计时器或按键拦截计数
    }

    void finishAction() {
        if (index_ >= actions_.size()) {
            finishOutput();
            return;
        }
        waitFor(config["delay"].toInt(), WaitingStep::NextAction); // 动作之间等多久
    }

    void runWaitingStep() {
        if (ended_) return;
        WaitingStep step = waitingStep_;
        waitingStep_ = WaitingStep::None;
        switch (step) {
        case WaitingStep::NextAction:
            runCurrentAction();
            break;
        case WaitingStep::PasteText:
            if (!moniCtrlV()) stop();
            else finishAction();
            break;
        case WaitingStep::WriteImageClipboard:
            if (!setClipboardImageFileWin32(waitingImageAction_)) stop(); // OpenClipboard、EmptyClipboard、SetClipboardData或CloseClipboard明确失败时立即停止
            else waitFor(50, WaitingStep::PasteImage); // 写剪贴板和粘贴之间留50ms，让剪贴板传播到位
            break;
        case WaitingStep::PasteImage:
            if (!moniCtrlV()) stop();
            else finishAction();
            break;
        case WaitingStep::ReleasePressBlock:
            if (pressBlockHeld_) {
                endQuickSayPressBlock();
                pressBlockHeld_ = false;
            }
            finishAction();
            break;
        case WaitingStep::FinishSleep:
            finishAction();
            break;
        case WaitingStep::None:
            break;
        }
    }

    void runCurrentAction() {
        if (ended_) return;
        if (index_ >= actions_.size()) {
            finishOutput();
            return;
        }

        QuickSayOutputAction action = actions_.at(index_);
        index_++;
        switch (action.type) {
        case QuickSayOutputActionType::Text: {
            QApplication::clipboard()->setText(action.text); // Qt写文本剪贴板没有可靠的成功返回值，因此这里只保留原行为，不用猜测性读取来判断成功与否
            // 【【【【【这里的延迟考虑写进设置里，让用户自定义？
            waitFor(50, WaitingStep::PasteText); // 写剪贴板后，到“目标程序能读到新内容”之间有一段传播延迟。若setText后立刻moniCtrlV()，Ctrl+V可能在这段窗口内到达，目标程序读到的还是上一条/空内容，导致这一行粘贴失败（剪贴板其实已写对，只是粘早了）。这里等50ms让剪贴板传播到位再粘，能明显降低输出失败概率。
            break;
        }
        case QuickSayOutputActionType::Image: {
            waitingImageAction_ = action; // 图片动作要跨过前置的1秒等待，存成成员而不是让计时器回调引用已经失效的局部变量
            waitFor(1000, WaitingStep::WriteImageClipboard); // 在微信输出图片时，有时会出现一个bug，经测试，在<img>标签前面加一个<sleep>能有效解决这个bug。于是我就实现了个：当用户输入<img>标签后，先延迟1秒，再来执行<img>标签的操作
            break;
        }
        case QuickSayOutputActionType::Press: {
            beginQuickSayPressBlock();
            pressBlockHeld_ = true;
            if (!sendPressAction(action)) {
                stop(); // SendInput没有完整发送时立即停止；stop()也会归还上面刚取得的按键拦截计数
                return;
            }
            waitFor(config["delay"].toInt(), WaitingStep::ReleasePressBlock); // 程序在模拟按键后的动作延迟时间内，临时阻止 QuickSay 自己的键盘浏览逻辑处理这些按键，避免你模拟出来的按键又被 QuickSay 当成用户输入捕获。 //虽然这和动作之间的delay延迟语义不完全一样，但用同一设置更方便管理
            break;
        }
        case QuickSayOutputActionType::Sleep: {
            waitFor(action.sleepMs, WaitingStep::FinishSleep);
            break;
        }
        }
    }
};

void stopQuickSayOutput() { // 锁屏、会话断开、注销、睡眠和休眠发生时调用；没有正在输出就什么也不做
    if (g_quickSayOutputRunner) g_quickSayOutputRunner->stop();
}

void startQuickSayOutput(const QString &text) {
    if (g_quickSayIsOutputting) return;

    QVector<QuickSayOutputAction> actions;
    if (!parseQuickSayOutputActions(text, actions)) {
        QuickSayOutputAction action;
        action.type = QuickSayOutputActionType::Text;
        action.text = text;
        actions.append(action);
    }
    g_quickSayIsOutputting = true;
    QuickSayOutputRunner *runner = new QuickSayOutputRunner(actions, qApp);
    g_quickSayOutputRunner = runner;
    runner->start();
}

void shuchu(const QListWidgetItem *item, QWidget *chuangkou) {
    QString text = item->data(Qt::UserRole).toString(); // 获取对应选项里的短语
    if (config["tudingflag"].toBool() == false) chuangkou->close(); // 如果没有钉住窗口，那么关闭窗口到托盘
    startQuickSayOutput(text);
}

void rebuildItemHotkeys(QListWidget &liebiao, QVector<QHotkey *> &itemHotkeys, QApplication *a) { // 先禁用当前已注册的QHotkey *对象，然后遍历列表中的所有项，为它们注册快捷键
    for (auto hk : itemHotkeys) { // 遍历动态数组中所有短语项对应的QHotkey *对象
        if (hk) { // 如果该QHotkey *对象不是空指针
            hk->setRegistered(false); // 禁用当前已注册的这个QHotkey *对象
            delete hk; // 释放这个QHotkey *对象的内存
        }
    }
    itemHotkeys.clear(); // 清空这个动态数组

    for (int i = 0; i < liebiao.count(); i++) { // 遍历列表中的所有项
        QListWidgetItem *it = liebiao.item(i); // 取出该短语项的指针，赋值给it
        QString hkStr = it->data(Qt::UserRole + 3).toString(); // 返回保存在该短语项的Qt::UserRole+3里的快捷键字符串，用hkStr接收
        if (!hkStr.isEmpty()) { // 如果hkStr不为空字符串
            QHotkey *hk = new QHotkey(QKeySequence(hkStr), true, a); // 定义一个QHotkey *对象，设置快捷键为hkStr，全局可用。此时就成功注册快捷键了，也就是说按下快捷键会发出信号
            // 设置按下快捷键后会怎样，即输出该短语项里的短语
            QObject::connect(hk, &QHotkey::activated,
                [it]() {
                    QString text = it->data(Qt::UserRole).toString(); // 获取该短语项里的短语
                    startQuickSayOutput(text);
                });
            itemHotkeys.append(hk); // 把QHotkey *对象hk添加到动态数组中
        } else { // 如果hkStr为空字符串
            itemHotkeys.append(nullptr); // 那就把nullptr添加到动态数组中
        }
    }
}

void applyMainWindowNoActivate() {
    if (!pchuangkou) return;
    HWND hwnd = (HWND)pchuangkou->winId();
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_NOACTIVATE);
}

void setMainWindowNoActivateEnabled(bool enabled) {
    if (!pchuangkou) return;
    HWND hwnd = (HWND)pchuangkou->winId();
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (enabled) {
        exStyle |= WS_EX_NOACTIVATE;
    } else {
        exStyle &= ~WS_EX_NOACTIVATE;
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void stopQuickSayKeyboardHook() {
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }
}

void startQuickSayKeyboardHook();

void enterSearchMode() {
    if (!pchuangkou || !g_search) return;

    g_searchMode = true;
    g_lastForegroundBeforeSearch = GetForegroundWindow();
    stopQuickSayKeyboardHook();
    setMainWindowNoActivateEnabled(false);

    if (!pchuangkou->isVisible()) pchuangkou->show();
    pchuangkou->raise();
    pchuangkou->activateWindow();
    g_search->setFocus(Qt::ShortcutFocusReason);
    g_search->selectAll();
}

void leaveSearchMode(bool restorePreviousFocus = true) {
    if (!pchuangkou || !g_search) return;

    g_searchMode = false;
    g_search->clearFocus();
    setMainWindowNoActivateEnabled(true);
    applyMainWindowNoActivate();
    startQuickSayKeyboardHook();

    if (restorePreviousFocus && g_lastForegroundBeforeSearch && IsWindow(g_lastForegroundBeforeSearch)) {
        SetForegroundWindow(g_lastForegroundBeforeSearch);
    }
    g_lastForegroundBeforeSearch = nullptr;
}

QListWidgetItem *visibleItemByBadgeIndex(int targetIndex) {
    if (!g_liebiao) return nullptr;
    int visibleCount = 0;
    for (int i = 0; i < g_liebiao->count(); i++) {
        QListWidgetItem *item = g_liebiao->item(i);
        if (!item->isHidden()) {
            visibleCount++;
            if (visibleCount == targetIndex) return item;
        }
    }
    return nullptr;
}

// 下面这几个函数是键盘浏览短语的地基：因为短语可以排成多列，所以“上下左右”不能再按列表行号算，得先把当前分组里看得见的短语抽出来，再按 序号→(行,列) 换算
QVector<QListWidgetItem *> visiblePhraseItems() { // 按列表顺序取出所有没隐藏的短语项，也就是当前分组里看得见的那些短语。它们在界面上就是从左到右、从上到下排的，所以下标就是它们的排列序号
    QVector<QListWidgetItem *> items;
    if (!g_liebiao) return items;
    for (int i = 0; i < g_liebiao->count(); i++) {
        if (!g_liebiao->item(i)->isHidden()) items << g_liebiao->item(i);
    }
    return items;
}

int currentVisibleIndex(const QVector<QListWidgetItem *> &items) { // 当前选中的短语项在可见短语里排第几个。没选中，或者选中的是别的分组的（已隐藏），都返回-1
    if (!g_liebiao) return -1;
    return items.indexOf(g_liebiao->currentItem());
}

int currentTabColumns() { // 当前分组的每行短语数
    if (!g_tabBar || g_tabBar->currentIndex() < 0) return 1;
    return tabColumns(*g_tabBar, g_tabBar->currentIndex());
}

void selectVisibleItemAt(int index) { // 选中第index个可见短语项，并且滚动到它
    QVector<QListWidgetItem *> items = visiblePhraseItems();
    if (index < 0 || index >= items.size()) return;
    g_liebiao->setCurrentItem(items[index]);
    g_liebiao->scrollToItem(items[index], QAbstractItemView::PositionAtCenter);
}

void selectVisibleItemWithScrollSafeArea(int index, int direction) { // 上下方向键专用：选中短语后只在越过上下安全区时滚动，不再每次都强制居中。direction为-1是↑、1是↓
    QVector<QListWidgetItem *> items = visiblePhraseItems();
    if (!g_liebiao || index < 0 || index >= items.size()) return;

    QListWidgetItem *item = items[index];
    QScrollBar *scrollBar = g_liebiao->verticalScrollBar();
    int oldScrollValue = scrollBar->value(); // setCurrentItem在某些Qt样式下可能会顺手滚动；先记住像素位置，确保后面完全按安全区规则决定滚不滚
    g_liebiao->setCurrentItem(item);
    if (scrollBar->value() != oldScrollValue) scrollBar->setValue(oldScrollValue);

    QRect itemRect = g_liebiao->visualItemRect(item); // 直接使用选中项在viewport里的实际像素矩形，多列时同一行各项的top/bottom相同，因此安全区天然按“行”计算
    int itemHeight = itemRect.height();
    int viewportHeight = g_liebiao->viewport()->height();
    if (!itemRect.isValid() || itemHeight <= 0 || viewportHeight <= 0) { // 窗口布局尚未完成时拿不到有效矩形，至少保证选中项能显示出来
        g_liebiao->scrollToItem(item, QAbstractItemView::EnsureVisible);
        return;
    }

    int visibleRows = viewportHeight / itemHeight; // 只按完整可视行计算，窗口过矮时不会把上下安全区硬挤到一起
    int safeRows = qMin(1, qMax(0, (visibleRows - 1) / 2)); // 正常上下各留1行；可视行数不足时同时缩小，至少给选中项自身留出一行【【【方向键安全区在这里调整】】】
    int safeTop = safeRows * itemHeight; // 选中项顶部允许到达的最上边界（viewport像素坐标）
    int safeBottom = viewportHeight - safeRows * itemHeight; // 选中项底部允许到达的最下边界；这里用右开坐标，便于和QRect的bottom()+1比较
    int newScrollValue = oldScrollValue;

    bool crossedTop = itemRect.top() < safeTop;
    bool crossedBottom = itemRect.bottom() + 1 > safeBottom;
    if (crossedTop && crossedBottom) { // 单项比可视区还高时会同时越过两边，只按本次移动方向对齐一边，避免在列表顶/底反复按键时滚动条来回跳
        if (direction < 0) newScrollValue += itemRect.top() - safeTop;
        else newScrollValue += itemRect.bottom() + 1 - safeBottom;
    } else if (crossedTop) { // 向上越过安全区：把选中项推回上边界；滚动条到顶后Qt会自动钳住，选中项就能继续往列表顶端移动
        newScrollValue += itemRect.top() - safeTop;
    } else if (crossedBottom) { // 向下越过安全区：把选中项推回下边界；滚动条到底后同理允许选中项继续往列表底端移动
        newScrollValue += itemRect.bottom() + 1 - safeBottom;
    }

    newScrollValue = qBound(scrollBar->minimum(), newScrollValue, scrollBar->maximum()); // ScrollPerPixel下滚动条值就是像素位置，直接加矩形越界的像素数才能避免跳动
    if (newScrollValue != oldScrollValue) scrollBar->setValue(newScrollValue);
}

void moveCurrentVisibleItem(int direction) { // 上下方向键：在同一列里上下移动。direction为-1是↑、1是↓
    QVector<QListWidgetItem *> items = visiblePhraseItems();
    if (items.isEmpty()) return; // 空分组，不选中短语
    int k = currentVisibleIndex(items);
    if (k < 0) { // 当前没选中任何可见短语（比如刚从空分组切过来），那么按↓选第一项、按↑选最后一项
        selectVisibleItemWithScrollSafeArea((direction > 0) ? 0 : items.size() - 1, direction);
        return;
    }
    int target = k + direction * currentTabColumns(); // 同一列上/下一行的那个短语，序号正好差一个“每行短语数”
    if (target >= 0 && target < items.size()) { // 上/下一行的同列位置确实有短语，那就选它
        selectVisibleItemWithScrollSafeArea(target, direction);
        return;
    }
    // 上/下一行的同列位置没有短语（已经在第一行了，或者最后一行的这一列排不满）：按↑就跳到本分组第一项，按↓就跳到本分组最后一项
    // 顺带覆盖了“第一项按↑、最后一项按↓保持不动”——因为此时跳过去的就是它自己
    selectVisibleItemWithScrollSafeArea((direction > 0) ? items.size() - 1 : 0, direction);
}

bool g_zuoyouZhiqiehuanFenzu = false; // 左右方向键已经切过一次分组的标记：置位后左右键一律切分组，不再在行内移动短语；按上下键清除

void switchTabAndSelect(int direction) { // 在行首按←/在行尾按→时切换分组。direction为-1是切到上一个分组、1是切到下一个分组
    if (!g_tabBar) return;
    int index = g_tabBar->currentIndex() + direction;
    if (index < 0 || index >= g_tabBar->count()) return; // 已经是第一个/最后一个分组了，不再继续切换
    g_zuoyouZhiqiehuanFenzu = true; // 确实切成了分组，打上标记；之后接着按左右键就是一直翻分组，不会在某个分组里被一行好几条短语拦住
    static QTimer *qingchuJishi = nullptr; // 停手0.5秒后自动清除上面那个标记，免得隔了半天再按左右键还在翻分组
    if (!qingchuJishi) {
        qingchuJishi = new QTimer(qApp);
        qingchuJishi->setSingleShot(true);
        QObject::connect(qingchuJishi, &QTimer::timeout, [] { g_zuoyouZhiqiehuanFenzu = false; });
    }
    qingchuJishi->start(500); // 【【【注：改这个数字就能改“停手多久后标记失效”，单位毫秒】】】每切一次分组都重新计时
    g_tabBar->setCurrentIndex(index); // 切换分组。分组切换的槽函数里会顺手把新分组的第一项选中，这正是往右切要的结果
    if (direction < 0) { // 往左切过来的，改成选中新分组第一行最后一个实际存在的短语，这样光标看起来是从右边接着走的
        QVector<QListWidgetItem *> items = visiblePhraseItems();
        if (items.isEmpty()) return; // 空分组，不选中短语
        selectVisibleItemAt(qMin(currentTabColumns(), items.size()) - 1);
    }
}

void moveCurrentVisibleItemHorizontal(int direction) { // 左右方向键：在同一行里左右移动，不跨行；已经在行首/行尾了就切换分组。direction为-1是←、1是→
    if (g_zuoyouZhiqiehuanFenzu) { // 刚才已经用左右键切过分组了，那就一直切分组，不管这个分组一行有几条短语
        switchTabAndSelect(direction);
        return;
    }
    QVector<QListWidgetItem *> items = visiblePhraseItems();
    int k = currentVisibleIndex(items);
    if (k < 0) { // 空分组，或者当前没选中可见短语，那么直接切换分组。这样在空分组里连按左右键也能一直切下去
        switchTabAndSelect(direction);
        return;
    }
    int columns = currentTabColumns();
    int col = k % columns; // 当前短语在本行里排第几列（从0开始）
    if (direction < 0 && col > 0) { // 不在行首，选左边那个
        selectVisibleItemAt(k - 1);
        return;
    }
    if (direction > 0 && col < columns - 1) { // 不在行尾，右边那一格可能有短语
        if (k + 1 < items.size()) { // 右边那一格确实有短语，选右边那个
            selectVisibleItemAt(k + 1);
            return;
        }
        if (k + 1 - columns >= 0) { // 右边那一格是空的（最后一行没排满，光标就在最后一个短语上）：跳到上一行同列的那个短语，也就是视觉上正右方向最近的短语。比如3列时“1 2 3 / 4 5”，在5上按→跳到3而不是切分组
            selectVisibleItemAt(k + 1 - columns);
            return;
        }
    }
    switchTabAndSelect(direction); // 在行首按←、在行尾按→：切换分组
}

void selectVisibleItemAtEdge(int direction) {
    if (!g_liebiao) return;
    int itemCount = g_liebiao->count();
    if (itemCount <= 0) return;

    int row = (direction > 0) ? 0 : itemCount - 1;
    while (row >= 0 && row < itemCount) {
        QListWidgetItem *item = g_liebiao->item(row);
        if (item && !item->isHidden()) {
            g_liebiao->setCurrentItem(item);
            g_liebiao->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            return;
        }
        row += direction;
    }
}

bool hasQuickSayBlockingWindow() {
    if (g_quickSayPressBlockCount > 0) return true;
    if (g_yuanshengCaidanZhankai) return true; // 托盘的原生右键菜单开着的时候也要放行按键
    if (QApplication::activeModalWidget() || QApplication::activePopupWidget()) return true;
    return (g_shezhichuangkou && g_shezhichuangkou->isVisible()) ||
        (g_tianjiachuangkou && g_tianjiachuangkou->isVisible()) ||
        (g_xiugaichuangkou && g_xiugaichuangkou->isVisible());
}

bool handleQuickSayBrowseKey(DWORD vkCode) {
    if (g_searchMode) return false;
    if (!pchuangkou || !pchuangkou->isVisible()) return false;
    if (hasQuickSayBlockingWindow()) return false;

    bool hasSystemModifier = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ||
        (GetAsyncKeyState(VK_MENU) & 0x8000) ||
        (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
        (GetAsyncKeyState(VK_RWIN) & 0x8000);

    switch (vkCode) {
    case VK_TAB:
        enterSearchMode();
        return true;
    case VK_OEM_3: // 反引号`键，作为图钉按钮的触发键，按一下就切换钉住（和点击图钉按钮等效）//注意：主窗口可见时此键会被吞掉，无法输入反引号字符
        if (g_tuding) g_tuding->click(); // 直接触发图钉按钮的点击，复用其换图标/换提示/改tudingflag/落盘的逻辑，避免状态脱节
        return true;
    case VK_ESCAPE:
        if (QWidget *popup = QApplication::activePopupWidget()) { // 如果右键菜单还在显示，先关闭右键菜单
            popup->close();
        }
        pchuangkou->close();
        return true;
    case VK_RETURN:
        if (config["tudingflag"].toBool() && !config["enter_key_input_phrase_when_pinned"].toBool(true)) return false; // 如果钉住了窗口，并且关闭了“钉住窗口时按下回车键输入短语”，那么不拦截回车键，让它传给前台程序
        if (g_liebiao && g_liebiao->currentItem()) {
            shuchu(g_liebiao->currentItem(), pchuangkou);
            return true;
        }
        return false;
    case VK_LEFT:
        moveCurrentVisibleItemHorizontal(-1);
        return true;
    case VK_RIGHT:
        moveCurrentVisibleItemHorizontal(1);
        return true;
    case VK_UP:
        g_zuoyouZhiqiehuanFenzu = false; // 按了上下键，清除“左右键只切分组”的标记，左右键恢复成正常的行内移动
        moveCurrentVisibleItem(-1);
        return true;
    case VK_DOWN:
        g_zuoyouZhiqiehuanFenzu = false; // 同上
        moveCurrentVisibleItem(1);
        return true;
    case VK_HOME:
        selectVisibleItemAtEdge(1);
        return true;
    case VK_END:
        selectVisibleItemAtEdge(-1);
        return true;
    default:
        break;
    }

    int targetIndex = -1;
    if (vkCode >= '1' && vkCode <= '9') targetIndex = vkCode - '0';
    else if (vkCode == '0') targetIndex = 10;
    else if (vkCode >= 'A' && vkCode <= 'Z') targetIndex = vkCode - 'A' + 11;

    if (targetIndex != -1 && !hasSystemModifier) {
        if (config["tudingflag"].toBool() && !config["badge_key_input_phrase_when_pinned"].toBool(true)) return false; // 如果钉住了窗口，并且关闭了“钉住窗口时按下短语项对应角标输入短语”，那么不拦截角标对应按键，让它传给前台程序
        QListWidgetItem *item = visibleItemByBadgeIndex(targetIndex);
        if (item) {
            shuchu(item, pchuangkou);
            return true;
        }
    }
    return false;
}

LRESULT CALLBACK quickSayKeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT *keyInfo = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if (handleQuickSayBrowseKey(keyInfo->vkCode)) return 1;
    }
    return CallNextHookEx(g_keyboardHook, code, wParam, lParam);
}

void startQuickSayKeyboardHook() {
    if (!g_searchMode && !g_keyboardHook) {
        g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, quickSayKeyboardProc, GetModuleHandleW(nullptr), 0);
    }
}

void showMainWindowNoActivate(QWidget &chuang) {
    g_searchMode = false;
    chuang.setAttribute(Qt::WA_ShowWithoutActivating, true);
    applyMainWindowNoActivate();
    if (!chuang.isVisible()) chuang.show();
    HWND hwnd = (HWND)chuang.winId();
    applyMainWindowNoActivate();
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    HWND insertAfter = config["zhiding"].toBool() ? HWND_TOPMOST : HWND_TOP;
    SetWindowPos(hwnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    startQuickSayKeyboardHook();
}

bool isWindowFrontmost(QWidget &chuang) { // 判断窗口当前是否在屏幕最前方（没有被其他窗口盖住），用窗口中心点处最前方的可见窗口是不是它来判断
    HWND hwnd = (HWND)chuang.winId();
    RECT r;
    if (!GetWindowRect(hwnd, &r)) return true; // 取不到窗口矩形时，保守地认为它在最前
    POINT pt{(r.left + r.right) / 2, (r.top + r.bottom) / 2}; // 窗口中心点的屏幕坐标
    HWND topHwnd = WindowFromPoint(pt); // 返回该屏幕点处最前方的可见窗口
    if (!topHwnd) return false;
    return GetAncestor(topHwnd, GA_ROOT) == hwnd; // 取得它的顶级窗口，如果就是本窗口，那么说明本窗口在中心点处没被盖住，在最前方
}

class NoActivateNativeFilter : public QAbstractNativeEventFilter {
  public:
    bool nativeEventFilter(const QByteArray &, void *message, qintptr *result) override {
        MSG *msg = reinterpret_cast<MSG *>(message);
        if (msg->message == WM_WTSSESSION_CHANGE) { // WTS会明确通知锁屏、注销以及本地/远程会话断开；这些状态下目标窗口已经不可安全继续接收高级输入
            if (msg->wParam == WTS_SESSION_LOCK || msg->wParam == WTS_SESSION_LOGOFF ||
                msg->wParam == WTS_CONSOLE_DISCONNECT || msg->wParam == WTS_REMOTE_DISCONNECT) {
                stopQuickSayOutput();
            }
        } else if (msg->message == WM_POWERBROADCAST && msg->wParam == PBT_APMSUSPEND) { // 睡眠和休眠都会先收到PBT_APMSUSPEND，立刻作废所有待执行输出步骤
            stopQuickSayOutput();
        }
        if (!pchuangkou) return false;
        if (msg->message == WM_MOUSEACTIVATE) {
            if (g_searchMode) return false;
            HWND mainHwnd = (HWND)pchuangkou->winId();
            if (g_search) {
                HWND searchHwnd = (HWND)g_search->winId();
                if (msg->hwnd == searchHwnd || IsChild(searchHwnd, msg->hwnd)) {
                    enterSearchMode();
                    *result = MA_ACTIVATE;
                    return true;
                }
            }
            if (msg->hwnd == mainHwnd || IsChild(mainHwnd, msg->hwnd)) {
                *result = MA_NOACTIVATE;
                return true;
            }
        }
        return false;
    }
};

void xianshi(QWidget &chuang) { // 如果窗口当前不可见，那么显示窗口，同时把窗口拉到屏幕最前方，并获得焦点
    if (pchuangkou && &chuang == pchuangkou) {
        showMainWindowNoActivate(chuang);
        return;
    }
    if (!chuang.isVisible()) chuang.show();
    chuang.activateWindow(); // 把窗口拉到屏幕最前方，并获得焦点
}

bool isValidHotkey(const QKeySequence &seq, QVector<QHotkey *> &itemHotkeys_, QKeySequenceEdit *edit_, QHotkey *selfhk = nullptr) { // 检查快捷键是否合规，如果不合规那么弹出对应警告对话框。规则：1.包含至少一个修饰键（Ctrl/Alt/Shift/Meta），同时有且只能有一个主键；或者可以是一个单独的F1~F11、Insert；2.快捷键不能已经存在动态数组itemHotkeys里，也就是说不能已被短语项使用；3.其他情况，比如用户输入了全局快捷键or已经被其他软件占用的快捷键，那么快捷键输入框会直接失去焦点，导致输入不了最后一个主键，也就是输入为空。这些情况就不用考虑了
    if (seq.isEmpty()) { // 如果快捷键为空 //这个if就是以防万一用的，正常情况下不可能触发这个if
        QMessageBox::warning(edit_, "快捷键不合规", "快捷键不能为空  "); // 弹出警告对话框（多了两个空格是因为要留出空间，保持美观）
        return false;
    }
    if (seq.count() != 1) { // 如果快捷键为多个快捷键的组合
        QMessageBox::warning(edit_, "快捷键不合规", "快捷键不能为多个快捷键的组合  ");
        return false;
    }
    QString seqStr = seq.toString();
    bool hasModifier = false;
    if (seqStr.contains("Ctrl") || seqStr.contains("Alt") || seqStr.contains("Shift") || seqStr.contains("Meta")) hasModifier = true; // 判断是否包含至少一个修饰键（Ctrl/Alt/Shift/Meta）
    QString lastseqStr = seqStr.split('+').last(); // seqStr.split('+')返回一个字符串数组，然后我们用last()取出它的最后一个元素
    bool hasPrimary = false;
    if (lastseqStr != "Ctrl" && lastseqStr != "Alt" && lastseqStr != "Shift" && lastseqStr != "Meta") hasPrimary = true; // 如果最后一个元素不是修饰键，那么判断为包含主键 //因为快捷键输入框的特殊性，所以就不用判断是否只有一个主键了。要我说其实判断是否包含主键这步都可以省略
    bool hasqita = false;
    if (seqStr == "F1" || seqStr == "F2" || seqStr == "F3" || seqStr == "F4" || seqStr == "F5" || seqStr == "F6" || seqStr == "F7" || seqStr == "F8" || seqStr == "F9" || seqStr == "F10" || seqStr == "F11" || seqStr == "Ins") hasqita = true; // 判断是不是一个单独的F1~F11、Insert
    if (hasModifier && hasPrimary || hasqita) { // 如果快捷键字符串满足要求
        for (auto &hk : itemHotkeys_) { // 遍历动态数组，也就是遍历所有短语项对应的QHotkey *对象 //这里使用的是引用遍历
            if (hk) { // 如果hk不为空指针
                if (hk == selfhk) continue; // 如果hk等于传入的selfhk，那么说明用户没有修改快捷键，需要跳过，否则就会自己和自己冲突
                if (hk->shortcut() == seq) { // 返回hk对应的QKeySequence对象，如果它和seq完全相同，那么说明快捷键已被使用
                    QMessageBox::warning(edit_, "快捷键不合规", "快捷键已被使用  ");
                    return false;
                }
            }
        }
        return true; // 如果遍历完成后都没有return，那么返回true
    } else {
        QMessageBox::warning(edit_, "快捷键不合规", "快捷键必须包含至少一个修饰键（Ctrl/Alt/Shift/Meta）和一个主键；  \n或者是一个单独的F1~F11、Insert。  ");
        return false;
    }
}

class NoEscCloseWidget : public QWidget {
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

// QuickSay的样式表，实现类似于Windows 11的现代UI风格。
// 注意它不是挂在QApplication上的：那样会连设置窗口一起染上，而设置窗口要的是Windows原生控件外观。
// 所以改成谁需要就挂给谁——主窗口、添加窗口、修改窗口、高级输入帮助窗口，以及那几个没有父对象的右键菜单
static const char *g_quanjuQss = R"(
    /*====================全局基础样式====================*/
    QWidget{
        background-color: #f3f3f3;                                                      /*主背景色：浅灰色*/
        font-family: "Inter","Segoe UI","Microsoft YaHei UI","Microsoft YaHei","Source Han Sans SC","PingFang SC","Helvetica Neue",Arial,sans-serif,"Segoe UI Symbol","Euphemia","Gadugi"; /*字体优先级*/
        font-size: 9pt;                                                                 /*全局字体大小：9磅*/
        font-weight: 400;                                                               /*全局字体粗细*/
        color: #323130;                                                                 /*全局字体颜色：深灰色*/
        selection-background-color: #0078d4;                                            /*文本选中背景色：蓝色*/
        selection-color: #ffffff;                                                       /*文本选中字体色：白色*/
    }
    /*====================图标按钮样式（专门为设置、添加按钮写的样式）====================*/
    QPushButton#iconButton{
        background-color: transparent;              /*按钮背景：透明*/
        border: none;                               /*按钮边框：无边框，完全隐藏边框*/
        border-radius: 4px;                         /*按钮圆角*/
        padding: 0px;                               /*按钮内边距*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    QPushButton#iconButton:hover{                   /*鼠标悬停时的按钮样式*/
        background-color: #f9f9f9;                  /*悬停背景：白色*/
        border: none;                               /*无边框*/
    }
    QPushButton#iconButton:pressed{                 /*按钮被按下时的样式*/
        background-color: #f7f7f7;                  /*按下背景：深一点的白色*/
        border: none;                               /*无边框*/
    }
    QPushButton#iconButton:focus{                   /*按钮获得焦点时的样式*/
        border: none;                               /*无边框*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    /*====================按钮样式====================*/
    QPushButton{
        background-color: #ffffff;                  /*按钮背景：纯白色*/
        border: 1px solid #d1d1d1;                  /*按钮边框：1像素浅灰色*/
        border-radius: 4px;                         /*按钮圆角*/
        padding: 6px 12px;                          /*按钮内边距：上下6像素，左右12像素*/
        color: #323130;                             /*按钮字体颜色：深灰色*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    QPushButton:hover{
        background-color: #f9f9f9;                  /*鼠标悬停背景：白色*/
        border-color: #c8c8c8;                      /*悬停边框：浅灰色*/
    }
    QPushButton:pressed{
        background-color: #e5f3ff;                  /*鼠标按下背景：白色*/
        border-color: #0078d4;                      /*按下边框：蓝色*/
        color: #005a9e;                             /*按下文字：深蓝色*/
    }
    QPushButton:focus{
        border-color: #0078d4;                      /*焦点边框：蓝色*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    /*====================列表控件样式====================*/
    QListWidget{
        background-color: #fcfcfc;                  /*列表背景：微灰白色，与短语项的#ffffff形成微妙对比，让界面更有深度感和专业感*/
        border: 1px solid #e5e5e5;                  /*列表边框：1像素浅灰色，营造阴影效果*/
        border-radius: 6px;                         /*列表圆角*/
        padding: 4px;                               /*列表内边距：上下左右4像素*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
        selection-background-color: transparent;    /*禁用默认选中样式*/
        alternate-background-color: transparent;    /*禁用交替行背景色*/
    }
    QListWidget::item{                              /*因为短语字体颜色和备注字体颜色不同，所以不能在这里设置字体颜色，不然在这里设置的字体颜色会覆盖在其他地方设置的字体颜色*/
        background-color: #ffffff;                  /*短语项背景：纯白色*/
        border: 1px solid #e5e5e5;                  /*短语项边框：1像素浅灰色*/
        border-radius: 4px;                         /*短语项圆角，与列表圆角一致*/
        margin: 2px 2px;                            /*短语项外边距：上下2像素，左右2像素*//*【【【注：想让短语项更紧凑一点在这里修改】】】*/
    }
    QListWidget::item:hover{
        background-color: #f9f9f9;                  /*鼠标悬停短语项背景：白色*/
        border-color: #d1d1d1;                      /*鼠标悬停短语项边框：浅灰色*/
    }
    QListWidget::item:selected{
        background-color: #e5f3ff;                  /*鼠标选中短语项背景：偏蓝一点的白色*/
        border-color: #0078d4;                      /*鼠标选中短语项边框：蓝色*/
    }
    QListWidget::item:selected:hover{
        background-color: #cce7ff;                  /*鼠标选中且悬停，短语项背景：浅蓝色*/
        border-color: #106ebe;                      /*鼠标选中且悬停，短语项边框：更深的蓝色*/
    }
    /*====================分组栏样式====================*/
    QTabBar{
        background-color: transparent;              /*分组栏背景：透明*/
        border: none;                               /*无边框*/
    }
    QTabBar::tab{
        background-color: #ffffff;                  /*分组背景：纯白色*/
        color: #323130;                             /*分组字体颜色：深灰色*/
        border: 1px solid #e5e5e5;                  /*分组边框：1像素浅灰色*/
        border-radius: 4px;                         /*分组圆角*/
        padding: 8px 16px;                          /*分组内边距：上下8像素，左右16像素*/
        margin: 2px 2px;                            /*分组外边距：上下2像素，左右2像素*/
        min-width: 30px;                            /*分组最小宽度*//*【【【注：想修改分组最小宽度在这里修改】】】*/
    }
    QTabBar::tab:hover{
        background-color: #f9f9f9;                  /*鼠标悬停分组背景：白色*/
        border-color: #d1d1d1;                      /*鼠标悬停分组边框：浅灰色*/
    }
    QTabBar::tab:selected{
        background-color: #e5f3ff;                  /*鼠标选中分组背景：偏蓝一点的白色*/
        border-color: #0078d4;                      /*鼠标选中分组边框：蓝色*/
        color: #005a9e;                             /*鼠标选中分组字体颜色：深蓝色*/
    }
    QTabBar::tab:selected:hover{
        background-color: #cce7ff;                  /*鼠标选中且悬停，分组背景：浅蓝色*/
        border-color: #106ebe;                      /*鼠标选中且悬停，分组边框：更深的蓝色*/
    }
    /*====================输入框样式====================*/
    QLineEdit,QPlainTextEdit,QTextEdit{
        background-color: #ffffff;                  /*输入框背景：纯白色*/
        border: 2px solid #e5e5e5;                  /*输入框边框：2像素浅灰色*/
        border-radius: 4px;                         /*输入框圆角：4像素*/
        padding: 8px 12px;                          /*输入框内边距：上下8像素，左右12像素*/
        color: #323130;                             /*输入框字体颜色：深灰色*/
        selection-background-color: #0078d4;        /*选中背景：蓝色*/
        selection-color: #ffffff;                   /*选中文字：白色*/
    }
    QLineEdit:focus,QPlainTextEdit:focus,QTextEdit:focus{
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    /*====================数字输入框样式====================*/
    QSpinBox{
        background-color: #ffffff;                  /*数字框背景：纯白色*/
        border: 2px solid #e5e5e5;                  /*数字框边框：2像素浅灰色*/
        border-radius: 4px;                         /*数字框圆角：4像素*/
        padding: 6px 8px;                           /*数字框内边距：上下6像素，左右8像素*//*【【【注：想修改数字输入框大小在这里修改】】】*/
        color: #323130;                             /*数字框字体颜色*/
    }
    QSpinBox:focus{
        border-color: #0078d4;                      /*焦点边框：蓝色*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    QSpinBox::up-button,QSpinBox::down-button{      /*完全隐藏数字输入框的上下箭头*/
        width: 0px;
        height: 0px;
        border: none;
        background: none;
    }
    /*====================快捷键输入框样式====================*/
    QKeySequenceEdit QLineEdit{                     /*对快捷键输入框QKeySequenceEdit内部的QLineEdit设置样式*/
        background-color: #ffffff;                  /*快捷键框背景：纯白色*/
        border: 2px solid #e5e5e5;                  /*快捷键框边框：2像素浅灰色*/
        border-radius: 4px;                         /*快捷键框圆角：4像素*/
        padding: 8px 12px;                          /*快捷键框内边距：上下8像素，左右12像素*//*【【【注：想修改快捷键输入框大小在这里修改】】】*/
        color: #323130;                             /*快捷键框字体颜色*/
    }
    QKeySequenceEdit QLineEdit:focus{
        border-color: #0078d4;                      /*焦点边框：蓝色*/
        outline: none;                              /*用于去掉焦点时的虚线边框*/
    }
    /*====================右键菜单样式====================*/
    QMenu {
        background-color: #fefefe;                  /*菜单背景：白色*/
        border: 1px solid #e5e5e5;                  /*菜单边框：1像素浅灰色*/
        padding: 3px 3px;                           /*菜单内边距：上下4像素，左右0像素*/
    }
    QMenu::item {
        padding: 8px 16px;                          /*菜单项内边距：上下8像素，左右16像素*/
        color: #222222;                             /*字体颜色*/
        min-width: 60px;                            /*菜单项最小宽度*/
    }
    QMenu::item:selected {
        background-color: #e5f3ff;                  /*菜单项选中背景：浅蓝色*/
    }
    QMenu::item:pressed {
        background-color: #cce7ff;                  /*菜单项按下背景：深蓝色*/
    }
    /*====================标签样式====================*/
    QLabel{
        color: #605e5c;                             /*标签字体颜色：深灰色*/
        padding: 2px 0px;                           /*标签内边距：上下2像素，左右0像素*/
    }
    /*====================滚动条样式====================*/
    QScrollBar:vertical{                            /*垂直滚动条*/
        background-color: #f3f3f3;                  /*背景颜色：浅灰色*/
        width: 8px;                                 /*滚动条宽度：8像素*/
        border-radius: 4px;                         /*滚动条圆角：4像素*/
        margin: 0px;                                /*滚动条外边距：0像素，也就是无*/
        border: none;                               /*去掉边框*/
    }
    QScrollBar::handle:vertical{
        background-color: #c7c7c7;                  /*滑块颜色：浅灰色*/
        border-radius: 4px;                         /*滑块圆角：4像素*/
        min-height: 20px;                           /*滑块最小高度：20像素，确保可拖拽*/
        margin: 2px 1px;                            /*滑块外边距：上下2像素，左右1像素*/
    }
    QScrollBar::handle:vertical:hover{
        background-color: #a6a6a6;                  /*鼠标悬停滑块颜色：灰色*/
    }
    QScrollBar::handle:vertical:pressed{
        background-color: #8a8a8a;                  /*鼠标按下滑块颜色：更深的灰色*/
    }
    QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{
        height: 0px;                                /*隐藏滚动条自带的那个箭头*/
    }
    QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{
        background-color: transparent;              /*滚动条轨道颜色：透明*/
    }
    QScrollBar:horizontal{                          /*平行滚动条*/
        background-color: #f3f3f3;                  /*背景颜色：浅灰色*/
        height: 8px;                                /*滚动条高度：8像素*/
        border-radius: 4px;                         /*滚动条圆角：4像素*/
        margin: 0px;                                /*滚动条外边距：0像素，也就是无*/
        border: none;                               /*去掉边框*/
    }
    QScrollBar::handle:horizontal{
        background-color: #c7c7c7;                  /*滑块颜色：浅灰色*/
        border-radius: 4px;                         /*滑块圆角：4像素*/
        min-width: 20px;                            /*滑块最小宽度：20像素，确保可拖拽*/
        margin: 1px 2px;                            /*滑块边距：上下1像素，左右2像素*/
    }
    QScrollBar::handle:horizontal:hover{
        background-color: #a6a6a6;                  /*鼠标悬停滑块颜色：灰色*/
    }
    QScrollBar::handle:horizontal:pressed{
        background-color: #8a8a8a;                  /*鼠标按下滑块颜色：更深的灰色*/
    }
    QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{
        width: 0px;                                 /*隐藏滚动条自带的那个箭头*/
    }
    QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal{
        background-color: transparent;              /*滚动条轨道颜色：透明*/
    }
    /*====================悬停提示样式====================*/
    QToolTip{
        background-color: #ffffff;                  /*提示背景颜色：纯白色*/
        color: #222222;                             /*提示字体颜色：深灰色*/
        padding: 2px 1px;                           /*提示内边距：上下2像素，左右1像素*/
    }
)";

void showAdvancedInputHelp(QWidget &parent) {
    NoEscCloseWidget *helpWindow = new NoEscCloseWidget();
    helpWindow->setStyleSheet(g_quanjuQss);
    helpWindow->setAttribute(Qt::WA_DeleteOnClose);
    helpWindow->setWindowTitle("如何更高级地输入？");
    helpWindow->setWindowIcon(parent.windowIcon());

    QTextBrowser *helpEdit = new QTextBrowser(helpWindow);
    helpEdit->setReadOnly(true);
    helpEdit->setOpenLinks(false);
    const QString basicTutorialHtml = R"(
<html>
  <head>
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
  </head>
  <body>
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
  </body>
</html>
    )";

    const QString fullTutorialHtml = R"(
<html>
  <head>
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
  </head>
  <body>
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
  </body>
</html>
    )";

    QObject::connect(helpEdit, &QTextBrowser::anchorClicked,
        [helpWindow, helpEdit, basicTutorialHtml, fullTutorialHtml](const QUrl &url) {
            if (url == QUrl("quicksay://full-tutorial")) {
                helpWindow->setFixedSize(500, 500);
                helpEdit->setGeometry(0, 0, 500, 500);
                helpEdit->setHtml(fullTutorialHtml);
            } else if (url == QUrl("quicksay://basic-tutorial")) {
                helpWindow->setFixedSize(500, 500);
                helpEdit->setGeometry(0, 0, 500, 500);
                helpEdit->setHtml(basicTutorialHtml);
            }
        });
    helpWindow->setFixedSize(500, 500);
    helpEdit->setGeometry(0, 0, 500, 500);
    helpEdit->setHtml(basicTutorialHtml);
    helpEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    helpEdit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    helpWindow->move(parent.x() + 30, parent.y() + 30);
    helpWindow->show();
    helpWindow->raise();
    helpWindow->activateWindow();
}

void adjustAllWindows(int w, int h, // 根据设置里的宽高调整主窗口；其他窗口始终使用默认的500*500
    QWidget &chuangkou, QListWidget &liebiao, QTabBar &tabBar, QLineEdit &search, QPushButton &shezhi, QPushButton &tianjia, QPushButton &tuding, // 主窗口
    QWidget &tianjiachuangkou, QPlainTextEdit &tianjiakuang, QPushButton &tianjia_gaojishuru, QLabel &tianjia_beizhuwenben, QPlainTextEdit &tianjia_beizhukuang, QLabel &tianjia_kjjwenben, QKeySequenceEdit &tianjia_kjjkuang, QPushButton &tianjia_kjjqingkong, QPushButton &tianjiaquxiao, QPushButton &tianjiaqueding, // 添加窗口
    QWidget &xiugaichuangkou, QPlainTextEdit &xiugaikuang, QPushButton &xiugai_gaojishuru, QLabel &xiugai_beizhuwenben, QPlainTextEdit &xiugai_beizhukuang, QLabel &xiugai_kjjwenben, QKeySequenceEdit &xiugai_kjjkuang, QPushButton &xiugai_kjjqingkong, QPushButton &xiugaiquxiao, QPushButton &xiugaiqueding // 修改窗口
) {
    // 主窗口
    chuangkou.setFixedSize(w, h);
    liebiao.move(0, 46);
    liebiao.setFixedSize(w, h - 40); // 500*460 //把liebiao最下面的6个像素放到窗口外，隐藏起来。这样平行滚动条就不会不好看了
    tabBar.move(5, 5);
    tabBar.setFixedSize(w - 230, 40); // 270*40
    search.move(w - 205, 7); // 295,7
    search.setFixedSize(90, 35);
    shezhi.move(w - 41, 5); // 459,5
    shezhi.setFixedSize(36, 36);
    tianjia.move(w - 77, 5); // 423,5
    tianjia.setFixedSize(36, 36);
    tuding.move(w - 113, 5); // 387,5
    tuding.setFixedSize(36, 36);

    w = 500; // 其他窗口暂时不跟随“主窗口大小”设置，一直使用默认宽度
    h = 500; // 其他窗口暂时不跟随“主窗口大小”设置，一直使用默认高度

    // 添加窗口
    tianjiachuangkou.setFixedSize(w, h);
    tianjiakuang.move(0, 0);
    tianjiakuang.setFixedSize(w, h * 0.4); // 500*200
    tianjia_gaojishuru.move(w * 0.7, h * 0.4); // 350,200
    tianjia_gaojishuru.setFixedSize(147, 35);
    tianjia_beizhuwenben.move(10, h * 0.5 - 20); // 10,230
    tianjia_beizhuwenben.setFixedSize(50, 20);
    tianjia_beizhukuang.move(0, h * 0.5); // 0,250
    tianjia_beizhukuang.setFixedSize(w, h * 0.25); // 500*125
    tianjia_kjjwenben.move(10, h * 0.8 + 7); // 10,407
    tianjia_kjjwenben.setFixedSize(50, 20);
    tianjia_kjjkuang.move(70, h * 0.8); // 70,400
    tianjia_kjjkuang.setFixedSize(w * 0.4, 38); // 200*38
    tianjia_kjjqingkong.move(w * 0.4 + 70, h * 0.8); // 270,400
    tianjia_kjjqingkong.setFixedSize(57, 38);
    tianjiaquxiao.move(0, h - 37); // 0,463
    tianjiaquxiao.setFixedSize(w / 2, 33); // 250*33
    tianjiaqueding.move(w / 2, h - 37); // 250,463
    tianjiaqueding.setFixedSize(w / 2, 33); // 250*33

    // 修改窗口
    xiugaichuangkou.setFixedSize(w, h);
    xiugaikuang.move(0, 0);
    xiugaikuang.setFixedSize(w, h * 0.4); // 500*200
    xiugai_gaojishuru.move(w * 0.7, h * 0.4); // 350,200
    xiugai_gaojishuru.setFixedSize(147, 35);
    xiugai_beizhuwenben.move(10, h * 0.5 - 20); // 10,230
    xiugai_beizhuwenben.setFixedSize(50, 20);
    xiugai_beizhukuang.move(0, h * 0.5); // 0,250
    xiugai_beizhukuang.setFixedSize(w, h * 0.25); // 500*125
    xiugai_kjjwenben.move(10, h * 0.8 + 7); // 10,407
    xiugai_kjjwenben.setFixedSize(50, 20);
    xiugai_kjjkuang.move(70, h * 0.8); // 70,400
    xiugai_kjjkuang.setFixedSize(w * 0.4, 38); // 200*38
    xiugai_kjjqingkong.move(w * 0.4 + 70, h * 0.8); // 270,400
    xiugai_kjjqingkong.setFixedSize(57, 38);
    xiugaiquxiao.move(0, h - 37); // 0,463
    xiugaiquxiao.setFixedSize(w / 2, 33); // 250*33
    xiugaiqueding.move(w / 2, h - 37); // 250,463
    xiugaiqueding.setFixedSize(w / 2, 33); // 250*33
}

// 自定义一个事件过滤器类WindowMoveFilter，实现：当用户移动窗口时记录窗口位置。使得呼出窗口时让窗口在记录的位置显示
class WindowMoveFilter : public QObject {
  private:
    QWidget *chuangkou; // 指向主窗口
    QWidget *shezhichuangkou; // 指向设置窗口
    QWidget *tianjiachuangkou; // 指向添加窗口
    QWidget *xiugaichuangkou; // 指向修改窗口
    QString configPath_; // 指向config.json文件路径
  public:
    WindowMoveFilter(QWidget *main, QWidget *shezhi, QWidget *tianjia, QWidget *xiugai, const QString &path, QObject *parent = nullptr) : chuangkou(main), shezhichuangkou(shezhi), tianjiachuangkou(tianjia), xiugaichuangkou(xiugai), configPath_(path), QObject(parent) {}

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (g_zhengzaiDaoruChongqi) return QObject::eventFilter(obj, event); // 导入完成后窗口关闭、失焦时可能再产生Move事件，不能让旧坐标写回刚导入的config.json
        if (event->type() == QEvent::Move) { // 如果是窗口移动事件
            if (obj == chuangkou) { // 如果移动的是主窗口
                config["chuangkou_x"] = chuangkou->x(); // 记录主窗口在x轴上的位置
                config["chuangkou_y"] = chuangkou->y(); // 记录主窗口在y轴上的位置
                saveConfig(configPath_); // 写入程序设置到config.json
            } else if (shezhichuangkou->isActiveWindow()) { // 如果焦点在设置窗口
                config["shezhichuangkou_x"] = shezhichuangkou->x(); // 记录设置窗口在x轴上的位置
                config["shezhichuangkou_y"] = shezhichuangkou->y(); // 记录设置窗口在y轴上的位置
                saveConfig(configPath_);
            } else if (tianjiachuangkou->isActiveWindow()) { // 如果焦点在添加窗口
                config["tianjiachuangkou_x"] = tianjiachuangkou->x(); // 记录添加窗口在x轴上的位置
                config["tianjiachuangkou_y"] = tianjiachuangkou->y(); // 记录添加窗口在y轴上的位置
                saveConfig(configPath_);
            } else if (xiugaichuangkou->isActiveWindow()) { // 如果焦点在修改窗口
                config["xiugaichuangkou_x"] = xiugaichuangkou->x(); // 记录修改窗口在x轴上的位置
                config["xiugaichuangkou_y"] = xiugaichuangkou->y(); // 记录修改窗口在y轴上的位置
                saveConfig(configPath_);
            }
        }
        return QObject::eventFilter(obj, event); // 其他事件走默认处理
    }
};

// 自定义一个事件过滤器类MyEventFilter，实现：Esc键可以关闭主窗口/添加窗口/修改窗口；回车键Enter可以输出光标处短语；左右方向键可以切换分组
class MyEventFilter : public QObject {
  private:
    QWidget *chuangkou; // 指向主窗口
    QWidget *tianjiachuangkou; // 指向添加窗口
    QWidget *xiugaichuangkou; // 指向修改窗口
    QPushButton *tianjiaquxiao; // 指向添加窗口的取消按钮
    QPushButton *xiugaiquxiao; // 指向修改窗口的取消按钮
    QListWidget *liebiao; // 指向主窗口里的列表
    QTabBar *tabBar; // 指向主窗口里的分组栏
    QLineEdit *search; // 指向主窗口里的搜索框
  public:
    MyEventFilter(QWidget *main, QWidget *tianjia, QWidget *xiugai, QPushButton *tjqx, QPushButton *xgqx, QListWidget *l, QTabBar *t, QLineEdit *s) : chuangkou(main), tianjiachuangkou(tianjia), xiugaichuangkou(xiugai), tianjiaquxiao(tjqx), xiugaiquxiao(xgqx), liebiao(l), tabBar(t), search(s) {}

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (obj == chuangkou && event->type() == QEvent::Hide) { // 主窗口关闭到托盘时，一并隐藏可能还在显示的鼠标悬停提示
            QToolTip::hideText();
        }
        if (obj == search && event->type() == QEvent::FocusOut && g_searchMode) {
            QTimer::singleShot(0, []() {
                if (g_searchMode && (!g_search || !g_search->hasFocus()) && (!pchuangkou || !pchuangkou->isActiveWindow())) {
                    leaveSearchMode(false);
                }
            });
        }
        if (event->type() == QEvent::KeyPress) { // 如果是键盘按下事件
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (g_searchMode && search->hasFocus()) {
                if (keyEvent->key() == Qt::Key_Escape) {
                    if (!search->text().isEmpty()) search->clear();
                    leaveSearchMode();
                    return true;
                }
                if (keyEvent->key() == Qt::Key_Tab) {
                    leaveSearchMode();
                    return true;
                }
                if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                    QListWidgetItem *item = liebiao->currentItem();
                    leaveSearchMode();
                    if (item) {
                        shuchu(item, chuangkou);
                    }
                    return true;
                }
                if (keyEvent->key() == Qt::Key_Up) {
                    moveCurrentVisibleItem(-1);
                    return true;
                }
                if (keyEvent->key() == Qt::Key_Down) {
                    moveCurrentVisibleItem(1);
                    return true;
                }
                return false;
            }
            if (keyEvent->key() == Qt::Key_Tab && chuangkou->isActiveWindow()) { // 如果按下的是Tab键，并且焦点在主窗口
                if (search->hasFocus()) liebiao->setFocus(); // 如果焦点在搜索框，那么给列表焦点
                else search->setFocus(); // 如果焦点不在搜索框，那么给搜索框焦点
                return true; // 拦截事件，不再向下传递
            }
            if (keyEvent->key() == Qt::Key_Escape) { // 如果按下的是Esc键
                if (chuangkou->isActiveWindow()) { // 如果焦点在主窗口
                    if (QWidget *popup = QApplication::activePopupWidget()) { // 如果右键菜单还在显示，先关闭右键菜单
                        popup->close();
                    }
                    chuangkou->close(); // 关闭主窗口
                    return true;
                } else if (tianjiachuangkou->isActiveWindow()) { // 如果焦点在添加窗口
                    tianjiaquxiao->click(); // 相当于按下“取消”按钮
                    return true;
                } else if (xiugaichuangkou->isActiveWindow()) { // 如果焦点在修改窗口
                    xiugaiquxiao->click(); // 相当于按下“取消”按钮
                    return true;
                }
            }
            if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) && chuangkou->isActiveWindow()) { // 如果按下的是回车键，并且焦点在主窗口
                if (search->hasFocus()) { // 如果焦点在搜索框
                    liebiao->setFocus(); // 给列表焦点
                    return true;
                }
                QListWidgetItem *item = liebiao->currentItem(); // 获取当前选中的短语项
                if (item) { // 如果有选中的项，那么调用shuchu函数
                    shuchu(item, chuangkou);
                    return true;
                }
            }
            if (keyEvent->key() == Qt::Key_Left && chuangkou->isActiveWindow()) { // 如果按下的是左方向键，并且焦点在主窗口
                if (search->hasFocus()) liebiao->setFocus(); // 如果焦点在搜索框，那么给列表焦点
                moveCurrentVisibleItemHorizontal(-1); // 设置列表选中短语项为 同一行左边的那个短语项；已经在行首就切换到上一个分组
                return true;
            }
            if (keyEvent->key() == Qt::Key_Right && chuangkou->isActiveWindow()) { // 如果按下的是右方向键，并且焦点在主窗口
                if (search->hasFocus()) liebiao->setFocus(); // 如果焦点在搜索框，那么给列表焦点
                moveCurrentVisibleItemHorizontal(1); // 设置列表选中短语项为 同一行右边的那个短语项；已经在行尾就切换到下一个分组
                return true;
            }
            if (keyEvent->key() == Qt::Key_Up && chuangkou->isActiveWindow()) { // 如果按下的是上方向键，并且焦点在主窗口
                if (search->hasFocus()) liebiao->setFocus(); // 如果焦点在搜索框，那么给列表焦点
                moveCurrentVisibleItem(-1); // 设置列表选中短语项为 当前选中短语项上边的那个短语项
                return true;
            }
            if (keyEvent->key() == Qt::Key_Down && chuangkou->isActiveWindow()) { // 如果按下的是下方向键，并且焦点在主窗口
                if (search->hasFocus()) liebiao->setFocus(); // 如果焦点在搜索框，那么给列表焦点
                moveCurrentVisibleItem(1); // 设置列表选中短语项为 当前选中短语项下边的那个短语项
                return true;
            }

            if (chuangkou->isActiveWindow() && !search->hasFocus()) { // 如果焦点在主窗口并且焦点不在搜索框
                int key = keyEvent->key(); // 获取按下的键值
                int targetIndex = -1; // 计算按下的键值对应第几条短语
                if (key >= Qt::Key_1 && key <= Qt::Key_9) { // 如果按下的是1~9
                    targetIndex = key - Qt::Key_0; // 对应第1~9条短语
                } else if (key == Qt::Key_0) { // 如果按下的是0
                    targetIndex = 10; // 对应第10条短语
                } else if (key >= Qt::Key_A && key <= Qt::Key_Z) { // 如果按下的是A~Z
                    targetIndex = key - Qt::Key_A + 11; // 对应第11~36条短语
                }
                if (targetIndex != -1) {
                    int visibleCount = 0; // 用于计数当前遍历到的可见项
                    for (int i = 0; i < liebiao->count(); i++) {
                        QListWidgetItem *item = liebiao->item(i);
                        if (!item->isHidden()) { // 如果该短语项没有被隐藏
                            visibleCount++;
                            if (visibleCount == targetIndex) { // 如果计数等于targetIndex
                                shuchu(item, chuangkou);
                                return true;
                            }
                        }
                    }
                }
            }
        }
        return QObject::eventFilter(obj, event); // 其他事件走默认处理
    }
};

// 为短语列表单独处理悬停提示，避免无焦点主窗口下Qt默认的item tooltip不触发
class PhraseItemToolTipFilter : public QObject {
  private:
    QListWidget *list_;
    QTimer showTimer_;
    QPoint lastViewportPos_;
    QPoint lastGlobalPos_;
    QListWidgetItem *lastItem_ = nullptr;

  public:
    PhraseItemToolTipFilter(QListWidget *list, QObject *parent = nullptr) : QObject(parent), list_(list) {
        showTimer_.setSingleShot(true);
        showTimer_.setInterval(250); // 当鼠标悬停在短语项上时，延迟250毫秒后显示鼠标悬停提示
        QObject::connect(&showTimer_, &QTimer::timeout, [&]() {
            showToolTipForLastItem();
        });
    }

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (!list_ || obj != list_->viewport()) return QObject::eventFilter(obj, event);

        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            QListWidgetItem *item = list_->itemAt(mouseEvent->pos());
            if (item && !item->isHidden() && !item->toolTip().isEmpty()) {
                if (item != lastItem_ || mouseEvent->pos() != lastViewportPos_) {
                    QToolTip::hideText();
                    lastItem_ = item;
                    lastViewportPos_ = mouseEvent->pos();
                    lastGlobalPos_ = list_->viewport()->mapToGlobal(mouseEvent->pos());
                    showTimer_.start();
                }
            } else {
                hideToolTip();
            }
            return false;
        }

        if (event->type() == QEvent::ToolTip) {
            if (showToolTipForLastItem()) return true;
            hideToolTip();
            return true;
        }

        if (event->type() == QEvent::Leave ||
            event->type() == QEvent::Wheel ||
            event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::MouseButtonRelease) {
            hideToolTip();
        }

        return QObject::eventFilter(obj, event);
    }

  private:
    bool showToolTipForLastItem() {
        if (!list_ || !lastItem_ || lastItem_->isHidden() || lastItem_->toolTip().isEmpty()) return false;
        QRect itemRect = list_->visualItemRect(lastItem_);
        if (!itemRect.contains(lastViewportPos_)) return false;
        QToolTip::showText(lastGlobalPos_, lastItem_->toolTip(), list_->viewport(), itemRect);
        return true;
    }

    void hideToolTip() {
        showTimer_.stop();
        lastItem_ = nullptr;
        QToolTip::hideText();
    }
};

// 自定义一个事件过滤器类PhraseListResizeFilter，实现：列表可视区宽度一变（改窗口大小、滚动条冒出来或者收回去）就重算一次每个短语项的宽度，保证一行还是正好放得下设定的短语数
// 不用担心来回抖动：一行放几个是固定的，所以总行数、总高度都不会因为短语项宽度变化而变化，滚动条也就不会跟着反复出现和消失
class PhraseListResizeFilter : public QObject {
  private:
    QListWidget *liebiao; // 指向主窗口里的列表
    QTabBar *tabBar; // 指向主窗口里的分组栏，用来取当前分组的每行短语数
  public:
    PhraseListResizeFilter(QListWidget *l, QTabBar *t, QObject *parent = nullptr) : QObject(parent), liebiao(l), tabBar(t) {}

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (obj == liebiao->viewport() && event->type() == QEvent::Resize && tabBar->currentIndex() >= 0) {
            updatePhraseListColumns(*liebiao, tabColumns(*tabBar, tabBar->currentIndex())); // 只重算短语项宽度，不重设样式表，不然setStyleSheet又会引起一次重新布局
        }
        return QObject::eventFilter(obj, event);
    }
};

// 为快捷键输入框tianjia_kjjkuang、xiugai_kjjkuang自定义一个事件过滤器类，用于拦截它们的焦点事件，实现：当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象，失去焦点时恢复
class KjjHotkeyEditFilter : public QObject {
  private:
    QKeySequenceEdit *edit_; // 指向快捷键输入框xiugai_kjjkuang
    QVector<QHotkey *> &itemHotkeys_; // 引用动态数组itemHotkeys
  public:
    KjjHotkeyEditFilter(QKeySequenceEdit *e, QVector<QHotkey *> &itemHotkeys, QObject *parent = nullptr) : edit_(e), itemHotkeys_(itemHotkeys), QObject(parent) {}

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override { // 重写eventFilter()以拦截事件
        if (obj == edit_ && event->type() == QEvent::FocusIn) { // 当xiugai_kjjkuang获得焦点
            for (auto &hk : itemHotkeys_) { // 遍历动态数组，也就是遍历所有短语项对应的QHotkey *对象 //这里使用的是引用遍历
                if (hk) hk->setRegistered(false); // 如果hk不为空指针，那么禁用当前已注册的快捷键hk
            }
            return false; // 返回false，不拦截事件，让快捷键输入框继续正常处理焦点。此时用户可继续输入新快捷键
        }
        if (obj == edit_ && event->type() == QEvent::FocusOut) { // 当xiugai_kjjkuang失去焦点
            for (auto &hk : itemHotkeys_) { // 遍历动态数组，也就是遍历所有短语项对应的QHotkey *对象
                if (hk) hk->setRegistered(true); // 如果hk不为空指针，那么恢复当前已注册的快捷键hk
            }
            return false; // 返回false，不拦截事件，让快捷键输入框继续正常处理焦点
        }
        return QObject::eventFilter(obj, event); // 其他事件走默认处理
    }
};

// 为快捷键输入框hotkeyEdit单独自定义一个事件过滤器类，用于拦截它的焦点事件，实现：1.当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象，失去焦点时恢复；2.当输入框获得焦点时立即禁用当前已注册的全局快捷键；3.当输入框失去焦点时判断用户输入的快捷键是否合规，合规的话就应用，不合规的话就恢复输入框为原始快捷键、弹出警告对话框
class HotkeyEditFilter : public QObject {
  private:
    QKeySequenceEdit *edit_; // 指向快捷键输入框hotkeyEdit
    QHotkey *hotkey_; // 指向hotkey，就是那个QHotkey *对象
    QVector<QHotkey *> &itemHotkeys_; // 引用动态数组itemHotkeys
  public:
    HotkeyEditFilter(QKeySequenceEdit *e, QHotkey *h, QVector<QHotkey *> &itemHotkeys, QObject *parent = nullptr) : edit_(e), hotkey_(h), itemHotkeys_(itemHotkeys), QObject(parent) {}

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override { // 重写eventFilter()以拦截事件
        if (obj == edit_ && event->type() == QEvent::FocusIn) { // 当hotkeyEdit获得焦点
            for (auto &hk : itemHotkeys_) { // 遍历动态数组，也就是遍历所有短语项对应的QHotkey *对象
                if (hk) hk->setRegistered(false); // 如果hk不为空指针，那么禁用当前已注册的快捷键hk
            }
            if (hotkey_) hotkey_->setRegistered(false); // 如果hotkey_不为空指针，那么禁用当前已注册的全局快捷键
            return false; // 返回false，不拦截事件，让快捷键输入框继续正常处理焦点。此时用户可继续输入新快捷键
        }
        if (obj == edit_ && event->type() == QEvent::FocusOut) { // 当hotkeyEdit失去焦点
            for (auto &hk : itemHotkeys_) { // 遍历动态数组，也就是遍历所有短语项对应的QHotkey *对象
                if (hk) hk->setRegistered(true); // 如果hk不为空指针，那么恢复当前已注册的快捷键hk
            }
            QKeySequence seq = edit_->keySequence(); // 取出用户在输入框里输入的快捷键
            if (seq.isEmpty() || !isValidHotkey(seq, itemHotkeys_, edit_)) { // 输入为空、或者快捷键不合规（调用isValidHotkey函数检查快捷键是否合规）
                edit_->setKeySequence(QKeySequence(config["hotkey"].toString())); // 恢复输入框为原始快捷键
            }
            // 注意：合规的新快捷键在这里只是留在输入框里，不注册也不落盘。它要等用户点了设置窗口的“应用”或“确定”才会真的生效
            if (hotkey_) hotkey_->setRegistered(true); // 把原来那个全局快捷键重新注册回去，编辑期间的临时注销到此结束
            return false; // 返回false，不拦截事件，让快捷键输入框继续正常处理焦点
        }
        return QObject::eventFilter(obj, event); // 其他事件走默认处理
    }
};

class BadgeDelegate : public QStyledItemDelegate { // 自定义一个委托类，用于在短语项左上角或者右上角绘制角标
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    // 如果data.json中最后一个短语项为：
    // wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww
    // 或者
    // 我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我我
    // 那么所有短语项就会被撑宽，而不是在合适的时候自动换行。其中一个现象是出现水平滚动条。
    // 但如果再添加一个较短的短语，使得data.json中最后一个短语项为：
    // 我
    // 那么所有短语项都会恢复正常的宽度，都会在合适的时候自动换行。其中一个现象是水平滚动条消失。
    // 原因是 QListWidget/QListView 会根据 item delegate 的 sizeHint() 估算内容区域宽度。最后一个可见短语如果是很长的连续英文或中文，没有自然断点，它的宽度提示会变得很大，于是列表内容宽度被撑大，水平滚动条出现；item 绘制区域也随之变宽，所以其他短语看起来也不按窗口宽度换行。最后一项变短后，估算宽度恢复，现象消失。
    // 解决方法就是这个函数↓
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override { // 重写 sizeHint()，沿用 QStyledItemDelegate::sizeHint() 的高度，但自己决定返回宽度
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setWidth(g_phraseCellWidth); // 单列时g_phraseCellWidth是0，也就是把宽度压小，理由见上面那一大段注释；多列时是算好的每格宽度，这样一行正好排得下设定的短语数（见updatePhraseListColumns）
        return size;
    }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override { // 重写绘制函数，用于自定义显示效果
        QStyleOptionViewItem opt(option);
        QVariant foreground = index.data(Qt::ForegroundRole);
        if ((opt.state & QStyle::State_Selected) && foreground.canConvert<QBrush>()) {
            opt.palette.setBrush(QPalette::HighlightedText, foreground.value<QBrush>()); // 选中时仍使用短语项自己的字体颜色
        }
        QStyledItemDelegate::paint(painter, opt, index); // 先调用默认的绘制方法，画好背景和文字
        QString badgeText = index.data(Qt::UserRole + 4).toString(); // 通过数据模型，从当前短语项的Qt::UserRole+4中取出角标字符
        if (!badgeText.isEmpty()) { // 如果角标不为空 //【【【注：想修改角标样式在这里修改】】】
            painter->save(); // 保存画笔状态
            painter->setRenderHint(QPainter::Antialiasing); // 开启抗锯齿
            QFont font = painter->font(); // 获取当前画笔的字体
            font.setPixelSize(10); // 设置字体大小为10像素
            font.setBold(true); // 设置字体为粗体
            painter->setFont(font); // 把设置好的字体应用给画笔
            painter->setBrush(QColor("#0078d4")); // 设置角标背景色
            painter->setPen(Qt::NoPen); // 绘制背景时无边框
            QRect badgeRect; // 定义角标的矩形区域
            if (config["jiaobiao"].toBool() == true) { // 设置角标位置和宽高
                badgeRect = QRect(option.rect.left() + 4, option.rect.top() + 4, 18, 18); // 左上角，宽高为18像素
            } else {
                badgeRect = QRect(option.rect.right() - 18 - 5, option.rect.top() + 5, 18, 18); // 右上角，宽高为18像素
            }
            painter->drawRoundedRect(badgeRect, 5, 5); // 绘制圆角矩形背景
            painter->setPen(Qt::white); // 设置字体颜色为白色
            painter->drawText(badgeRect, Qt::AlignCenter, badgeText); // 绘制数字/字母，水平垂直居中显示
            painter->restore(); // 恢复画笔状态
        }
    }
};

// 为tabBar单独自定义一个继承自QTabBar的子类，同时重写wheelEvent()实现滚动鼠标滚轮可以切换分组
class MyTabBar : public QTabBar {
  public:
    using QTabBar::QTabBar; // 直接继承QTabBar构造函数
  protected:
    void wheelEvent(QWheelEvent *event) override {
        int delta = event->angleDelta().y(); // 获取鼠标滚轮的y方向角度增量。正值为向上滚，负值为向下滚
        if (delta > 0) {
            setCurrentIndex(qMax(0, currentIndex() - 1)); // 设置选中分组为 当前选中分组左边的那个分组
        } else if (delta < 0) {
            setCurrentIndex(qMin(count() - 1, currentIndex() + 1)); // 设置选中分组为 当前选中分组右边的那个分组
        }
        event->accept(); // 标记事件已处理，防止继续传递
    }
};

//====================检查更新====================
// 向GitHub Releases API要最新的一个release，拿它的tag_name和本地版本号比大小。
// 请求用WinHTTP裸写：整个程序就这一处要联网，犯不着为一个GET把Qt Network整个模块拉进来。
// 请求本身是阻塞的，所以扔到后台线程里跑，结果再抛回主线程弹窗——GitHub偶尔要卡好几秒，绝不能卡住界面
static const wchar_t *g_gengxinZhuji = L"api.github.com";
static const wchar_t *g_gengxinLujing = L"/repos/DarkKandaoMaster/QuickSay/releases/latest";
static const QString g_gengxinFabuYe = "https://github.com/DarkKandaoMaster/QuickSay/releases"; // 检查失败时手上没有具体某个release的地址，一律退回到发布列表页
bool g_zhengzaiJianchaGengxin = false; // 一次只允许有一个检查在跑：启动后的自动检查和手动点的“立即检查”可能撞在一起
int g_gengxinShibaiCishu = 0; // 启动后自动检查连续失败了几次。连着失败3次就不再重试

struct GengxinJieguo {
    bool chenggong = false; // 网络请求和解析都成功才算
    QString banben; // 最新版本号，比如“1.8.1”
    QString url; // 那个release的页面地址，更新日志和下载都在这一页上
};

GengxinJieguo qingqiuZuixinBanben() { // 真的发一次HTTPS GET。整段是同步阻塞的，只能在后台线程里调
    GengxinJieguo jieguo;
    HINTERNET huihua = WinHttpOpen(L"QuickSay", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0); // 走系统的自动代理设置，用户挂着代理时也能通
    if (!huihua) return jieguo;
    WinHttpSetTimeouts(huihua, 8000, 8000, 8000, 8000); // 连不上时8秒就放弃，别让10秒后的那次重试排到几十秒之后去
    QByteArray shuju;
    if (HINTERNET lianjie = WinHttpConnect(huihua, g_gengxinZhuji, INTERNET_DEFAULT_HTTPS_PORT, 0)) {
        if (HINTERNET qingqiu = WinHttpOpenRequest(lianjie, L"GET", g_gengxinLujing, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)) {
            const wchar_t *toubu = L"User-Agent: QuickSay\r\nAccept: application/vnd.github+json\r\n"; // GitHub API强制要求带User-Agent，不带直接被403挡回来
            if (WinHttpSendRequest(qingqiu, toubu, -1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(qingqiu, nullptr)) {
                DWORD zhuangtaima = 0, changdu = sizeof(zhuangtaima);
                WinHttpQueryHeaders(qingqiu, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &zhuangtaima, &changdu, WINHTTP_NO_HEADER_INDEX);
                if (zhuangtaima == 200) {
                    DWORD keduo = 0;
                    while (WinHttpQueryDataAvailable(qingqiu, &keduo) && keduo > 0) { // 一块一块读，直到读不出东西为止
                        QByteArray kuai(keduo, 0);
                        DWORD dudao = 0;
                        if (!WinHttpReadData(qingqiu, kuai.data(), keduo, &dudao)) break;
                        shuju.append(kuai.constData(), dudao);
                        if (shuju.size() > 1024 * 1024) break; // release的JSON最多也就几十KB，超过1MB说明拿到的根本不是想要的东西，别一直读下去
                    }
                }
            }
            WinHttpCloseHandle(qingqiu);
        }
        WinHttpCloseHandle(lianjie);
    }
    WinHttpCloseHandle(huihua);

    QJsonDocument wendang = QJsonDocument::fromJson(shuju);
    if (!wendang.isObject()) return jieguo;
    QJsonObject dui = wendang.object();
    // release的标签名不是光秃秃的版本号，历来写成“QuickSay_v1.8.1”这样，所以从里面把第一串数字点分的版本号抠出来
    QRegularExpressionMatch pipei = QRegularExpression("\\d+(?:\\.\\d+){1,3}").match(dui["tag_name"].toString());
    if (!pipei.hasMatch()) return jieguo;
    QString biaoqian = pipei.captured();
    QString url = dui["html_url"].toString();
    jieguo.chenggong = true;
    jieguo.banben = biaoqian;
    jieguo.url = url.startsWith("https://github.com/") ? url : g_gengxinFabuYe; // 只认GitHub自己的地址，免得响应被人掉包后把用户带去别处
    return jieguo;
}

// 更新弹窗：一行说明文字 + 一个“查看更新日志”链接 + 两个按钮。这里一句样式表都不挂，控件全交给Qt原生样式去画，风格和设置窗口一致。
// zuoAnniuWenzi是左边那个按钮的文字：启动时自动弹出来的写“忽略该版本”，手动点“立即检查”弹出来的写“取消”。
// 返回值：2=点了“前往下载”，1=点了左边那个按钮，0=直接把窗口关掉了
int tanchuGengxinChuangkou(const QString &shuoming, const QString &url, const QString &zuoAnniuWenzi) {
    QDialog duihua;
    duihua.setWindowTitle("QuickSay");
    duihua.setWindowIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/软件图标.svg"));
    duihua.setWindowFlags(duihua.windowFlags() & ~Qt::WindowContextHelpButtonHint); // 去掉标题栏上那个没用的问号按钮
    QVBoxLayout *waiceng = new QVBoxLayout(&duihua);
    waiceng->setContentsMargins(16, 16, 16, 14);
    waiceng->setSpacing(10);
    waiceng->addWidget(new QLabel(shuoming));
    QLabel *rizhi = new QLabel(QString("<a href=\"%1\">查看更新日志</a>").arg(url));
    rizhi->setOpenExternalLinks(true); // 点一下用系统默认浏览器打开
    rizhi->setCursor(Qt::PointingHandCursor);
    waiceng->addWidget(rizhi);
    waiceng->addStretch();
    QHBoxLayout *anniuHang = new QHBoxLayout;
    anniuHang->setSpacing(12); // 按钮之间空12像素，和设置窗口底部那三个按钮一样
    anniuHang->addStretch();
    QPushButton *zuoAnniu = new QPushButton(zuoAnniuWenzi, &duihua);
    QPushButton *xiazaiAnniu = new QPushButton("前往下载", &duihua);
    for (QPushButton *anniu : {zuoAnniu, xiazaiAnniu}) {
        anniu->setFixedSize(92, 28); // 按钮尺寸也和设置窗口保持一致
        anniuHang->addWidget(anniu);
    }
    waiceng->addLayout(anniuHang);
    xiazaiAnniu->setDefault(true);
    QObject::connect(zuoAnniu, &QPushButton::clicked, [&duihua]() { duihua.done(1); });
    QObject::connect(xiazaiAnniu, &QPushButton::clicked, [&duihua]() { duihua.done(2); });
    duihua.setFixedSize(300, 148);
    QRect ping = QGuiApplication::primaryScreen()->geometry();
    duihua.move(ping.center() - QPoint(duihua.width() / 2, duihua.height() / 2)); // 没有父窗口，Qt不会帮忙居中，自己摆到屏幕中间
    return duihua.exec();
}

void jianchaGengxin(bool shoudong); // 处理结果时失败还要再排一次检查，两个函数互相调用，这里先声明一下

void chuliGengxinJieguo(bool shoudong, const GengxinJieguo &jieguo) { // 在主线程里处理后台线程拿回来的结果。shoudong=true表示这次是用户点“立即检查”点出来的
    g_zhengzaiJianchaGengxin = false;
    if (shoudong) QApplication::restoreOverrideCursor();
    if (!jieguo.chenggong) {
        if (shoudong) { // 手动检查失败要报出来，同时照样给出更新日志和下载入口，让用户能自己去看一眼
            if (tanchuGengxinChuangkou("检查更新失败，请检查网络连接。", g_gengxinFabuYe, "取消") == 2) QDesktopServices::openUrl(QUrl(g_gengxinFabuYe));
            return;
        }
        if (++g_gengxinShibaiCishu >= 3) return; // 启动后的自动检查一律静默：连着失败3次就彻底放弃，剩下的交给用户自己去点“立即检查”
        QTimer::singleShot(10000, qApp, []() { jianchaGengxin(false); }); // 10秒后再试一次
        return;
    }
    g_gengxinShibaiCishu = 0;
    bool kezhi = false; // 两个版本号里只要有一个不是纯数字点分格式，就没法比大小
    if (compareQuickSayVersions(jieguo.banben, g_quickSayVersion, kezhi) <= 0 || !kezhi) { // 服务器上的版本不比本地新
        if (shoudong) {
            QMessageBox tishi;
            tishi.setWindowTitle("QuickSay");
            tishi.setWindowIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/软件图标.svg"));
            tishi.setIcon(QMessageBox::Information);
            tishi.setText("当前已经是最新版本。  ");
            tishi.setStandardButtons(QMessageBox::Ok);
            tishi.button(QMessageBox::Ok)->setText("确定");
            tishi.exec();
        }
        return;
    }
    if (!shoudong && jieguo.banben == config["hulve_banben"].toString()) return; // 这个版本用户已经说过“忽略”了，启动时就别再拿它烦人
    int xuanle = tanchuGengxinChuangkou(QString("发现新版本 %1").arg(jieguo.banben), jieguo.url, shoudong ? "取消" : "忽略该版本");
    if (xuanle == 2) {
        QDesktopServices::openUrl(QUrl(jieguo.url)); // 更新日志和下载都在同一个release页面上
    } else if (xuanle == 1 && !shoudong) { // 只有“忽略该版本”这个按钮才记忽略，直接把窗口关掉不算——那只是这次不想看
        config["hulve_banben"] = jieguo.banben;
        saveConfig(QCoreApplication::applicationDirPath() + "/config.json");
    }
}

void jianchaGengxin(bool shoudong) { // 检查更新的唯一入口。启动后的自动检查传false（失败静默重试），设置里点“立即检查”传true（失败要报出来）
    if (g_zhengzaiJianchaGengxin) return; // 上一次还没跑完，这次就不重复发请求了
    g_zhengzaiJianchaGengxin = true;
    if (shoudong) QApplication::setOverrideCursor(Qt::WaitCursor); // 手动检查时给个等待光标，不然点完按钮好几秒没动静，用户会以为按钮坏了
    std::thread([shoudong]() {
        GengxinJieguo jieguo = qingqiuZuixinBanben(); // 阻塞的网络请求在这条后台线程里跑
        QMetaObject::invokeMethod(qApp, [shoudong, jieguo]() { chuliGengxinJieguo(shoudong, jieguo); }, Qt::QueuedConnection); // 弹窗只能在主线程里弹，把结果抛回去
    }).detach();
}

//====================设置窗口====================
// 设置窗口的外观照着TrafficMonitor的设置对话框做：顶部四个页签、每页一块独立的滚动区域、底部固定“确定/取消/应用”。
// 这份样式表只挂在设置窗口自己身上，而且每条选择器都以 #shezhiChuangkou 开头——ID选择器的优先级比main()里那份全局QSS高，
// 所以设置窗口的外观和QuickSay其他窗口完全隔离，改哪边都不会影响另一边
static const char *g_shezhiQss = R"(
/*这里只写窗口和页签的底色，别的什么都不写：数字框、复选框、按钮、快捷键框这些一律不加规则，
  Qt就会用Windows原生样式去画它们——外观和TrafficMonitor一致，数字框的上下箭头也是系统自己画的*/
QWidget#shezhiChuangkou{
    background-color:#f0f0f0;                       /*窗口本体：未选中页签右边那片空白、底部按钮那一条都是这个颜色*//*【【【注：想改设置窗口外围底色在这里改】】】*/
}
/*====================页签====================*/
QWidget#shezhiChuangkou QTabWidget::pane{
    background-color:#f9f9f9;                       /*页签下方的配置区域底色*/
    border:1px solid #e5e5e5;                       /*边框颜色是照着TrafficMonitor量出来的：一条很浅的灰线*//*【【【注：想改设置窗口里页签边框的深浅在这里和下面一起改】】】*/
    top:-1px;                                       /*往上挪1像素，和页签底边压在一起，选中页签就和配置区域连成一体了*/
}
QWidget#shezhiChuangkou QTabWidget::tab-bar{
    left:0px;                                       /*页签从左边开始排*/
}
QWidget#shezhiChuangkou QTabBar{
    background-color:transparent;                   /*页签栏本身透明，露出窗口的#f0f0f0*/
    border:none;
}
QWidget#shezhiChuangkou QTabBar::tab{
    background-color:#f0f0f0;                       /*未选中页签：和外围背景同色*/
    border:1px solid #e5e5e5;
    color:#000000;
    height:24px;                                    /*未选中页签高度24像素，和TrafficMonitor一致*//*【【【注：想改页签高度在这里改】】】*/
    padding:0px 8px;                                /*页签左右内边距，宽度随图标和文字自适应*/
    margin-top:2px;                                 /*未选中页签顶部让出2像素；选中后会向上长高，复现TrafficMonitor原生页签的凸起效果*/
    margin-right:-1px;                              /*相邻页签共用一条竖边框，不会出现两条并排的线*/
}
QWidget#shezhiChuangkou QTabBar::tab:hover{
    background-color:#f5f5f5;
}
QWidget#shezhiChuangkou QTabBar::tab:selected{
    background-color:#f9f9f9;                       /*选中页签：和下方配置区域同色*/
    border-bottom-color:#f9f9f9;                    /*底边染成配置区域的颜色，等于把两者之间那条线抹掉*/
    height:26px;                                    /*选中页签比未选中页签高2像素，并占掉上面的留白；页签和里面的图文都会自然往上抬一点*/
    margin-top:0px;
}
/*====================滚动页====================*/
/*只点名滚动内容和视口这两个控件。绝不能写成“QScrollArea下面的所有控件”，那样连滚动条都会被套上规则，
  滚动条一旦匹配到规则就得由样式表自己画，原生外观就没了*/
QWidget#shezhiChuangkou QWidget#shezhiYemian,QWidget#shezhiChuangkou QWidget#shezhiShikou{
    background-color:#f9f9f9;                       /*滚动内容和它的视口都用配置区域的底色；分组框不加规则，露出的就是这个颜色*/
}
/*====================说明文字====================*/
QWidget#shezhiChuangkou QLabel#shezhiShuoming{
    color:#606060;                                  /*说明文字：灰色，和黑色的选项文字区分开*//*【【【注：想改说明文字颜色在这里改】】】*/
}
)";

// 设置窗口页签栏单独接管图标和文字的绘制。Qt默认在两者中间留得比Windows原生页签宽，TrafficMonitor用的是原生CTabCtrl，所以这里把距离固定成5像素
class ShezhiYeqianLan : public QTabBar {
  public:
    using QTabBar::QTabBar;

  protected:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);
        QStylePainter huabi(this);
        for (int i = 0; i < count(); ++i) { // 先画未选中的页签，最后再画选中页签，避免相邻页签盖住选中页签凸出来的边框
            if (i != currentIndex()) huaYeqian(huabi, i);
        }
        if (currentIndex() >= 0) huaYeqian(huabi, currentIndex());
    }

  private:
    void huaYeqian(QStylePainter &huabi, int index) {
        QStyleOptionTab xuanxiang;
        initStyleOption(&xuanxiang, index);
        huabi.drawControl(QStyle::CE_TabBarTabShape, xuanxiang); // 页签底色和边框仍然交给上面的样式表画，只接管里面的图标和文字

        const bool xuanzhong = xuanxiang.state.testFlag(QStyle::State_Selected);
        QRect neirong = xuanxiang.rect;
        if (xuanzhong) neirong.translate(0, -1); // 高度增加2像素只会让居中的图文自然抬高1像素，所以选中时再额外上移1像素，合起来正好抬高2像素
        else neirong.adjust(0, 2, 0, 0); // 未选中页签本身矮2像素，图文中心会比选中页签低2像素
        QIcon::Mode moshi = xuanxiang.state.testFlag(QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled;
        QIcon::State zhuangtai = xuanzhong ? QIcon::On : QIcon::Off;
        QSize tubiaoChicun = xuanxiang.icon.actualSize(iconSize(), moshi, zhuangtai);
        int wenziKuandu = xuanxiang.fontMetrics.horizontalAdvance(xuanxiang.text);
        int juli = !xuanxiang.icon.isNull() && !xuanxiang.text.isEmpty() ? 5 : 0; // 图标和文字只隔5像素，接近TrafficMonitor原生页签的距离
        int zongKuandu = tubiaoChicun.width() + juli + wenziKuandu;
        int zuo = neirong.center().x() - zongKuandu / 2;

        if (!xuanxiang.icon.isNull()) {
            QRect tubiaoKuang(zuo, neirong.center().y() - tubiaoChicun.height() / 2 + 1, tubiaoChicun.width(), tubiaoChicun.height()); // 图标比文字的视觉重心偏高，单独下移1像素，和TrafficMonitor原生页签对齐
            xuanxiang.icon.paint(&huabi, tubiaoKuang, Qt::AlignCenter, moshi, zhuangtai);
            zuo = tubiaoKuang.right() + 1 + juli;
        }
        QRect wenziKuang(zuo, neirong.top(), wenziKuandu, neirong.height());
        style()->drawItemText(&huabi, wenziKuang, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic, xuanxiang.palette,
            xuanxiang.state.testFlag(QStyle::State_Enabled), xuanxiang.text, QPalette::WindowText);
    }
};

// 只有QTabWidget的子类才能替换内部页签栏；设置窗口用上面的紧凑页签栏，QuickSay主窗口的分组栏不受影响
class ShezhiYeqian : public QTabWidget {
  public:
    explicit ShezhiYeqian(QWidget *parent = nullptr) : QTabWidget(parent) {
        setTabBar(new ShezhiYeqianLan(this));
    }
};

// 设置窗口本体。它不是QDialog，而是普通的QWidget：QDialog自带回车=确定、Esc=取消，而这个窗口要求任何快捷键都不能触发确定/取消/应用
class ShezhiChuangkou : public QWidget {
  public:
    explicit ShezhiChuangkou(const QString &configPath) : configPath_(configPath) {}
    std::function<void()> guanbiHuidiao; // 窗口被关掉时的回调：用来丢弃还没应用的修改

  protected:
    void showEvent(QShowEvent *e) override {
        QWidget::showEvent(e);
        if (chicunYiChushihua_) return;
        chicunYiChushihua_ = true; // 下面这套换算只在第一次显示时做一次
        muBiaoWaikuang_ = QSize(config["shezhichuangkou_w"].toInt(), config["shezhichuangkou_h"].toInt()); // 目标外框尺寸只在这里取一次。要是校正时再去读config，就会读到上一次四舍五入后存回去的值，尺寸会一次比一次大
        dingChicun(); // 先按现在能问到的边框尺寸摆一次，窗口一出来就是接近正确的大小，不至于跳一下
        QTimer::singleShot(0, this, [this]() { dingChicun(); }); // 窗口真正贴到屏幕上之后frameGeometry()才准，回到事件循环再校正一次
    }
    QSize waikuangChicun() { // 窗口连标题栏带边框的外框尺寸（逻辑像素）。不用frameGeometry()：它算的是DWM那圈看得见的边，不含Windows 10/11外面那圈透明的拖拽边，会比GetWindowRect小十几个像素
        RECT wai;
        if (!GetWindowRect((HWND)winId(), &wai)) return frameGeometry().size();
        qreal bi = devicePixelRatioF(); // GetWindowRect给的是物理像素，Qt这边一律用逻辑像素
        return QSize(qRound((wai.right - wai.left) / bi), qRound((wai.bottom - wai.top) / bi));
    }
    void dingChicun() { // 把“外框620*576”换算成客户区尺寸：设成最小尺寸，再恢复上次记下的普通状态尺寸
        QSize bianKuang = waikuangChicun() - size(); // 外框比客户区大出来的那一圈
        if (bianKuang.isEmpty()) return; // 窗口还没真正建好，这时问不出边框尺寸，交给下面那次延迟校正
        setMinimumSize(QSize(620, 576) - bianKuang); // 最小外框尺寸620*576，和TrafficMonitor中文设置窗口完全一致
        resize(muBiaoWaikuang_ - bianKuang); // 默认外框尺寸也是620*576
    }
    void resizeEvent(QResizeEvent *e) override {
        QWidget::resizeEvent(e);
        if (g_zhengzaiDaoruChongqi || !chicunYiChushihua_) return; // 导入重启期间不准再往config.json里写东西
        if (isMaximized() || isMinimized() || !isVisible()) return; // 只记忆普通状态尺寸：最大化、最小化时的尺寸不算数
        QSize waikuang = waikuangChicun(); // 记的是外框尺寸，和最小尺寸、默认尺寸用的是同一套口径
        config["shezhichuangkou_w"] = waikuang.width();
        config["shezhichuangkou_h"] = waikuang.height();
        saveConfig(configPath_);
    }
    void closeEvent(QCloseEvent *e) override {
        if (guanbiHuidiao) guanbiHuidiao(); // 标题栏的关闭按钮和“取消”一样：丢弃还没应用的修改
        QWidget::closeEvent(e);
    }

  private:
    QString configPath_;
    bool chicunYiChushihua_ = false;
    QSize muBiaoWaikuang_; // 这次要摆出来的外框尺寸
};

// 鼠标滚轮落在数字框、或者还没展开的下拉框上时，不要改控件的值，改成滚动它所在的那一页
class GunlunLvqi : public QObject {
  public:
    using QObject::QObject;

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() != QEvent::Wheel) return QObject::eventFilter(obj, event);
        QWidget *kongjian = qobject_cast<QWidget *>(obj);
        if (!kongjian) return QObject::eventFilter(obj, event);
        if (QComboBox *xiala = qobject_cast<QComboBox *>(kongjian)) {
            if (xiala->view() && xiala->view()->isVisible()) return QObject::eventFilter(obj, event); // 下拉框已经展开了，滚轮照常用来选项目
        }
        QWidget *zu = kongjian->parentWidget();
        while (zu && !qobject_cast<QScrollArea *>(zu)) zu = zu->parentWidget(); // 一路往上找它所在的滚动页
        if (QScrollArea *quyu = qobject_cast<QScrollArea *>(zu)) QApplication::sendEvent(quyu->viewport(), event); // 把这次滚轮转交给滚动页
        return true; // 不管找没找到滚动页都把事件吃掉，绝不能让滚轮改掉控件的值
    }
};

//====================托盘右键菜单====================
// 托盘右键菜单不用QMenu，改用Win32的TrackPopupMenu：这样菜单是Windows自己画的，外观和TrafficMonitor完全一致，也就天然和QuickSay的全局QSS隔离
HBITMAP caidanTubiaoWeitu(const QString &icoPath, int bian) { // 把.ico读成菜单项能用的32位带alpha位图，MENUITEMINFO::hbmpItem要的就是这个
    QImage tu = QIcon(icoPath).pixmap(QSize(bian, bian), 1.0).toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied); // 菜单位图要的是预乘alpha；这里固定按1倍缩放取图，因为bian已经是系统按DPI算好的物理像素了
    if (tu.isNull()) return nullptr;
    BITMAPINFO xinxi = {};
    xinxi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    xinxi.bmiHeader.biWidth = tu.width();
    xinxi.bmiHeader.biHeight = -tu.height(); // 负数表示自上而下排列，和QImage的行序一致，可以整块拷过去
    xinxi.bmiHeader.biPlanes = 1;
    xinxi.bmiHeader.biBitCount = 32;
    xinxi.bmiHeader.biCompression = BI_RGB;
    void *xiangsu = nullptr;
    HBITMAP weitu = CreateDIBSection(nullptr, &xinxi, DIB_RGB_COLORS, &xiangsu, nullptr, 0);
    if (!weitu) return nullptr;
    memcpy(xiangsu, tu.constBits(), static_cast<size_t>(tu.sizeInBytes())); // ARGB32每行字节数正好是宽度*4，和DIB的4字节对齐一致
    return weitu;
}

HWND tuopanCaidanZhuChuangkou() { // 原生菜单需要一个宿主窗口：TrackPopupMenu靠它收消息，弹出前还要把它设成前台窗口，菜单才会在点别处时自动消失
    static HWND chuang = nullptr;
    if (chuang) return chuang;
    static const wchar_t *leiming = L"QuickSayTrayMenuHost";
    WNDCLASSW lei = {};
    lei.lpfnWndProc = DefWindowProcW;
    lei.hInstance = GetModuleHandleW(nullptr);
    lei.lpszClassName = leiming;
    RegisterClassW(&lei);
    chuang = CreateWindowExW(WS_EX_TOOLWINDOW, leiming, L"", WS_POPUP, -32000, -32000, 1, 1, nullptr, nullptr, lei.hInstance, nullptr); // 1*1、扔在屏幕外的工具窗口：用户看不见，也不会出现在任务栏和Alt+Tab里
    ShowWindow(chuang, SW_SHOWNA); // 必须真的显示出来，否则SetForegroundWindow对它无效，菜单就会点了别处也不消失
    return chuang;
}

int tanchuTuopanCaidan(const QString &tubiaoMulu) { // 弹出托盘右键菜单，返回用户点了哪一项：1=设置，2=退出，0=什么都没点
    HWND zhuChuang = tuopanCaidanZhuChuangkou();
    HMENU caidan = CreatePopupMenu();
    int bian = GetSystemMetrics(SM_CXSMICON); // 菜单图标尺寸跟着系统DPI走，100%缩放下就是16*16，和TrafficMonitor一致
    HBITMAP tuSheZhi = caidanTubiaoWeitu(tubiaoMulu + "/setting.ico", bian);
    HBITMAP tuTuiChu = caidanTubiaoWeitu(tubiaoMulu + "/exit.ico", bian);
    QString shezhiWen = "设置", tuichuWen = "退出";
    AppendMenuW(caidan, MF_STRING, 1, reinterpret_cast<const wchar_t *>(shezhiWen.utf16()));
    AppendMenuW(caidan, MF_STRING, 2, reinterpret_cast<const wchar_t *>(tuichuWen.utf16()));
    MENUITEMINFOW xiang = {};
    xiang.cbSize = sizeof(xiang);
    xiang.fMask = MIIM_BITMAP;
    xiang.hbmpItem = tuSheZhi;
    SetMenuItemInfoW(caidan, 1, FALSE, &xiang);
    xiang.hbmpItem = tuTuiChu;
    SetMenuItemInfoW(caidan, 2, FALSE, &xiang);
    POINT weizhi;
    GetCursorPos(&weizhi);
    SetForegroundWindow(zhuChuang); // MSDN明确要求：弹菜单前把宿主窗口设成前台窗口，否则点到菜单外面菜单不会消失
    g_yuanshengCaidanZhankai = true;
    int xuanle = TrackPopupMenu(caidan, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, weizhi.x, weizhi.y, 0, zhuChuang, nullptr);
    g_yuanshengCaidanZhankai = false;
    PostMessageW(zhuChuang, WM_NULL, 0, 0); // MSDN明确要求：弹完补一条空消息，否则下次点托盘时菜单可能弹不出来
    DestroyMenu(caidan);
    if (tuSheZhi) DeleteObject(tuSheZhi);
    if (tuTuiChu) DeleteObject(tuTuiChu);
    return xuanle;
}

// 新建一个设置页：外层是独立的滚动区域（每一页各自记住自己的滚动位置），里面是一个竖着排分组框的内容控件
QScrollArea *jianShezhiYemian(QVBoxLayout *&neirongLayout) {
    QScrollArea *quyu = new QScrollArea;
    quyu->setWidgetResizable(true); // 让内容控件随滚动区域宽度自适应
    quyu->setFrameShape(QFrame::NoFrame);
    quyu->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 只要竖直滚动条
    quyu->viewport()->setObjectName("shezhiShikou"); // 给视口起个名字，样式表才点得到它，好把它染成配置区域的底色
    QWidget *neirong = new QWidget;
    neirong->setObjectName("shezhiYemian"); // 样式表靠这个ID给滚动内容上#f9f9f9的底色
    neirongLayout = new QVBoxLayout(neirong);
    neirongLayout->setContentsMargins(10, 10, 10, 10);
    neirongLayout->setSpacing(10);
    quyu->setWidget(neirong);
    return quyu;
}

// 往设置页里加一个分组框，里面用网格布局排选项：第0列放说明文字，第1列放控件，第2列吸收多余宽度好让控件都靠左
QGridLayout *jianFenzukuang(const QString &biaoti, QVBoxLayout *yemianLayout) {
    QGroupBox *kuang = new QGroupBox(biaoti);
    QGridLayout *gezi = new QGridLayout(kuang);
    gezi->setContentsMargins(10, 10, 10, 10);
    gezi->setHorizontalSpacing(8);
    gezi->setVerticalSpacing(8);
    gezi->setColumnStretch(2, 1);
    yemianLayout->addWidget(kuang);
    return gezi;
}

int main(int argc, char *argv[]) {
    // 提权实例的入口：“开机自启动”和“以管理员权限启动”同时勾上时，QuickSay会用runas把自己再启动一份，那一份就带着这个参数
    // 必须抢在下面的单实例检测之前处理完就退出：它只负责建计划任务，不是真的要再开一个QuickSay，不能被单实例检测拦下来，也不用创建任何窗口
    bool houtaiQidong = false; // 是不是开机自启起来的（注册表Run项和计划任务都会给启动参数带上--autostart）
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--create-admin-task") return createAdminRenwu() ? 0 : 1; // 退出码0表示任务建好了，非0表示没建成。等着它退出的那个非提权实例靠这个退出码判断成没成
        if (QString(argv[i]) == "--autostart") houtaiQidong = true;
    }
    // 跨完整性级别的单实例检测。必须抢在QApplication之前：后启动的那个实例要在什么Qt对象都还没创建的时候就退出
    if (tongzhiYiyouShili()) return 0;
    // “以管理员权限启动”：手动启动、开关开着、而自己又没有管理员权限，就弹UAC把自己重新启动一份提权的
    // 必须放在单实例检测之后：QuickSay已经在跑的时候双击exe只是想把窗口叫出来，不该再弹一次UAC。也必须放在QApplication之前，别白白建一堆马上就要丢掉的Qt对象
    if (duGuanliyuanKaiguan() && !isProcessElevated() && !houtaiQidong) {
        shifangDanshili(); // 先让出互斥体，否则提权起来的那份会被单实例检测拦下，结果谁也没起来
        if (tiquanChongqiZishen()) return 0; // 提权的那份已经起来了，这一份的活干完了
        tongzhiYiyouShili(); // 用户在UAC弹窗上点了“否”：把互斥体重新抢回来，以普通权限接着跑
    }
    QApplication a(argc, argv);
    // QFontDatabase::addApplicationFont(QCoreApplication::applicationDirPath()+"/fonts/SourceHanSansSC-Regular-2.otf");//程序启动后注册 /fonts/SourceHanSansSC-Regular-2.otf
    // 当用户又启动了一次程序时，把已经在跑的这个实例的主窗口显示出来
    QWinEventNotifier danshiliTongzhi(g_danshiliEvent); // 监听那个命名事件：又有人启动了一次QuickSay时，后启动的那个实例会SetEvent然后自己退出，这边负责把主窗口显示出来
    QObject::connect(&danshiliTongzhi, &QWinEventNotifier::activated,
        []() {
            ResetEvent(g_danshiliEvent); // 事件是手动重置的，不重置就会一直触发
            if (pchuangkou) {
                pchuangkou->move(config["chuangkou_x"].toInt(), config["chuangkou_y"].toInt()); // 把chuangkou移动到记录的位置
                xianshi(*pchuangkou);
            }
        });

    a.setQuitOnLastWindowClosed(false); // 这里填false的话就是关闭窗口后让程序隐藏到托盘，继续在后台运行。此时如果不在托盘的右键菜单添加一个退出键，你就只能在任务管理器里关闭该程序了
    QString configPath = QCoreApplication::applicationDirPath() + "/config.json"; // 定义config.json文件路径 //QCoreApplication::applicationDirPath()返回的是可执行文件的目录路径（不包含文件名本身）
    loadConfig(configPath); // 程序启动时调用loadConfig函数
    saveConfig(configPath); // 然后调用saveConfig函数，兼容旧版本
    // 启动时把系统里的自启状态调成和config一致：纠正程序挪过位置后过期的路径，并保证注册表Run项和计划任务互斥（两个同时留着会开机启动两个实例）
    applyZiqidong(true); // 不打扰模式：这里绝不弹UAC。要提权的话，手动启动的那份早在main()开头就提过了；开机自启起来的那份更不能弹

    QWidget chuangkou;
    pchuangkou = &chuangkou; // 创建主窗口时把地址赋值给全局指针，用于当用户启动程序时，如果已经有实例正在运行，那么显示正在运行的那个实例的主窗口
    a.installNativeEventFilter(new NoActivateNativeFilter());
    HWND quickSayMainHwnd = (HWND)chuangkou.winId(); // WTS会话通知必须注册到一个真实的Win32窗口句柄；即使主窗口开机时不显示，这个句柄也一直存在
    bool wtsSessionRegistered = WTSRegisterSessionNotification(quickSayMainHwnd, NOTIFY_FOR_THIS_SESSION) != FALSE; // 只监听当前用户会话的锁屏、注销和断开，不接收其他登录用户的会话变化
    QObject::connect(&a, &QCoreApplication::aboutToQuit, [quickSayMainHwnd, wtsSessionRegistered]() {
        stopQuickSayOutput(); // 退出事件循环前也先停掉成员计时器并恢复输出状态，避免QApplication销毁子对象时仍有等待步骤
        if (wtsSessionRegistered) WTSUnRegisterSessionNotification(quickSayMainHwnd);
    });
    chuangkou.setStyleSheet(g_quanjuQss); // QuickSay自己那套样式表现在是一个窗口一个窗口挂上去的，见g_quanjuQss上面的注释
    chuangkou.setWindowTitle("QuickSay");
    chuangkou.setWindowIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/软件图标.svg"));

    // 短语列表
    QListWidget liebiao(&chuangkou);
    g_liebiao = &liebiao;
    QString dataPath = QCoreApplication::applicationDirPath() + "/data.json"; // 定义data.json文件路径
    loadListFromJson(liebiao, dataPath); // 程序启动时调用loadListFromJson函数
    saveListToJson(liebiao, dataPath); // 然后调用saveListToJson函数，兼容旧版本
    liebiao.setWordWrap(true); // 开启列表的自动换行功能，这样当文本超出宽度时会自动折行
    liebiao.setUniformItemSizes(true); // 告诉视图所有短语项的高度都相同，这样才能正常自动折行。同时这会跳过逐个计算短语项的高度的过程，提高程序性能
    liebiao.setItemDelegate(new BadgeDelegate(&liebiao)); // 给列表设置BadgeDelegate委托类对象，实现绘制角标
    liebiao.setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); // 开启像素级滚动
    liebiao.verticalScrollBar()->setSingleStep(config["gundong"].toInt()); // 设置滚动条滚动速度
    liebiao.setMouseTracking(true);
    liebiao.viewport()->setMouseTracking(true);
    liebiao.viewport()->installEventFilter(new PhraseItemToolTipFilter(&liebiao, &liebiao));
    QVector<QHotkey *> itemHotkeys; // 创建一个动态数组，保存所有短语项对应的QHotkey *对象，并且保证它们的下标（0~n-1）与短语项的行号一一对应。因此如果对应短语项的快捷键字符串为空字符串，那么对应QHotkey *对象为空指针
    rebuildItemHotkeys(liebiao, itemHotkeys, &a); // 程序启动时为liebiao中的短语项注册快捷键
    // 实现liebiao选项拖动排序
    liebiao.setDragEnabled(true); // 允许短语项被拖动
    liebiao.setAcceptDrops(true); // 允许列表接收拖动放下的项
    liebiao.setDropIndicatorShown(true); // 显示拖动放下时的指示器
    liebiao.setDefaultDropAction(Qt::MoveAction); // 设置默认拖放行为为移动，而不是复制
    liebiao.setDragDropMode(QAbstractItemView::InternalMove); // 设置内部移动模式，用户只能在列表内部拖动
    // 当按下liebiao中的某个选项时，就调用shuchu函数
    QObject::connect(&liebiao, &QListWidget::itemClicked,
        [&](QListWidgetItem *item) {
            if (g_searchMode) {
                leaveSearchMode();
            }
            shuchu(item, &chuangkou);
        });
    // 分组栏
    MyTabBar tabBar(&chuangkou);
    g_tabBar = &tabBar;
    QString tabPath = QCoreApplication::applicationDirPath() + "/tab.json"; // 定义tab.json文件路径
    loadTabFromJson(tabBar, tabPath); // 程序启动时调用loadTabFromJson函数
    saveTabToJson(tabBar, tabPath); // 然后调用saveTabToJson函数，兼容旧版本
    tabBar.setExpanding(false); // 始终根据分组内容长度来确定分组宽度，而不是在短语较少时根据分组栏宽度强制拉伸分组宽度
    tabBar.setUsesScrollButtons(false); // 不显示左右按钮
    tabBar.setDrawBase(false); // 不绘制基座
    tabBar.setMovable(true); // 允许通过拖动改变分组顺序
    liebiao.viewport()->installEventFilter(new PhraseListResizeFilter(&liebiao, &tabBar, &liebiao)); // 列表可视区宽度一变就重算网格每格宽度，保证一行正好放得下设定的短语数。装在这里是因为它要用到tabBar，得等tabBar建好
    // 搜索框
    QLineEdit search(&chuangkou);
    g_search = &search;
    search.setClearButtonEnabled(true); // 开启一键清除按钮（输入框右边的小叉叉）
    // 搜索框文字发生变化触发
    QObject::connect(&search, &QLineEdit::textChanged,
        [&](const QString &text) {
            filterListByTab(liebiao, tabBar.tabText(tabBar.currentIndex()), text); // 根据当前选中分组和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
            for (int i = 0; i < liebiao.count(); i++) { // 遍历列表中的所有项
                if (!liebiao.item(i)->isHidden()) { // 如果该短语项没有隐藏
                    liebiao.setCurrentItem(liebiao.item(i)); // 设置该短语项为当前选中的短语项
                    break;
                }
            }
        });
    // 用户拖动短语项完成后触发
    QObject::connect(liebiao.model(), &QAbstractItemModel::rowsMoved,
        [&]() {
            saveListToJson(liebiao, dataPath);
            rebuildItemHotkeys(liebiao, itemHotkeys, &a); // 拖动完成后为liebiao中的短语项注册快捷键
            filterListByTab(liebiao, tabBar.tabText(tabBar.currentIndex()), search.text()); // 拖动完成后根据当前选中分组和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
        });
    // 用户拖动分组完成后触发
    QObject::connect(&tabBar, &QTabBar::tabMoved,
        [&]() {
            saveTabToJson(tabBar, tabPath);
        });

    filterListByTab(liebiao, tabBar.tabText(tabBar.currentIndex()), search.text()); // 程序启动时根据当前选中分组和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
    applyPhraseListLayout(liebiao, tabItemHeight(tabBar, tabBar.currentIndex()), tabColumns(tabBar, tabBar.currentIndex())); // 程序启动时根据当前选中分组取出并应用短语项高度、内边距和每行短语数（注意程序启动时默认选中的是第一个分组。这是因为loadTabFromJson是在空分组栏里一个个新建分组的，所以Qt会选中第一个分组）//要放在filterListByTab后面，这样量行高时量到的才是当前分组里看得见的短语项
    for (int i = 0; i < liebiao.count(); i++) { // 然后选中没有隐藏的第一个短语项
        if (!liebiao.item(i)->isHidden()) { // 如果该短语项没有隐藏
            liebiao.setCurrentItem(liebiao.item(i)); // 设置该短语项为当前选中的短语项
            break;
        }
    }
    // 如果当前选中分组发生改变，那么根据当前选中分组过滤短语项，然后选中没有隐藏的第一个短语项
    QObject::connect(&tabBar, &QTabBar::currentChanged,
        [&](int index) { // index是新选中分组的索引
            if (index >= 0 && index < tabBar.count()) { // 如果索引有效
                filterListByTab(liebiao, tabBar.tabText(index), search.text()); // 根据当前选中分组和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
                applyPhraseListLayout(liebiao, tabItemHeight(tabBar, index), tabColumns(tabBar, index)); // 根据当前选中分组取出并应用短语项高度、内边距和每行短语数。要放在filterListByTab后面，这样量行高时量到的才是新分组里看得见的短语项
                liebiao.setCurrentItem(nullptr); // 先清掉选中项，这样切到空分组时就不会还选着上一个分组里那个已经隐藏了的短语
                for (int i = 0; i < liebiao.count(); i++) { // 遍历列表中的所有项
                    if (!liebiao.item(i)->isHidden()) { // 如果该短语项没有隐藏
                        liebiao.setCurrentItem(liebiao.item(i)); // 设置该短语项为当前选中的短语项
                        break;
                    }
                }
            }
        });
    tabBar.setContextMenuPolicy(Qt::CustomContextMenu); // 为tabBar设置自定义右键菜单
    // 当右键tabBar时，执行lambda表达式
    QObject::connect(&tabBar, &QWidget::customContextMenuRequested,
        [&](const QPoint &pos) {
            int index = tabBar.tabAt(pos); // 根据点击位置，返回该位置处分组的索引（如果点到空白区域则返回-1）
            if (index < 0) { // 如果点到空白区域，那么弹出菜单，上面有新建分组一个选项 //虽然它返回的是-1，但因为C++标准库很多函数“未找到”时返回的都是负数，而不仅仅是-1，所以这里还是填<0更让我放心一点
                QMenu menu2;
                menu2.setStyleSheet(g_quanjuQss); // 这个菜单没有父对象，得自己把样式表挂上
                QAction tianjia("新建分组", &menu2);
                menu2.addAction(&tianjia);
                QAction *selectedAction = menu2.exec(tabBar.mapToGlobal(pos)); // 在鼠标点击的位置弹出菜单，等待用户选择一个QAction
                if (selectedAction == &tianjia) { // 如果用户选了“新建分组”
                    QString tabName = "";
                    int itemHeight = config["default_item_height"].toInt(); // 取出config里的默认短语项高度，用作弹窗输入框里的默认值
                    int columns = 1; // 新建分组默认每行1个短语，用作弹窗输入框里的默认值
                    bool ok = showTabDialog(chuangkou, "新建分组", tabName, itemHeight, columns); // 调用我们自定义的showTabDialog函数，于是tabName、itemHeight、columns里面存储的就是用户输入的数据
                    if (ok && !tabName.isEmpty()) { // 如果用户选了“确认”并且输入的分组名称不为空
                        if (!isTabNameDuplicate(tabBar, tabName)) { // 如果用户输入的分组名称不重复
                            tabBar.addTab(tabName); // 在分组栏末尾新建分组
                            setTabData(tabBar, tabBar.count() - 1, itemHeight, columns); // 把用户输入的短语项高度和每行短语数存到该分组的tabData中。必须赶在setCurrentIndex前面，因为分组切换的槽函数要读它
                            tabBar.setCurrentIndex(tabBar.count() - 1); // 设置选中分组为该分组
                            saveTabToJson(tabBar, tabPath);
                        } else { // 如果用户输入的分组名称重复
                            QMessageBox::warning(&chuangkou, "分组名重复", "该分组名已存在，请使用其他名称  ");
                            return; // 结束当前这个槽函数的执行
                        }
                    }
                }
            } else { // 如果点到某个分组，那么弹出菜单，上面有在当前分组后新建分组、修改分组、删除分组三个选项
                QMenu menu2;
                menu2.setStyleSheet(g_quanjuQss); // 这个菜单没有父对象，得自己把样式表挂上
                QAction tianjia("在当前分组后新建分组", &menu2);
                menu2.addAction(&tianjia);
                QAction xiugai("修改分组", &menu2);
                menu2.addAction(&xiugai);
                QAction shanchu("删除分组", &menu2);
                menu2.addAction(&shanchu);
                QAction *selectedAction = menu2.exec(tabBar.mapToGlobal(pos)); // 在鼠标点击的位置弹出菜单，等待用户选择一个QAction
                if (selectedAction == &tianjia) { // 如果用户选了“在当前分组后新建分组”
                    QString tabName = "";
                    int itemHeight = config["default_item_height"].toInt(); // 取出config里的默认短语项高度，用作弹窗输入框里的默认值
                    int columns = 1; // 新建分组默认每行1个短语，用作弹窗输入框里的默认值
                    bool ok = showTabDialog(chuangkou, "新建分组", tabName, itemHeight, columns); // 调用我们自定义的showTabDialog函数，于是tabName、itemHeight、columns里面存储的就是用户输入的数据
                    if (ok && !tabName.isEmpty()) { // 如果用户选了“确认”并且输入的分组名称不为空
                        if (!isTabNameDuplicate(tabBar, tabName)) { // 如果用户输入的分组名称不重复
                            tabBar.insertTab(index + 1, tabName); // 在当前分组后新建分组
                            setTabData(tabBar, index + 1, itemHeight, columns); // 把用户输入的短语项高度和每行短语数存到该分组的tabData中。必须赶在setCurrentIndex前面，因为分组切换的槽函数要读它
                            tabBar.setCurrentIndex(index + 1); // 设置选中分组为该分组
                            saveTabToJson(tabBar, tabPath);
                        } else { // 如果用户输入的分组名称重复
                            QMessageBox::warning(&chuangkou, "分组名重复", "该分组名已存在，请使用其他名称  ");
                            return; // 结束当前这个槽函数的执行
                        }
                    }
                } else if (selectedAction == &xiugai) { // 如果用户选了“修改分组”
                    QString oldName = tabBar.tabText(index); // 记录修改前的分组名称
                    QString newName = oldName; // 这个newName将会用作弹窗输入框里的默认文本
                    int itemHeight = tabItemHeight(tabBar, index); // 取出当前分组的短语项高度，用作弹窗输入框里的默认值
                    int columns = tabColumns(tabBar, index); // 取出当前分组的每行短语数，用作弹窗输入框里的默认值
                    bool ok = showTabDialog(chuangkou, "修改分组", newName, itemHeight, columns); // 调用我们自定义的showTabDialog函数，于是newName、itemHeight、columns里面存储的就是用户输入的数据
                    if (ok && !newName.isEmpty()) { // 如果用户选了“确认”并且输入的分组名称不为空
                        if (!isTabNameDuplicate(tabBar, newName) || newName == oldName) { // 如果用户输入的分组名称不重复，或者和oldName一样
                            tabBar.setTabText(index, newName); // 设置索引为index处的分组名称
                            setTabData(tabBar, index, itemHeight, columns); // 把用户输入的短语项高度和每行短语数存到该分组的tabData中
                            if (index == tabBar.currentIndex()) { // 如果修改的分组是当前正在显示的分组
                                applyPhraseListLayout(liebiao, itemHeight, columns); // 那么立刻应用短语项高度、内边距和每行短语数
                            }
                            for (int i = 0; i < liebiao.count(); i++) { // 遍历列表中的所有项
                                if (liebiao.item(i)->data(Qt::UserRole + 2).toString() == oldName) { // 如果该短语项的分组名称等于修改前的分组名称，那么把该短语项的分组名称改为修改后的分组名称
                                    liebiao.item(i)->setData(Qt::UserRole + 2, newName); // 把新分组名称存到当前项的Qt::UserRole+2
                                }
                            }
                            saveTabToJson(tabBar, tabPath);
                            saveListToJson(liebiao, dataPath);
                            filterListByTab(liebiao, tabBar.tabText(tabBar.currentIndex()), search.text()); // 修改后根据当前选中分组和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
                        } else { // 如果用户给出的分组名称重复
                            QMessageBox::warning(&chuangkou, "分组名重复", "该分组名已存在，请使用其他名称  ");
                            return; // 结束当前这个槽函数的执行
                        }
                    }
                } else if (selectedAction == &shanchu) { // 如果用户选了“删除分组”
                    if (tabBar.count() <= 1) {
                        QMessageBox::warning(&chuangkou, "提示", "至少需要保留一个分组  ");
                        return;
                    }
                    QString deletedTabName = tabBar.tabText(index); // 记录待删除的分组名称
                    int ret = QMessageBox::question(&chuangkou, "确认删除", "确定要删除分组吗？该分组下所有短语也会被删除  "); // 弹出询问弹窗，并获取用户点击的选项
                    if (ret == QMessageBox::Yes) {
                        tabBar.removeTab(index); // 删除索引为index处的分组
                        for (int i = liebiao.count() - 1; i >= 0; i--) { // 倒序遍历列表中的所有项，避免删除时索引错乱
                            if (liebiao.item(i)->data(Qt::UserRole + 2).toString() == deletedTabName) { // 如果该短语项的分组名称等于待删除的分组名称，那么把该短语项删除
                                delete liebiao.item(i);
                            }
                        }
                        saveTabToJson(tabBar, tabPath);
                        saveListToJson(liebiao, dataPath);
                        rebuildItemHotkeys(liebiao, itemHotkeys, &a); // 删除后为liebiao中的短语项注册快捷键
                        filterListByTab(liebiao, tabBar.tabText(tabBar.currentIndex()), search.text()); // 删除后根据当前选中分组和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
                    }
                }
            }
        });

    // 创建设置窗口。它是非模态的普通窗口：打开着也能接着用主窗口和其他窗口；重复打开只是把它拉到前台，不会重置还没应用的修改
    ShezhiChuangkou shezhichuangkou(configPath);
    g_shezhichuangkou = &shezhichuangkou;
    shezhichuangkou.setObjectName("shezhiChuangkou"); // 设置窗口专用样式表全靠这个ID选择器压过全局QSS
    shezhichuangkou.setStyleSheet(g_shezhiQss);
    shezhichuangkou.setWindowTitle("QuickSay-设置");
    shezhichuangkou.setWindowIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/软件图标.svg"));

    std::function<void()> yingyongZhuchuangkouDaxiao; // 把主窗口大小设置真正落到窗口上的函数。实现放在main()最后面：adjustAllWindows需要的那一堆控件到那时候才全都创建好

    QVBoxLayout *shezhiWaiceng = new QVBoxLayout(&shezhichuangkou); // 外层布局：上面是页签，下面是固定的三个按钮
    shezhiWaiceng->setContentsMargins(13, 14, 13, 14); // 四周留白：量TrafficMonitor量出来的，左右13、上下14
    shezhiWaiceng->setSpacing(7); // 页签和底部按钮之间的间距
    QTabWidget *shezhiYeqian = new ShezhiYeqian(&shezhichuangkou); // 顶部页签用设置窗口专属的页签栏，图标和文字会像TrafficMonitor一样靠近
    shezhiYeqian->setIconSize(QSize(16, 16)); // 页签图标16*16，和TrafficMonitor一致
    shezhiWaiceng->addWidget(shezhiYeqian, 1);

    GunlunLvqi *gunlunLvqi = new GunlunLvqi(&shezhichuangkou); // 滚轮落在数字框、下拉框上时改成滚动整页，装在下面每一个数字框上
    QString tubiaoMulu = QCoreApplication::applicationDirPath() + "/icons"; // 页签图标和托盘菜单图标都放在这里

    // 新建一个数字输入框：按位数固定宽度，和TrafficMonitor里那些窄输入框一样紧凑
    auto jianShuziKuang = [&](int zuixiao, int zuida, int kuandu) {
        QSpinBox *shuzi = new QSpinBox;
        shuzi->setRange(zuixiao, zuida);
        shuzi->setFixedSize(kuandu, 24); // 高度24像素：Qt默认的数字框比TrafficMonitor的矮4像素，这里按量出来的值补齐
        shuzi->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        shuzi->installEventFilter(gunlunLvqi);
        return shuzi;
    };
    // 往分组框里加一行“说明文字 + 控件”
    auto jiaHang = [](QGridLayout *gezi, const QString &wenzi, QWidget *kongjian) {
        int hang = gezi->rowCount();
        gezi->addWidget(new QLabel(wenzi), hang, 0);
        gezi->addWidget(kongjian, hang, 1);
    };
    // 往分组框里加一行复选框。复选框自带文字，横跨整行
    auto jiaGouxuan = [](QGridLayout *gezi, QCheckBox *gouxuan) {
        gezi->addWidget(gouxuan, gezi->rowCount(), 0, 1, 3);
    };
    // 往分组框里加一行灰色说明文字，横跨整行
    auto jiaShuoming = [](QGridLayout *gezi, const QString &wenzi) {
        QLabel *biaoqian = new QLabel(wenzi);
        biaoqian->setObjectName("shezhiShuoming"); // 样式表靠这个ID把它染成灰色
        biaoqian->setWordWrap(true);
        gezi->addWidget(biaoqian, gezi->rowCount(), 0, 1, 3);
    };

    //--------------------页签1：主窗口设置--------------------
    QVBoxLayout *yemian1Layout = nullptr;
    QScrollArea *yemian1 = jianShezhiYemian(yemian1Layout);
    shezhiYeqian->addTab(yemian1, QIcon(tubiaoMulu + "/item.ico"), "主窗口设置");

    QGridLayout *zu_chuangkou = jianFenzukuang("窗口", yemian1Layout);
    QSpinBox *widthSpin = jianShuziKuang(250, 2000, 70); // 主窗口宽度
    jiaHang(zu_chuangkou, "主窗口宽度（像素）", widthSpin);
    QSpinBox *heightSpin = jianShuziKuang(250, 2000, 70); // 主窗口高度
    jiaHang(zu_chuangkou, "主窗口高度（像素）", heightSpin);
    QCheckBox *zhidingCheck = new QCheckBox("主窗口始终置顶");
    jiaGouxuan(zu_chuangkou, zhidingCheck);
    if (config["zhiding"].toBool() == true) {
        chuangkou.setWindowFlags(chuangkou.windowFlags() | Qt::WindowStaysOnTopHint); // 在不改变其他窗口属性的前提下，给主窗口添加始终置顶属性
    }

    QGridLayout *zu_duanyuxiang = jianFenzukuang("短语项", yemian1Layout);
    QSpinBox *itemHeightSpin = jianShuziKuang(10, 100, 59); // 默认短语项高度【【【【【
    jiaHang(zu_duanyuxiang, "默认短语项高度（像素）", itemHeightSpin);
    jiaShuoming(zu_duanyuxiang, "只影响新建分组时的短语项高度。要改某个分组的短语项高度，请在主窗口右键修改该分组。");
    QSpinBox *itemPaddingHorizontalSpin = jianShuziKuang(0, 100, 59); // 短语项左右内边距
    jiaHang(zu_duanyuxiang, "短语项左右内边距（像素）", itemPaddingHorizontalSpin);
    QSpinBox *itemPaddingVerticalSpin = jianShuziKuang(0, 100, 59); // 短语项上下内边距
    jiaHang(zu_duanyuxiang, "短语项上下内边距（像素）", itemPaddingVerticalSpin);
    QCheckBox *jiaobiaoCheck = new QCheckBox("角标显示在短语项左上角（不勾选则显示在右上角）");
    jiaGouxuan(zu_duanyuxiang, jiaobiaoCheck);
    jiaShuoming(zu_duanyuxiang, "角标位置改完后，鼠标移到主窗口上才会生效。");

    QGridLayout *zu_gundong = jianFenzukuang("滚动", yemian1Layout);
    QSpinBox *gundongSpin = jianShuziKuang(1, 100, 59); // 滚动条滚动速度
    jiaHang(zu_gundong, "滚动条滚动速度", gundongSpin);
    yemian1Layout->addStretch(); // 让分组框都往上挤，别被拉长

    //--------------------页签2：输入设置--------------------
    QVBoxLayout *yemian2Layout = nullptr;
    QScrollArea *yemian2 = jianShezhiYemian(yemian2Layout);
    shezhiYeqian->addTab(yemian2, QIcon(tubiaoMulu + "/taskbar_window.ico"), "输入设置");

    QGridLayout *zu_gaojishuru = jianFenzukuang("高级输入", yemian2Layout);
    QSpinBox *delaySpin = jianShuziKuang(0, 2000, 70); // 高级输入间隔
    jiaHang(zu_gaojishuru, "高级输入间隔（毫秒）", delaySpin);
    jiaShuoming(zu_gaojishuru, "高级输入里每两个动作之间等待的时间。目标程序反应慢时可以调大一些。");

    QGridLayout *zu_jianpan = jianFenzukuang("键盘操作", yemian2Layout);
    QCheckBox *badgeKeyCheck = new QCheckBox("钉住窗口时，按下短语项对应角标输入短语");
    jiaGouxuan(zu_jianpan, badgeKeyCheck);
    QCheckBox *enterKeyCheck = new QCheckBox("钉住窗口时，按下回车键输入短语");
    jiaGouxuan(zu_jianpan, enterKeyCheck);
    yemian2Layout->addStretch();

    //--------------------页签3：常规设置--------------------
    QVBoxLayout *yemian3Layout = nullptr;
    QScrollArea *yemian3 = jianShezhiYemian(yemian3Layout);
    shezhiYeqian->addTab(yemian3, QIcon(tubiaoMulu + "/setting.ico"), "常规设置");

    QGridLayout *zu_kuaijiejian = jianFenzukuang("快捷键", yemian3Layout);
    QKeySequenceEdit *hotkeyEdit = new QKeySequenceEdit(QKeySequence(config["hotkey"].toString())); // 快捷键输入框，里面一开始就存放着config里的快捷键
    hotkeyEdit->setFixedSize(170, 24); // 和数字框一样高
    jiaHang(zu_kuaijiejian, "全局快捷键（呼出主窗口）", hotkeyEdit);
    QHotkey *hotkey = new QHotkey(QKeySequence(config["hotkey"].toString()), true, &a); // 定义一个QHotkey *对象，设置快捷键为config里的快捷键，全局可用。此时就成功注册快捷键了，也就是说按下快捷键会发出信号 //这句代码里已经把a作为父对象传给了hotkey，a会自动管理其内存，不需要手动释放内存
    // 设置按下全局快捷键后会怎样
    QObject::connect(hotkey, &QHotkey::activated,
        [&]() {
            // 按下呼出快捷键时的切换逻辑：
            // 窗口没显示 → 显示并拉到最前；
            // 窗口已显示且开启了“主窗口始终置顶”（此时必然在最前） → 关闭窗口到托盘（相当于按下Esc）；
            // 窗口已显示且没开启“主窗口始终置顶”：被其他窗口盖住 → 拉到最前；已在最前 → 关闭窗口到托盘（相当于按下Esc）。
            if (chuangkou.isVisible() && (config["zhiding"].toBool() || isWindowFrontmost(chuangkou))) { // 窗口已显示，且开启了始终置顶（必然在最前）或没开启但已在最前 → 关闭窗口到托盘（相当于按下Esc）
                if (QWidget *popup = QApplication::activePopupWidget()) popup->close(); // 先关闭可能存在的右键菜单，和按下Esc行为保持一致
                chuangkou.close(); // 关闭窗口到托盘
            } else { // 窗口没显示，或被其他窗口盖住 → 移动到记录的位置并显示、拉到最前
                chuangkou.move(config["chuangkou_x"].toInt(), config["chuangkou_y"].toInt()); // 把chuangkou移动到记录的位置
                xianshi(chuangkou); // 显示窗口并拉到屏幕最前
            }
        });
    // 使用自定义的事件过滤器类HotkeyEditFilter，用于拦截hotkeyEdit的焦点事件，实现：1.当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象和当前已注册的全局快捷键，让用户能自由地敲任何组合键；2.当hotkeyEdit失去焦点时判断用户输入的快捷键是否合规，不合规就恢复成原来的快捷键，并把旧的全局快捷键重新注册回去。新快捷键只有点了“应用”或“确定”才会真的注册
    hotkeyEdit->installEventFilter(new HotkeyEditFilter(hotkeyEdit, hotkey, itemHotkeys, &a)); // 创建事件过滤器对象，并把它安装到hotkeyEdit上

    QGridLayout *zu_qidong = jianFenzukuang("启动", yemian3Layout);
    QCheckBox *autostartupCheck = new QCheckBox("开机自启动");
    jiaGouxuan(zu_qidong, autostartupCheck);
    // “以管理员权限启动”和“开机自启动”互相独立：只开它的话开机不自启、手动启动时自己弹UAC提权；两个都开的话开机由计划任务直接以管理员权限起来
    QCheckBox *guanliyuanCheck = new QCheckBox("以管理员权限启动");
    jiaGouxuan(zu_qidong, guanliyuanCheck);
    jiaShuoming(zu_qidong, "勾选后就能允许QuickSay在任何地方输入，比如以管理员权限运行的记事本。");

    QGridLayout *zu_beifen = jianFenzukuang("备份与恢复", yemian3Layout);
    QWidget *beifenWidget = new QWidget; // 把导出和导入两个按钮并排放在一行里
    QHBoxLayout *beifenLayout = new QHBoxLayout(beifenWidget);
    beifenLayout->setSpacing(8);
    beifenLayout->setContentsMargins(0, 0, 0, 0);
    QPushButton *exportBackupButton = new QPushButton("导出备份");
    QPushButton *importBackupButton = new QPushButton("导入备份");
    exportBackupButton->setFixedSize(92, 28);
    importBackupButton->setFixedSize(92, 28);
    beifenLayout->addWidget(exportBackupButton);
    beifenLayout->addWidget(importBackupButton);
    beifenLayout->addStretch();
    zu_beifen->addWidget(beifenWidget, zu_beifen->rowCount(), 0, 1, 3);
    jiaShuoming(zu_beifen, "备份包含全部设置、分组和短语。导入会完全覆盖现有内容，并立即重启QuickSay。这两个按钮点了就直接执行，不受“应用”“取消”影响。");
    yemian3Layout->addStretch();

    //--------------------页签4：关于--------------------
    QVBoxLayout *yemian4Layout = nullptr;
    QScrollArea *yemian4 = jianShezhiYemian(yemian4Layout);
    shezhiYeqian->addTab(yemian4, QIcon(tubiaoMulu + "/info.ico"), "关于");

    QGridLayout *zu_guanyu = jianFenzukuang("关于 QuickSay", yemian4Layout);
    jiaHang(zu_guanyu, "版本", new QLabel(g_quickSayVersion)); //【【【更新版本后记得改一下文件开头的g_quickSayVersion】】】
    QLabel *xiangmuLianjie = new QLabel("<a href=\"https://github.com/DarkKandaoMaster/QuickSay\">https://github.com/DarkKandaoMaster/QuickSay</a>");
    xiangmuLianjie->setOpenExternalLinks(true); // 点一下用系统默认浏览器打开
    xiangmuLianjie->setCursor(Qt::PointingHandCursor);
    jiaHang(zu_guanyu, "项目地址", xiangmuLianjie);
    jiaShuoming(zu_guanyu, "QuickSay 是一个 Windows 上的快捷短语工具：把常用的话存起来，并在需要时使用 QuickSay 输入。");

    QGridLayout *zu_gengxin = jianFenzukuang("更新", yemian4Layout);
    QCheckBox *gengxinCheck = new QCheckBox("启动时检查更新");
    jiaGouxuan(zu_gengxin, gengxinCheck);
    QPushButton *lijiJianchaButton = new QPushButton("立即检查");
    lijiJianchaButton->setFixedSize(92, 28);
    zu_gengxin->addWidget(lijiJianchaButton, zu_gengxin->rowCount(), 0, 1, 3, Qt::AlignLeft);
    QObject::connect(lijiJianchaButton, &QPushButton::clicked, []() { jianchaGengxin(true); });
    yemian4Layout->addStretch();

    //--------------------底部固定的三个按钮--------------------
    QHBoxLayout *shezhiAnniuHang = new QHBoxLayout;
    shezhiAnniuHang->setSpacing(12); // 按钮之间空12像素，和TrafficMonitor一样
    shezhiAnniuHang->addStretch();
    QPushButton *quedingButton = new QPushButton("确定", &shezhichuangkou);
    QPushButton *quxiaoButton = new QPushButton("取消", &shezhichuangkou);
    QPushButton *yingyongButton = new QPushButton("应用", &shezhichuangkou);
    for (QPushButton *anniu : {quedingButton, quxiaoButton, yingyongButton}) {
        anniu->setFixedSize(92, 28); // 按钮尺寸也是照着TrafficMonitor量的
        shezhiAnniuHang->addWidget(anniu);
    }
    shezhiWaiceng->addLayout(shezhiAnniuHang);

    //--------------------设置事务：应用 / 确定 / 取消--------------------
    // 界面上的值和config里的值有一处对不上，就说明有还没应用的修改
    auto youWeiYingyongXiugai = [&]() -> bool {
        if (hotkeyEdit->keySequence().toString() != config["hotkey"].toString()) return true;
        if (zhidingCheck->isChecked() != config["zhiding"].toBool()) return true;
        if (widthSpin->value() != config["width"].toInt()) return true;
        if (heightSpin->value() != config["height"].toInt()) return true;
        if (itemHeightSpin->value() != config["default_item_height"].toInt()) return true;
        if (itemPaddingHorizontalSpin->value() != config["phrase_item_padding_horizontal"].toInt()) return true;
        if (itemPaddingVerticalSpin->value() != config["phrase_item_padding_vertical"].toInt()) return true;
        if (gundongSpin->value() != config["gundong"].toInt()) return true;
        if (jiaobiaoCheck->isChecked() != config["jiaobiao"].toBool()) return true;
        if (delaySpin->value() != config["delay"].toInt()) return true;
        if (badgeKeyCheck->isChecked() != config["badge_key_input_phrase_when_pinned"].toBool()) return true;
        if (enterKeyCheck->isChecked() != config["enter_key_input_phrase_when_pinned"].toBool()) return true;
        if (autostartupCheck->isChecked() != config["ziqidong"].toBool()) return true;
        if (guanliyuanCheck->isChecked() != config["guanliyuan"].toBool()) return true;
        if (gengxinCheck->isChecked() != config["qidong_jiancha_gengxin"].toBool()) return true;
        return false;
    };
    auto gengxinYingyongButton = [&]() { yingyongButton->setEnabled(youWeiYingyongXiugai()); }; // 没有修改时“应用”是灰的

    // 把config里的设置填回所有控件。打开窗口、点“取消”、关掉窗口时都调它，效果就是丢弃还没应用的修改
    QVector<QObject *> shezhiKongjian = {hotkeyEdit, zhidingCheck, widthSpin, heightSpin, itemHeightSpin, itemPaddingHorizontalSpin, itemPaddingVerticalSpin, gundongSpin, jiaobiaoCheck, delaySpin, badgeKeyCheck, enterKeyCheck, autostartupCheck, guanliyuanCheck, gengxinCheck};
    auto zairuShezhi = [&]() {
        for (QObject *kongjian : shezhiKongjian) kongjian->blockSignals(true); // 程序自己填值时不能触发下面那些信号，否则又要跑一遍“有没有修改”的判断
        hotkeyEdit->setKeySequence(QKeySequence(config["hotkey"].toString()));
        zhidingCheck->setChecked(config["zhiding"].toBool());
        widthSpin->setValue(config["width"].toInt());
        heightSpin->setValue(config["height"].toInt());
        itemHeightSpin->setValue(config["default_item_height"].toInt());
        itemPaddingHorizontalSpin->setValue(config["phrase_item_padding_horizontal"].toInt());
        itemPaddingVerticalSpin->setValue(config["phrase_item_padding_vertical"].toInt());
        gundongSpin->setValue(config["gundong"].toInt());
        jiaobiaoCheck->setChecked(config["jiaobiao"].toBool());
        delaySpin->setValue(config["delay"].toInt());
        badgeKeyCheck->setChecked(config["badge_key_input_phrase_when_pinned"].toBool());
        enterKeyCheck->setChecked(config["enter_key_input_phrase_when_pinned"].toBool());
        autostartupCheck->setChecked(config["ziqidong"].toBool());
        guanliyuanCheck->setChecked(config["guanliyuan"].toBool());
        gengxinCheck->setChecked(config["qidong_jiancha_gengxin"].toBool());
        for (QObject *kongjian : shezhiKongjian) kongjian->blockSignals(false);
        gengxinYingyongButton();
    };
    zairuShezhi(); // 建完控件先填一遍值

    // 任何一个控件被改动都要重算“应用”按钮的可用状态
    QObject::connect(hotkeyEdit, &QKeySequenceEdit::keySequenceChanged, [&]() { gengxinYingyongButton(); });
    for (QCheckBox *gouxuan : {zhidingCheck, jiaobiaoCheck, badgeKeyCheck, enterKeyCheck, autostartupCheck, guanliyuanCheck, gengxinCheck}) {
        QObject::connect(gouxuan, &QCheckBox::toggled, [&]() { gengxinYingyongButton(); });
    }
    for (QSpinBox *shuzi : {widthSpin, heightSpin, itemHeightSpin, itemPaddingHorizontalSpin, itemPaddingVerticalSpin, gundongSpin, delaySpin}) {
        QObject::connect(shuzi, QOverload<int>::of(&QSpinBox::valueChanged), [&]() { gengxinYingyongButton(); });
    }

    // 打开设置窗口前，按系统里真实的开机自启状态刷新config。因为用户完全可能绕过QuickSay，自己跑去“任务计划程序”里删了任务、或者去注册表里删了Run项
    auto shuaxinZiqidongZhuangtai = [&]() {
        QSettings reg(g_ziqidongRegPath, QSettings::NativeFormat); // 创建QSettings对象，用于访问注册表Run项
        QString renwuExe;
        bool renwuOn = queryAdminRenwu(&renwuExe) && renwuExe.compare(ziqidongExePath(), Qt::CaseInsensitive) == 0; // 任务在、启用着、记的路径也还是程序现在的位置，才算管理员权限开机自启真的生效了
        bool on = renwuOn || reg.contains(g_ziqidongRegName); // 两条路只要有一条通着，就是开着自启
        if (config["ziqidong"].toBool() != on) { // 真实状态和config对不上，以真实状态为准
            config["ziqidong"] = on;
            saveConfig(configPath); // 写入程序设置到config.json
        }
    };

    // “应用”和“确定”都走这里：先校验，再把界面上的值一项项落到config和正在运行的程序上。返回false表示这次没应用成功
    auto yingyongShezhi = [&]() -> bool {
        // 全局快捷键：只有真的改了才校验、才注册。焦点离开输入框时旧快捷键已经恢复注册了，这里才是新快捷键真正生效的时刻
        QKeySequence xinKuaijiejian = hotkeyEdit->keySequence();
        if (xinKuaijiejian.toString() != config["hotkey"].toString()) {
            if (xinKuaijiejian.isEmpty() || !isValidHotkey(xinKuaijiejian, itemHotkeys, hotkeyEdit)) { // 不合规时isValidHotkey自己会弹警告
                hotkeyEdit->setKeySequence(QKeySequence(config["hotkey"].toString())); // 恢复成原来的快捷键，本次应用失败
                gengxinYingyongButton();
                return false;
            }
            hotkey->setShortcut(xinKuaijiejian, true); // 立即应用新快捷键
            config["hotkey"] = xinKuaijiejian.toString();
        }

        bool gaoduBianle = itemHeightSpin->value() != config["default_item_height"].toInt(); // 这两项应用完要提示用户，先记下来，等落盘之后再弹窗
        bool jiaobiaoBianle = jiaobiaoCheck->isChecked() != config["jiaobiao"].toBool();
        bool daxiaoBianle = widthSpin->value() != config["width"].toInt() || heightSpin->value() != config["height"].toInt();
        bool neibianjuBianle = itemPaddingHorizontalSpin->value() != config["phrase_item_padding_horizontal"].toInt() || itemPaddingVerticalSpin->value() != config["phrase_item_padding_vertical"].toInt();
        bool zhidingBianle = zhidingCheck->isChecked() != config["zhiding"].toBool();

        config["width"] = widthSpin->value();
        config["height"] = heightSpin->value();
        config["zhiding"] = zhidingCheck->isChecked();
        config["default_item_height"] = itemHeightSpin->value();
        config["phrase_item_padding_horizontal"] = itemPaddingHorizontalSpin->value();
        config["phrase_item_padding_vertical"] = itemPaddingVerticalSpin->value();
        config["gundong"] = gundongSpin->value();
        config["jiaobiao"] = jiaobiaoCheck->isChecked();
        config["delay"] = delaySpin->value();
        config["badge_key_input_phrase_when_pinned"] = badgeKeyCheck->isChecked();
        config["enter_key_input_phrase_when_pinned"] = enterKeyCheck->isChecked();
        config["qidong_jiancha_gengxin"] = gengxinCheck->isChecked();
        saveConfig(configPath); // 写入程序设置到config.json

        liebiao.verticalScrollBar()->setSingleStep(config["gundong"].toInt()); // 滚动速度立刻同步到正在使用的滚动条，不用重启软件才能生效
        if (neibianjuBianle) applyPhraseListLayout(liebiao, tabItemHeight(tabBar, tabBar.currentIndex()), tabColumns(tabBar, tabBar.currentIndex())); // 内边距变了行高也会跟着变，所以要连网格一起重设
        if (daxiaoBianle && yingyongZhuchuangkouDaxiao) yingyongZhuchuangkouDaxiao(); // 调整主窗口大小和主窗口控件
        if (zhidingBianle) { // 在不改变其他窗口属性的前提下，给主窗口添加/删除始终置顶属性
            if (config["zhiding"].toBool()) chuangkou.setWindowFlags(chuangkou.windowFlags() | Qt::WindowStaysOnTopHint);
            else chuangkou.setWindowFlags(chuangkou.windowFlags() & ~Qt::WindowStaysOnTopHint);
            chuangkou.move(config["chuangkou_x"].toInt(), config["chuangkou_y"].toInt()); // 因为修改窗口属性后窗口会自动关闭，所以我们这里要手动显示主窗口
            xianshi(chuangkou);
        }

        // 开机自启动和以管理员权限启动这两个开关要一起交给applyZiqidong处理。放在最后应用，是因为它可能弹UAC、可能失败，失败时要把这两项恢复原样
        bool jiuZiqidong = config["ziqidong"].toBool(), jiuGuanliyuan = config["guanliyuan"].toBool();
        bool xinGuanliyuan = guanliyuanCheck->isChecked();
        bool ziqidongBianle = autostartupCheck->isChecked() != jiuZiqidong || xinGuanliyuan != jiuGuanliyuan;
        if (ziqidongBianle) {
            config["ziqidong"] = autostartupCheck->isChecked();
            config["guanliyuan"] = xinGuanliyuan;
            if (!applyZiqidong(false)) { // 可能是计划任务没建成，也可能是旧计划任务删不掉
                config["ziqidong"] = jiuZiqidong;
                config["guanliyuan"] = jiuGuanliyuan;
                applyZiqidong(true); // 按原来的设置恢复系统状态；不打扰模式保证这里绝不再弹UAC
                saveConfig(configPath); // 把恢复后的设置写回config.json
                zairuShezhi(); // 界面上那两个勾也恢复成config里的真实状态，不能让界面和系统里的真实状态对不上
                QMessageBox::information(&shezhichuangkou, "提示", "应用开机自启动设置失败");
                return false;
            }
            saveConfig(configPath); // 系统设置应用成功后写入config.json
        }

        gengxinYingyongButton(); // 全应用完了，“应用”按钮该变灰了
        if (gaoduBianle) QMessageBox::information(&shezhichuangkou, "提示", "该设置名称为“默认短语项高度”，因此只影响新建分组时的短语项高度的值。  \n如果要修改某个分组的短语项高度，请右键修改该分组。  ");
        if (jiaobiaoBianle) QMessageBox::information(&shezhichuangkou, "提示", "鼠标移到主窗口后生效  ");

        // 刚勾上“以管理员权限启动”、而当前这个进程还是普通权限时，得重启一次才能真的以管理员权限跑。问一句要不要现在就重启，而不是自作主张把用户正用着的QuickSay掐掉
        if (ziqidongBianle && xinGuanliyuan && !jiuGuanliyuan && !isProcessElevated()) {
            QString chongqiTishi = "重启QuickSay生效"; // 当前普通权限进程无法原地变成管理员权限，只能重启为管理员权限进程
            QMessageBox box(QMessageBox::Question, "QuickSay", chongqiTishi, QMessageBox::NoButton, &shezhichuangkou);
            QPushButton *lijiButton = box.addButton("立即重启", QMessageBox::RejectRole); // 放在“暂不重启”右边；虽然交换了按钮角色，但下面仍按按钮指针判断用户点了哪个
            QPushButton *zanbuButton = box.addButton("暂不重启", QMessageBox::AcceptRole); // 不重启也没关系，开关已经存进config.json了，下次启动照样生效
            box.setDefaultButton(lijiButton); // 回车仍然表示立即重启，不受为了交换位置而调整按钮角色的影响
            box.setEscapeButton(zanbuButton); // Esc仍然表示暂不重启
            box.exec();
            if (box.clickedButton() != lijiButton) return true;
            // 下面这段和main()开头提权重启那段是一回事，只是多了「先把快捷键让出来」这一步
            for (auto &hk : itemHotkeys) {
                if (hk) hk->setRegistered(false);
            } // 先注销短语项快捷键：提权那份起来得比这份退得快的话，会撞上还没释放的快捷键，注册不上
            if (hotkey) hotkey->setRegistered(false); // 全局快捷键同理
            danshiliTongzhi.setEnabled(false); // 下面会关闭它正在监听的事件句柄，先停掉监听，避免QWinEventNotifier继续盯着一个已经失效的句柄
            shifangDanshili(); // 再让出互斥体，否则提权起来的那份会被单实例检测拦下，结果谁也没起来
            // 如果刚才为了管理员开机自启已经弹UAC建好了计划任务，就直接用那份已授权的任务重启；这样点“立即重启”时不会再次弹UAC
            if (config["ziqidong"].toBool() && yongRenwuChongqiGuanliyuan()) {
                a.quit();
                return true;
            }
            if (tiquanChongqiZishen()) {
                a.quit();
                return true;
            } // 没有管理员任务时（例如没开开机自启），正常用runas弹一次UAC启动提权实例
            // 用户在UAC弹窗上点了「否」：把让出去的东西一样样抢回来，以普通权限接着跑
            tongzhiYiyouShili();
            danshiliTongzhi.setHandle(g_danshiliEvent); // 重新抢到的是一个新事件句柄，必须交给通知器；它原来记着的旧句柄已经在shifangDanshili()里关掉了
            danshiliTongzhi.setEnabled(true);
            if (hotkey) hotkey->setRegistered(true);
            for (auto &hk : itemHotkeys) {
                if (hk) hk->setRegistered(true);
            }
            QMessageBox::information(&shezhichuangkou, "提示", "重启失败，请手动重启"); // 用户拒绝UAC后明确提示重启没有成功；管理员开关仍会在下次手动重启时生效
        }
        return true;
    };

    QObject::connect(yingyongButton, &QPushButton::clicked, [&]() { yingyongShezhi(); }); // 应用：校验并应用，不关窗口
    QObject::connect(quedingButton, &QPushButton::clicked, [&]() { if (yingyongShezhi()) shezhichuangkou.close(); }); // 确定：应用成功了才关窗口
    QObject::connect(quxiaoButton, &QPushButton::clicked, [&]() { shezhichuangkou.close(); }); // 取消：关窗口，下面那个回调会把界面恢复成上次应用后的状态
    shezhichuangkou.guanbiHuidiao = [&]() { zairuShezhi(); }; // 标题栏的关闭按钮和“取消”一样：丢弃还没应用的修改

    // 打开设置窗口。重复打开只把窗口拉到前台，绝不重新填一遍值，否则用户还没应用的修改就没了
    std::function<void()> dakaiShezhiChuangkou = [&]() {
        if (!shezhichuangkou.isVisible()) {
            shuaxinZiqidongZhuangtai(); // 按系统里真实的自启状态刷新config
            zairuShezhi(); // 再把config里的值填进界面
            shezhichuangkou.move(config["shezhichuangkou_x"].toInt(), config["shezhichuangkou_y"].toInt()); // 把设置窗口移动到记录的位置
            shezhichuangkou.show();
        }
        if (shezhichuangkou.isMinimized()) shezhichuangkou.showNormal();
        shezhichuangkou.raise();
        shezhichuangkou.activateWindow();
    };

    // 导出时直接从当前内存里的设置、分组和短语生成备份；正文中的<img>/<file>路径保持原文，不读取路径指向的文件
    QObject::connect(exportBackupButton, &QPushButton::clicked,
        [&]() {
            QString defaultName = QString("QuickSay备份_%1").arg(QDate::currentDate().toString(Qt::ISODate));
            QString selectedPath = QFileDialog::getSaveFileName(&shezhichuangkou, "导出QuickSay备份", QDir::home().filePath(defaultName), "QuickSay备份文件 (*)");
            if (selectedPath.isEmpty()) return;
            QFileInfo selectedInfo(selectedPath);
            QString filePath = selectedPath;
            if (!selectedInfo.suffix().isEmpty()) filePath = selectedInfo.dir().filePath(selectedInfo.completeBaseName()); // 即使用户手动输入了.txt之类的扩展名，也按要求导出为无扩展名文件
            if (filePath != selectedPath && QFile::exists(filePath)) {
                if (QMessageBox::question(&shezhichuangkou, "确认覆盖", QString("文件“%1”已存在，是否覆盖？").arg(QFileInfo(filePath).fileName()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
            }
            QString error;
            if (!writeQuickSayFileAtomically(filePath, createQuickSayBackupFile(liebiao, tabBar), error)) {
                QMessageBox::warning(&shezhichuangkou, "导出备份失败", QString("无法导出备份：%1").arg(error));
                return;
            }
            QMessageBox::information(&shezhichuangkou, "导出备份成功", QString("备份已导出到：\n%1").arg(QDir::toNativeSeparators(filePath)));
        });

    // 导入失败时parseQuickSayBackupFile只读取不写入；用户确认后才进入三文件事务，因此取消、校验失败或替换失败都不会改变现有数据
    QObject::connect(importBackupButton, &QPushButton::clicked,
        [&]() {
            QString filePath = QFileDialog::getOpenFileName(&shezhichuangkou, "导入QuickSay备份", QDir::homePath(), "QuickSay备份文件 (*)");
            if (filePath.isEmpty()) return;
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                QMessageBox::warning(&shezhichuangkou, "导入备份失败", QString("无法读取备份：%1").arg(file.errorString()));
                return;
            }
            QByteArray fileData = file.readAll();
            file.close();
            QuickSayBackupData backup;
            QString error;
            if (!parseQuickSayBackupFile(fileData, backup, error)) {
                QMessageBox::warning(&shezhichuangkou, "导入备份失败", QString("文件无效或已损坏：%1").arg(error));
                return;
            }
            QString confirmText = "导入备份将完全覆盖当前的全部设置、分组和短语，且无法撤销。\n\n是否继续？";
            if (QMessageBox::warning(&shezhichuangkou, "确认导入备份", confirmText, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;

            moveImportedWindowsOntoScreens(backup.settings); // 原坐标仍在任意屏幕上就原样保留，完全离屏的窗口才回到主屏幕中央
            if (!replaceQuickSayDataTransactionally(backup, configPath, dataPath, tabPath, error)) {
                QMessageBox::warning(&shezhichuangkou, "导入备份失败", QString("导入失败：%1").arg(error));
                return;
            }
            g_zhengzaiDaoruChongqi = true; // 从这一刻开始旧窗口和旧全局状态只准退出，绝不准再落盘覆盖导入结果

            bool versionKnown = false;
            int versionCompare = compareQuickSayVersions(backup.quickSayVersion, g_quickSayVersion, versionKnown);
            if (versionKnown && versionCompare > 0) {
                QMessageBox::warning(&shezhichuangkou, "导入成功", QString("刚刚导入的备份来自 QuickSay 的较新版本（%1），某些设置或短语内容可能无法被完整识别或支持。建议更新到最新版本。").arg(backup.quickSayVersion));
            } else if (!versionKnown || versionCompare < 0) {
                QString localDateTime = backup.exportedAtUtc.toLocalTime().toString("yyyy年M月d日 HH:mm:ss");
                QMessageBox::warning(&shezhichuangkou, "导入成功", QString("刚刚导入的备份版本较旧或未知（导出于：%1），建议重新导出以更新备份。").arg(localDateTime));
            } else {
                QMessageBox::information(&shezhichuangkou, "导入成功", "备份已完整导入，QuickSay 将立即重启。");
            }

            // 导入成功后不提供“稍后重启”：先停输出和键盘钩子、注销全部快捷键、释放单实例对象，再让普通权限Explorer启动不带参数的新实例
            stopQuickSayOutput();
            stopQuickSayKeyboardHook();
            for (auto &hk : itemHotkeys) {
                if (hk) hk->setRegistered(false);
            }
            if (hotkey) hotkey->setRegistered(false);
            danshiliTongzhi.setEnabled(false);
            shifangDanshili();
            QString restartError;
            if (!startQuickSayThroughExplorer(restartError)) {
                QMessageBox::critical(&shezhichuangkou, "重启失败", QString("备份已经导入，但无法通过 Windows Explorer 重新启动 QuickSay：%1\n\n请手动启动 QuickSay。").arg(restartError));
            }
            a.quit(); // 无论Explorer返回结果如何，旧实例都必须立即退出，不能带着导入前的全局状态继续运行
        });

    // 在chuangkou右上角放一个“设置”按钮
    QPushButton shezhi("", &chuangkou); // 创建设置按钮，文本为空字符串。不然文本也会显示在按钮上
    shezhi.setObjectName("iconButton"); // 应用图标按钮样式
    shezhi.setIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/settings.svg")); // 设置按钮图标
    shezhi.setIconSize(QSize(20, 20)); // 调整图标大小为20*20像素
    shezhi.setToolTip("设置"); // 设置鼠标悬停提示文字为“设置”
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false); // 禁用Qt自带的QToolTip悬停提示动画
    // 当按下“设置”按钮时，打开设置窗口
    QObject::connect(&shezhi, &QPushButton::clicked, [&]() { dakaiShezhiChuangkou(); });

    // 创建tianjiachuangkou窗口
    QString currentTabName = ""; // 记录用户添加短语时使用的分组名称
    int insertAfterRow = -1; // 记录新增短语要插入到哪一行后面；-1表示追加到列表末尾
    QWidget tianjiachuangkou;
    tianjiachuangkou.setStyleSheet(g_quanjuQss);
    g_tianjiachuangkou = &tianjiachuangkou;
    tianjiachuangkou.setWindowTitle("QuickSay-添加");
    tianjiachuangkou.setWindowIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/软件图标.svg"));
    QPlainTextEdit tianjiakuang(&tianjiachuangkou);
    QPushButton tianjia_gaojishuru("如何更高级地输入？", &tianjiachuangkou);
    QLabel tianjia_beizhuwenben("备注：", &tianjiachuangkou);
    QPlainTextEdit tianjia_beizhukuang(&tianjiachuangkou);
    QLabel tianjia_kjjwenben("快捷键：", &tianjiachuangkou);
    QKeySequenceEdit tianjia_kjjkuang(&tianjiachuangkou);
    QPushButton tianjia_kjjqingkong("清空", &tianjiachuangkou);
    QPushButton tianjiaquxiao("取消", &tianjiachuangkou);
    QPushButton tianjiaqueding("添加", &tianjiachuangkou);
    // 当按下“如何更高级地输入？”按钮时，弹出表达式提示窗口
    QObject::connect(&tianjia_gaojishuru, &QPushButton::clicked,
        [&]() {
            showAdvancedInputHelp(tianjiachuangkou);
        });
    // 当按下“清空”按钮时，直接清空快捷键输入框
    QObject::connect(&tianjia_kjjqingkong, &QPushButton::clicked,
        [&]() {
            tianjia_kjjkuang.clear();
        });
    // 当按下“取消”按钮时，直接关闭tianjiachuangkou窗口
    QObject::connect(&tianjiaquxiao, &QPushButton::clicked,
        [&]() {
            tianjiachuangkou.close();
        });
    // 当按下“添加”按钮时
    QObject::connect(&tianjiaqueding, &QPushButton::clicked,
        [&]() {
            QKeySequence seq = tianjia_kjjkuang.keySequence(); // 获取快捷键输入框里的快捷键
            if (!seq.isEmpty()) { // 如果快捷键不为空
                if (!isValidHotkey(seq, itemHotkeys, &tianjia_kjjkuang)) { // 如果快捷键不合规（调用isValidHotkey函数检查快捷键是否合规）
                    tianjia_kjjkuang.clear(); // 清空快捷键输入框
                    return; // 结束当前这个槽函数的执行，但是不关闭tianjiachuangkou，于是用户可以重新在这个窗口输入快捷键
                }
            }

            QString text = tianjiakuang.toPlainText(); // 获取输入框里的内容
            if (!text.isEmpty()) { // 如果获取到的内容不是空的
                QListWidgetItem *newItem = new QListWidgetItem(); // new一个短语项对象，并把它的地址给newItem
                newItem->setData(Qt::UserRole, text); // 把用户输入的短语存到newItem的Qt::UserRole
                newItem->setData(Qt::UserRole + 1, tianjia_beizhukuang.toPlainText()); // 获取备注输入框里的内容，并把输入内容存到对应短语项的Qt::UserRole+1
                newItem->setData(Qt::UserRole + 2, currentTabName); // 把记录的分组名称存到newItem的Qt::UserRole+2
                newItem->setData(Qt::UserRole + 3, seq.toString()); // 把用户输入的快捷键字符串存到对应短语项的Qt::UserRole+3
                updateItemDisplay(newItem); // 更新对应短语项的显示
                if (insertAfterRow >= 0 && insertAfterRow < liebiao.count()) { // 如果是通过右键“在当前短语后添加短语”打开的添加窗口
                    liebiao.insertItem(insertAfterRow + 1, newItem); // 把newItem插入到当前短语后面
                } else {
                    liebiao.addItem(newItem); // 把newItem添加到列表末尾
                }
                saveListToJson(liebiao, dataPath); // 添加后写入列表内容到data.json
                rebuildItemHotkeys(liebiao, itemHotkeys, &a); // 添加后为liebiao中的短语项注册快捷键
                filterListByTab(liebiao, tabBar.tabText(tabBar.currentIndex()), search.text()); // 添加后根据当前选中分组和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
            }
            tianjiachuangkou.close();
        });
    // 使用自定义的事件过滤器类KjjHotkeyEditFilter，用于拦截tianjia_kjjkuang的焦点事件，实现：当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象，失去焦点时恢复
    tianjia_kjjkuang.installEventFilter(new KjjHotkeyEditFilter(&tianjia_kjjkuang, itemHotkeys, &a)); // 创建事件过滤器对象，并把它安装到tianjia_kjjkuang上

    // 在chuangkou右上角放一个“添加”按钮
    QPushButton tianjia("", &chuangkou); // 创建添加按钮，文本为空字符串
    tianjia.setObjectName("iconButton"); // 应用图标按钮样式
    tianjia.setIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/add.svg")); // 设置按钮图标
    tianjia.setIconSize(QSize(20, 20)); // 调整图标大小为20*20像素
    tianjia.setToolTip("添加短语"); // 设置鼠标悬停提示文字为“添加短语”
    // 当按下“添加”按钮时
    QObject::connect(&tianjia, &QPushButton::clicked,
        [&]() {
            tianjiakuang.clear();
            tianjia_beizhukuang.clear();
            tianjia_kjjkuang.clear();
            currentTabName = tabBar.tabText(tabBar.currentIndex()); // 记录用户点击右上角加号时当前分组的名称
            insertAfterRow = -1; // 右上角加号仍然追加到列表末尾
            tianjiachuangkou.move(config["tianjiachuangkou_x"].toInt(), config["tianjiachuangkou_y"].toInt()); // 把tianjiachuangkou移动到记录的位置
            xianshi(tianjiachuangkou);
            tianjiakuang.setFocus(); // 把焦点给到tianjiakuang，而不是其他控件
        });

    // 在chuangkou右上角放一个图钉按钮
    QPushButton tuding("", &chuangkou); // 创建图钉按钮，文本为空字符串
    g_tuding = &tuding; // 把图钉按钮地址赋值给全局指针，用于让键盘钩子能通过反引号`键触发“切换钉住”
    tuding.setObjectName("iconButton"); // 应用图标按钮样式
    if (config["tudingflag"].toBool() == true) { // 如果钉住窗口
        tuding.setIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/实心图钉.svg")); // 设置按钮图标为实心图钉
        tuding.setToolTip("钉住/取消钉住窗口（ ` ）"); // 设置鼠标悬停提示文字为“钉住/取消钉住窗口（ ` ）”
    } else { // 如果没有钉住窗口
        tuding.setIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/空心图钉.svg")); // 设置按钮图标为空心图钉
        tuding.setToolTip("钉住/取消钉住窗口（ ` ）"); // 设置鼠标悬停提示文字为“钉住/取消钉住窗口（ ` ）”
    }
    tuding.setIconSize(QSize(20, 20)); // 调整图标大小为20*20像素
    // 当按下图钉按钮时，切换按钮图标
    QObject::connect(&tuding, &QPushButton::clicked,
        [&]() {
            if (config["tudingflag"].toBool() == true) { // 如果钉住窗口
                tuding.setIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/空心图钉.svg")); // 切换按钮图标为空心图钉
                tuding.setToolTip("钉住/取消钉住窗口（ ` ）"); // 设置鼠标悬停提示文字为“钉住/取消钉住窗口（ ` ）”
                config["tudingflag"] = false;
                saveConfig(configPath); // 写入程序设置到config.json
            } else { // 如果没有钉住窗口
                tuding.setIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/实心图钉.svg")); // 切换按钮图标为实心图钉
                tuding.setToolTip("钉住/取消钉住窗口（ ` ）"); // 设置鼠标悬停提示文字为“钉住/取消钉住窗口（ ` ）”
                config["tudingflag"] = true;
                saveConfig(configPath); // 写入程序设置到config.json
            }
        });

    // 创建xiugaichuangkou窗口
    QListWidgetItem *currentEditingItem = nullptr; // 记录用户点到的是liebiao中的哪个选项
    QWidget xiugaichuangkou;
    xiugaichuangkou.setStyleSheet(g_quanjuQss);
    g_xiugaichuangkou = &xiugaichuangkou;
    xiugaichuangkou.setWindowTitle("QuickSay-修改");
    xiugaichuangkou.setWindowIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/软件图标.svg"));
    QPlainTextEdit xiugaikuang(&xiugaichuangkou);
    QPushButton xiugai_gaojishuru("如何更高级地输入？", &xiugaichuangkou);
    QLabel xiugai_beizhuwenben("备注：", &xiugaichuangkou);
    QPlainTextEdit xiugai_beizhukuang(&xiugaichuangkou);
    QLabel xiugai_kjjwenben("快捷键：", &xiugaichuangkou);
    QKeySequenceEdit xiugai_kjjkuang(&xiugaichuangkou);
    QPushButton xiugai_kjjqingkong("清空", &xiugaichuangkou);
    QPushButton xiugaiquxiao("取消", &xiugaichuangkou);
    QPushButton xiugaiqueding("修改", &xiugaichuangkou);
    // 当按下“如何更高级地输入？”按钮时，弹出表达式提示窗口
    QObject::connect(&xiugai_gaojishuru, &QPushButton::clicked,
        [&]() {
            showAdvancedInputHelp(xiugaichuangkou);
        });
    // 当按下“清空”按钮时，直接清空快捷键输入框
    QObject::connect(&xiugai_kjjqingkong, &QPushButton::clicked,
        [&]() {
            xiugai_kjjkuang.clear();
        });
    // 当按下“取消”按钮时，直接关闭xiugaichuangkou窗口
    QObject::connect(&xiugaiquxiao, &QPushButton::clicked,
        [&]() {
            xiugaichuangkou.close();
        });
    // 当按下“修改”按钮时
    QObject::connect(&xiugaiqueding, &QPushButton::clicked,
        [&]() {
            if (!currentEditingItem) { // 如果currentEditingItem为空指针 //以防万一用
                xiugaichuangkou.close(); // 关闭xiugaichuangkou
                return; // 结束当前这个槽函数的执行
            }

            QKeySequence seq = xiugai_kjjkuang.keySequence(); // 获取快捷键输入框里的快捷键
            if (!seq.isEmpty()) { // 如果快捷键不为空
                if (!isValidHotkey(seq, itemHotkeys, &xiugai_kjjkuang, itemHotkeys[liebiao.row(currentEditingItem)])) { // 如果快捷键不合规（调用isValidHotkey函数检查快捷键是否合规） //这里需要传入selfhk参数，即最后一个参数，用来防止自己和自己冲突 //我来解释一下最后一个参数哈：它先是获取当前正在编辑的短语项的行号，然后取出itemHotkeys数组中对应行号的QHotkey *指针，如果该短语项没有快捷键，那么取出的就是nullptr
                    xiugai_kjjkuang.setKeySequence(QKeySequence(currentEditingItem->data(Qt::UserRole + 3).toString())); // 把对应短语项的Qt::UserRole+3里的快捷键字符串重新放进输入框，也就是说恢复输入框为原始快捷键
                    return; // 结束当前这个槽函数的执行，但是不关闭xiugaichuangkou，于是用户可以重新在这个窗口输入快捷键
                }
            }
            currentEditingItem->setData(Qt::UserRole + 3, seq.toString()); // 如果快捷键合规或者为空，那么把用户输入的快捷键字符串存到对应短语项的Qt::UserRole+3

            QString text = xiugaikuang.toPlainText(); // 获取输入框里的内容
            if (!text.isEmpty()) { // 如果获取到的内容不是空的
                currentEditingItem->setData(Qt::UserRole, text); // 把输入内容修改到短语项的Qt::UserRole中
            }

            currentEditingItem->setData(Qt::UserRole + 1, xiugai_beizhukuang.toPlainText()); // 获取备注输入框里的内容，并把输入内容存到对应短语项的Qt::UserRole+1

            updateItemDisplay(currentEditingItem); // 更新对应短语项的显示
            saveListToJson(liebiao, dataPath); // 修改后写入列表内容到data.json
            rebuildItemHotkeys(liebiao, itemHotkeys, &a); // 修改后为liebiao中的短语项注册快捷键
            filterListByTab(liebiao, tabBar.tabText(tabBar.currentIndex()), search.text()); // 修改后根据当前选中分组和搜索框文字过滤短语项，并且生成角标字符、存到对应短语项的Qt::UserRole+4
            xiugaichuangkou.close();
        });
    // 使用自定义的事件过滤器类KjjHotkeyEditFilter，用于拦截xiugai_kjjkuang的焦点事件，实现：当输入框获得焦点时立即禁用动态数组itemHotkeys中所有的QHotkey *对象，失去焦点时恢复
    xiugai_kjjkuang.installEventFilter(new KjjHotkeyEditFilter(&xiugai_kjjkuang, itemHotkeys, &a)); // 创建事件过滤器对象，并把它安装到xiugai_kjjkuang上

    // 右键liebiao中的某个选项时，弹出一个菜单，上面有修改、删除两个选项
    QMenu menu1;
    menu1.setStyleSheet(g_quanjuQss); // 这个菜单没有父对象，得自己把样式表挂上
    QAction xiugai("修改", &menu1);
    menu1.addAction(&xiugai);
    QAction tianjia_after("在当前短语后添加短语", &menu1);
    menu1.addAction(&tianjia_after);
    QAction shanchu("删除", &menu1);
    menu1.addAction(&shanchu);
    liebiao.setContextMenuPolicy(Qt::CustomContextMenu); // 为liebiao设置自定义右键菜单
    // 当右键liebiao时，执行lambda表达式
    QObject::connect(&liebiao, &QListWidget::customContextMenuRequested,
        [&](const QPoint &pos) {
            QListWidgetItem *item = liebiao.itemAt(pos); // 根据点击位置，找到对应的QListWidgetItem（如果点到空白区域则返回空指针）
            if (item) { // 如果点到liebiao中的某个选项，那么弹出菜单，上面有修改、删除两个选项
                QAction *selectedAction = menu1.exec(liebiao.mapToGlobal(pos)); // 在鼠标点击的位置弹出菜单，等待用户选择一个QAction
                if (selectedAction == &xiugai) { // 如果用户选了“修改”
                    xiugaikuang.clear();
                    xiugai_beizhukuang.clear();
                    xiugai_kjjkuang.clear();
                    currentEditingItem = item; // 记录当前要修改的选项
                    xiugaikuang.setPlainText(item->data(Qt::UserRole).toString()); // 把原短语放进输入框
                    xiugai_beizhukuang.setPlainText(item->data(Qt::UserRole + 1).toString()); // 把原备注放进输入框
                    xiugai_kjjkuang.setKeySequence(QKeySequence(item->data(Qt::UserRole + 3).toString())); // 把这个短语项的Qt::UserRole+3里的快捷键字符串放进输入框
                    xiugaichuangkou.move(config["xiugaichuangkou_x"].toInt(), config["xiugaichuangkou_y"].toInt()); // 把xiugaichuangkou移动到记录的位置
                    xianshi(xiugaichuangkou);
                    xiugaikuang.setFocus(); // 把焦点给到xiugaikuang，而不是其他控件
                    xiugaikuang.selectAll(); // 全选输入框里的所有文本
                } else if (selectedAction == &tianjia_after) { // 如果用户选了“在当前短语后添加短语”
                    tianjiakuang.clear();
                    tianjia_beizhukuang.clear();
                    tianjia_kjjkuang.clear();
                    currentTabName = item->data(Qt::UserRole + 2).toString(); // 新短语使用被右键短语所在的分组
                    insertAfterRow = liebiao.row(item); // 记录要插入到哪一行后面
                    tianjiachuangkou.move(config["tianjiachuangkou_x"].toInt(), config["tianjiachuangkou_y"].toInt()); // 把tianjiachuangkou移动到记录的位置
                    xianshi(tianjiachuangkou);
                    tianjiakuang.setFocus(); // 把焦点给到tianjiakuang，而不是其他控件
                } else if (selectedAction == &shanchu) { // 如果用户选了“删除”
                    delete item;
                    saveListToJson(liebiao, dataPath); // 删除后写入列表内容到data.json
                    rebuildItemHotkeys(liebiao, itemHotkeys, &a); // 删除后为liebiao中的短语项注册快捷键
                }
            }
        });

    // 创建托盘
    QSystemTrayIcon *trayIcon = new QSystemTrayIcon(&a);
    trayIcon->setIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/软件图标.svg")); // 设置托盘图标
    // 点托盘会怎样
    // 左键单击：把主窗口叫出来。右键单击：弹出Windows原生右键菜单——不用QMenu，所以菜单是系统自己画的，外观和TrafficMonitor一致，也完全不受QuickSay全局QSS影响
    QObject::connect(trayIcon, &QSystemTrayIcon::activated,
        [&](QSystemTrayIcon::ActivationReason reason) { // reason变量可以用来接收托盘被激活的具体原因
            if (reason == QSystemTrayIcon::Trigger) { // 如果具体原因是鼠标左键单击
                chuangkou.move(config["chuangkou_x"].toInt(), config["chuangkou_y"].toInt());
                xianshi(chuangkou);
            } else if (reason == QSystemTrayIcon::Context) { // 如果具体原因是鼠标右键单击
                int xuanle = tanchuTuopanCaidan(tubiaoMulu); // 弹出原生菜单，等用户选完再返回
                if (xuanle == 1) dakaiShezhiChuangkou(); // “设置”
                else if (xuanle == 2) a.quit(); // “退出”
            }
        });
    trayIcon->show();
    trayIcon->setToolTip("QuickSay"); // 设置鼠标悬停在托盘上时显示的提示文字。这句代码必须写在show()之后

    // 使用自定义的事件过滤器类WindowMoveFilter，实现：当用户移动窗口时记录窗口位置。使得呼出窗口时让窗口在记录的位置显示
    a.installEventFilter(new WindowMoveFilter(&chuangkou, &shezhichuangkou, &tianjiachuangkou, &xiugaichuangkou, configPath, &a)); // 创建事件过滤器对象，并把它安装到a上
    // 使用自定义的事件过滤器类MyEventFilter，实现：Esc键可以关闭主窗口/添加窗口/修改窗口；回车键Enter可以输出光标处短语；左右方向键可以切换分组
    a.installEventFilter(new MyEventFilter(&chuangkou, &tianjiachuangkou, &xiugaichuangkou, &tianjiaquxiao, &xiugaiquxiao, &liebiao, &tabBar, &search)); // 创建事件过滤器对象，并把它安装到a上

    // 从全局对象config读取主窗口大小；其他窗口及其控件始终使用默认的500*500
    adjustAllWindows(config["width"].toInt(), config["height"].toInt(),
        chuangkou, liebiao, tabBar, search, shezhi, tianjia, tuding,
        tianjiachuangkou, tianjiakuang, tianjia_gaojishuru, tianjia_beizhuwenben, tianjia_beizhukuang, tianjia_kjjwenben, tianjia_kjjkuang, tianjia_kjjqingkong, tianjiaquxiao, tianjiaqueding,
        xiugaichuangkou, xiugaikuang, xiugai_gaojishuru, xiugai_beizhuwenben, xiugai_beizhukuang, xiugai_kjjwenben, xiugai_kjjkuang, xiugai_kjjqingkong, xiugaiquxiao, xiugaiqueding);
    // 设置窗口里“主窗口宽度/高度”真正生效的地方。之所以放在这里，是因为adjustAllWindows要用的那一堆控件到这时候才全都创建好了
    yingyongZhuchuangkouDaxiao = [&]() {
        adjustAllWindows(config["width"].toInt(), config["height"].toInt(),
            chuangkou, liebiao, tabBar, search, shezhi, tianjia, tuding,
            tianjiachuangkou, tianjiakuang, tianjia_gaojishuru, tianjia_beizhuwenben, tianjia_beizhukuang, tianjia_kjjwenben, tianjia_kjjkuang, tianjia_kjjqingkong, tianjiaquxiao, tianjiaqueding,
            xiugaichuangkou, xiugaikuang, xiugai_gaojishuru, xiugai_beizhuwenben, xiugai_beizhukuang, xiugai_kjjwenben, xiugai_kjjkuang, xiugai_kjjqingkong, xiugaiquxiao, xiugaiqueding);
    };

    if (!a.arguments().contains("--autostart")) { // 程序启动时检查程序启动参数，如果没有包含我们专门为开机自启添加的标记“--autostart”（也就是说用户是通过双击可执行文件打开的程序，而不是通过开机自启自动打开的程序）
        chuangkou.move(config["chuangkou_x"].toInt(), config["chuangkou_y"].toInt()); // 把chuangkou移动到记录的位置
        xianshi(chuangkou);
    }
    // 否则就是通过开机自启自动打开的程序，那么什么也不做，就后台运行个托盘

    // 启动后延时检查一次更新。开机自启的那份多等一会儿：开机时Windows还在忙着拉一堆自启动程序，网络也未必已经通了
    if (config["qidong_jiancha_gengxin"].toBool()) QTimer::singleShot(houtaiQidong ? 10000 : 3000, &a, []() { jianchaGengxin(false); });

    return a.exec();
}
