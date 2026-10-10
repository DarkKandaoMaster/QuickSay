#include "platform/startup.h"

#include "core/files.h"
#include "core/settings.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QXmlStreamReader>

#include <windows.h>
#include <sddl.h>
#include <shellapi.h>
#include <shldisp.h>
#include <taskschd.h>

namespace {

const wchar_t *const kMutexName = L"Local\\QuickSay_SingleInstance";
const wchar_t *const kShowEventName = L"Local\\QuickSay_ShowWindow";

HANDLE g_mutex = nullptr;
HANDLE g_showEvent = nullptr;

// “所有人完全放行”+“完整性标签降到 Low”。后半句 S:(ML;;NW;;;LW) 绝不能删：
// 不降标签的话，中完整性进程打不开管理员实例建的对象，单实例检测就失效了。
template <typename Create>
HANDLE createWithLowIntegrity(Create create) {
    PSECURITY_DESCRIPTOR sd = nullptr;
    ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GA;;;WD)S:(ML;;NW;;;LW)", SDDL_REVISION_1, &sd, nullptr);
    SECURITY_ATTRIBUTES sa = {sizeof(sa), sd, FALSE};
    HANDLE handle = create(sd ? &sa : nullptr);
    if (sd) LocalFree(sd);
    return handle;
}

} // namespace

namespace SingleInstance {

bool notifyIfRunning() {
    bool alreadyRunning = false;
    g_mutex = createWithLowIntegrity([&alreadyRunning](SECURITY_ATTRIBUTES *sa) {
        HANDLE mutex = CreateMutexW(sa, FALSE, kMutexName);
        alreadyRunning = GetLastError() == ERROR_ALREADY_EXISTS;
        return mutex;
    });
    // 手动重置事件，属性由第一个实例决定
    g_showEvent = createWithLowIntegrity([](SECURITY_ATTRIBUTES *sa) {
        return CreateEventW(sa, TRUE, FALSE, kShowEventName);
    });
    if (alreadyRunning && g_showEvent) SetEvent(g_showEvent);
    return alreadyRunning;
}

void release() {
    if (g_mutex) {
        CloseHandle(g_mutex);
        g_mutex = nullptr;
    }
    if (g_showEvent) {
        CloseHandle(g_showEvent);
        g_showEvent = nullptr;
    }
}

void *showWindowEvent() {
    return g_showEvent;
}

void *presignalShowWindow() {
    return createWithLowIntegrity([](SECURITY_ATTRIBUTES *sa) {
        return CreateEventW(sa, TRUE, TRUE, kShowEventName);
    });
}

void waitForNewInstance() {
    // 最多等 30 秒，正常几十毫秒。新实例先建互斥体、紧接着打开显示事件，抢到互斥体后再多等 50ms 补上这个缝
    for (int i = 0; i < 3000; ++i) {
        HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, kMutexName);
        if (mutex) {
            CloseHandle(mutex);
            Sleep(50);
            return;
        }
        Sleep(10);
    }
}

} // namespace SingleInstance

namespace Elevation {

bool isProcessElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation = {};
    DWORD length = 0;
    const bool elevated = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &length) && elevation.TokenIsElevated;
    CloseHandle(token);
    return elevated;
}

bool relaunchElevated() {
    wchar_t exe[MAX_PATH] = {0};
    Paths::executable().toWCharArray(exe);
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC; // 本进程紧接着就退出，要等 ShellExecuteEx 把事做完
    info.lpVerb = L"runas";
    info.lpFile = exe;
    info.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&info);
}

bool createAdminTaskWithUac() {
    if (isProcessElevated()) return AutoStart::createAdminTask();
    wchar_t exe[MAX_PATH] = {0};
    Paths::executable().toWCharArray(exe);
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = exe;
    info.lpParameters = L"--create-admin-task"; // 提权实例看到它只建任务，建完立即退出
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info) || !info.hProcess) return false;
    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    return code == 0;
}

bool launchViaExplorer(QString &error) {
    const QString exePath = Paths::executable(); // 不带 --autostart：新实例要按一次正常手动启动走完提权和自启处理
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IShellDispatch2 *shell = nullptr;
    // Shell.Application 跑在普通权限的 Explorer 里，由它启动就不会继承当前进程的管理员令牌
    HRESULT hr = CoCreateInstance(CLSID_Shell, nullptr, CLSCTX_LOCAL_SERVER, IID_IShellDispatch2, reinterpret_cast<void **>(&shell));
    if (SUCCEEDED(hr) && shell) {
        BSTR file = SysAllocString(reinterpret_cast<const OLECHAR *>(exePath.utf16()));
        VARIANT empty, show;
        VariantInit(&empty);
        VariantInit(&show);
        show.vt = VT_I4;
        show.lVal = SW_SHOWNORMAL;
        hr = shell->ShellExecute(file, empty, empty, empty, show);
        SysFreeString(file);
        shell->Release();
    }
    if (SUCCEEDED(init)) CoUninitialize();
    if (SUCCEEDED(hr)) return true;

    // COM 外壳偶尔不可用时，仍然把启动请求交给 explorer.exe
    const QString parameters = QString("\"%1\"").arg(QDir::toNativeSeparators(exePath));
    HINSTANCE result = ShellExecuteW(nullptr, L"open", L"explorer.exe", reinterpret_cast<const wchar_t *>(parameters.utf16()), nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) > 32) return true;
    error = QString("Windows Explorer 启动失败（错误 %1）").arg(reinterpret_cast<INT_PTR>(result));
    return false;
}

} // namespace Elevation

namespace {

const QString kRunKeyPath = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const QString kRunValueName = "QuickSay";
const wchar_t *const kTaskName = L"QuickSay 管理员自启"; // 建在任务计划程序库根目录

// Qt 已在主线程初始化过 COM，这里通常拿到 S_FALSE 或 RPC_E_CHANGED_MODE，两者都能用；
// 只有真正初始化成功才配对 CoUninitialize。
class ComScope {
  public:
    ComScope() : hr_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComScope() {
        if (SUCCEEDED(hr_)) CoUninitialize();
    }
    bool usable() const {
        return SUCCEEDED(hr_) || hr_ == RPC_E_CHANGED_MODE;
    }

  private:
    HRESULT hr_;
};

QString runValue() {
    return QString("\"%1\" --autostart").arg(Paths::executable());
}

// 「域\用户名」：登录触发器和运行身份都用它，任务只在这个用户登录时触发
QString currentUserName() {
    wchar_t name[256] = {0};
    wchar_t domain[256] = {0};
    GetEnvironmentVariableW(L"USERNAME", name, 256);
    GetEnvironmentVariableW(L"USERDOMAIN", domain, 256);
    return QString::fromWCharArray(domain) + "\\" + QString::fromWCharArray(name);
}

// System 和管理员组完全控制，另外给当前用户完全控制——这样普通权限的 QuickSay 自己就能删改任务，
// 关闭管理员自启时不用再弹 UAC。抄自 PowerToys 的 auto_start_helper.cpp，别删。
QString taskSddl() {
    QString sddl = "D:(A;;FA;;;SY)(A;;FA;;;BA)";
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        BYTE info[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE] = {0};
        DWORD length = 0;
        if (GetTokenInformation(token, TokenUser, info, sizeof(info), &length)) {
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

// MinGW 的 taskschd.h 没有 ILogonTrigger，所以直接喂整份任务 XML 给 RegisterTask。改任务定义就改这里。
QString taskXml(const QString &user) {
    const QString exe = Paths::executable();
    const QString workingDir = QFileInfo(exe).absolutePath().replace('/', '\\');
    return QString(
        "<?xml version=\"1.0\" encoding=\"UTF-16\"?>"
        "<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">"
        "<RegistrationInfo><Author>%1</Author><Description>QuickSay 以管理员权限开机自启</Description></RegistrationInfo>"
        // 延迟 3 秒：登录瞬间 explorer 还没起来，托盘图标加不上去
        "<Triggers><LogonTrigger><Enabled>true</Enabled><UserId>%1</UserId><Delay>PT3S</Delay></LogonTrigger></Triggers>"
        "<Principals><Principal id=\"Author\"><UserId>%1</UserId><LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel></Principal></Principals>"
        "<Settings>"
        "<MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>"
        "<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>" // 这两项默认都是 true，笔记本用电池时会不启动/被结束
        "<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>"
        "<AllowHardTerminate>false</AllowHardTerminate>"
        "<StartWhenAvailable>false</StartWhenAvailable>"
        "<AllowStartOnDemand>true</AllowStartOnDemand>"
        "<Enabled>true</Enabled>"
        "<Hidden>false</Hidden>"
        "<RunOnlyIfIdle>false</RunOnlyIfIdle>"
        "<WakeToRun>false</WakeToRun>"
        "<ExecutionTimeLimit>PT0S</ExecutionTimeLimit>" // 不限运行时长，否则开久了会被计划任务结束
        "<Priority>4</Priority>"
        "</Settings>"
        "<Actions Context=\"Author\"><Exec><Command>%2</Command><Arguments>--autostart</Arguments><WorkingDirectory>%3</WorkingDirectory></Exec></Actions>"
        "</Task>")
        .arg(user.toHtmlEscaped())
        .arg(exe.toHtmlEscaped())
        .arg(workingDir.toHtmlEscaped());
}

// 连上计划任务服务并拿到根目录。成功时 *service 和返回值都要 Release。
ITaskFolder *openTaskRoot(ITaskService **service) {
    *service = nullptr;
    ITaskService *svc = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, reinterpret_cast<void **>(&svc)))) return nullptr;
    VARIANT empty;
    VariantInit(&empty); // 四个空 VARIANT：本机、当前用户
    if (FAILED(svc->Connect(empty, empty, empty, empty))) {
        svc->Release();
        return nullptr;
    }
    ITaskFolder *folder = nullptr;
    BSTR root = SysAllocString(L"\\");
    const HRESULT hr = svc->GetFolder(root, &folder);
    SysFreeString(root);
    if (FAILED(hr)) {
        svc->Release();
        return nullptr;
    }
    *service = svc;
    return folder;
}

// 打开根目录后把 folder 交给 fn，统一负责 COM 初始化和释放
template <typename Fn>
bool withTaskRoot(Fn fn) {
    ComScope com;
    if (!com.usable()) return false;
    ITaskService *svc = nullptr;
    ITaskFolder *folder = openTaskRoot(&svc);
    bool result = false;
    if (folder) {
        result = fn(folder);
        folder->Release();
    }
    if (svc) svc->Release();
    return result;
}

IRegisteredTask *getTask(ITaskFolder *folder) {
    IRegisteredTask *task = nullptr;
    BSTR name = SysAllocString(kTaskName);
    const HRESULT hr = folder->GetTask(name, &task);
    SysFreeString(name);
    return SUCCEEDED(hr) ? task : nullptr;
}

// 任务在、启用着、而且记的还是程序现在的位置
bool adminTaskIsCurrent() {
    QString taskExe;
    return AutoStart::queryAdminTask(&taskExe) && taskExe.compare(Paths::executable(), Qt::CaseInsensitive) == 0;
}

} // namespace

namespace AutoStart {

bool queryAdminTask(QString *exePath) {
    if (exePath) exePath->clear();
    return withTaskRoot([exePath](ITaskFolder *folder) {
        IRegisteredTask *task = getTask(folder);
        if (!task) return false;
        VARIANT_BOOL enabled = VARIANT_FALSE;
        const bool isEnabled = SUCCEEDED(task->get_Enabled(&enabled)) && enabled == VARIANT_TRUE;
        BSTR xml = nullptr;
        // 用 XML 解析器读 <Command>，路径里的 &amp; 之类实体才能还原
        if (exePath && SUCCEEDED(task->get_Xml(&xml)) && xml) {
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
        return isEnabled;
    });
}

bool deleteAdminTask() {
    return withTaskRoot([](ITaskFolder *folder) {
        BSTR name = SysAllocString(kTaskName);
        const HRESULT hr = folder->DeleteTask(name, 0);
        SysFreeString(name);
        return SUCCEEDED(hr) || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND); // 本来就没有也算成功
    });
}

bool createAdminTask() {
    const QString user = currentUserName();
    const QString xml = taskXml(user);
    return withTaskRoot([&](ITaskFolder *folder) {
        BSTR name = SysAllocString(kTaskName);
        BSTR xmlText = SysAllocString(reinterpret_cast<const wchar_t *>(xml.utf16()));
        VARIANT userId;
        VariantInit(&userId);
        userId.vt = VT_BSTR;
        userId.bstrVal = SysAllocString(reinterpret_cast<const wchar_t *>(user.utf16()));
        VARIANT sddl;
        VariantInit(&sddl);
        sddl.vt = VT_BSTR;
        sddl.bstrVal = SysAllocString(reinterpret_cast<const wchar_t *>(taskSddl().utf16()));
        VARIANT password;
        VariantInit(&password); // InteractiveToken 不需要存密码
        IRegisteredTask *task = nullptr;
        const HRESULT hr = folder->RegisterTask(name, xmlText, TASK_CREATE_OR_UPDATE, userId, password, TASK_LOGON_INTERACTIVE_TOKEN, sddl, &task);
        if (task) task->Release();
        VariantClear(&userId);
        VariantClear(&sddl);
        SysFreeString(xmlText);
        SysFreeString(name);
        return SUCCEEDED(hr);
    });
}

bool runAdminTaskNow() {
    // 任务参数是 --autostart，新实例本来只进托盘；预先置位显示事件，它进入事件循环后就会显示主窗口
    HANDLE showRequest = SingleInstance::presignalShowWindow();
    if (!showRequest) return false;

    const bool started = withTaskRoot([](ITaskFolder *folder) {
        IRegisteredTask *task = getTask(folder);
        if (!task) return false;
        VARIANT empty;
        VariantInit(&empty);
        IRunningTask *running = nullptr;
        const bool ok = SUCCEEDED(task->Run(empty, &running)); // 由任务计划程序启动，直接继承 HighestAvailable
        if (running) running->Release();
        task->Release();
        return ok;
    });

    // 等新实例抢到互斥体再关事件句柄，否则旧进程退得太快，事件对象可能在新实例打开它之前就没了
    if (started) SingleInstance::waitForNewInstance();
    CloseHandle(showRequest);
    return started;
}

bool isEnabledInSystem() {
    QSettings reg(kRunKeyPath, QSettings::NativeFormat);
    return adminTaskIsCurrent() || reg.contains(kRunValueName);
}

bool apply(bool quiet) {
    QSettings reg(kRunKeyPath, QSettings::NativeFormat);
    if (!Settings::autoStart()) {
        // 计划任务删不掉就保持原状，不能假装已关闭
        if (!deleteAdminTask()) return false;
        reg.remove(kRunValueName);
        return true;
    }
    if (!Settings::runAsAdmin()) {
        if (!deleteAdminTask()) return false;
        if (reg.value(kRunValueName).toString() != runValue()) reg.setValue(kRunValueName, runValue());
        return true;
    }

    reg.remove(kRunValueName);
    if (adminTaskIsCurrent()) return true;
    // 任务不存在、被禁用，或程序挪过位置导致路径过期：都要重建
    if ((!quiet || Elevation::isProcessElevated()) && Elevation::createAdminTaskWithUac()) return true;
    // 没建成（不打扰模式下没提权，或用户拒绝了 UAC）：先退回普通权限自启，别让用户开机什么都没有。
    // “以管理员权限启动”保持开着，下次以管理员身份运行时会不弹 UAC 地把任务补上。
    reg.setValue(kRunValueName, runValue());
    return false;
}

} // namespace AutoStart
