# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目

QuickSay：Windows 上的快捷短语工具。Qt 6.5.3 + MinGW，qmake 构建。只支持 Windows 10/11，大量直接调用 Win32 API（`SendInput`、`WH_KEYBOARD_LL` 钩子、剪贴板、注册表 Run 项），不考虑跨平台。

## 构建与运行

工具链固定在 `D:/Programs/DevEnvironments/Qt/6.5.3/mingw_64` + `D:/Programs/DevEnvironments/Qt/Tools/mingw1120_64`。

```powershell
cd QuickSay
$env:PATH="D:\Programs\DevEnvironments\Qt\Tools\mingw1120_64\bin;D:\Programs\DevEnvironments\Qt\6.5.3\mingw_64\bin;$env:PATH"
D:/Programs/DevEnvironments/Qt/6.5.3/mingw_64/bin/qmake.exe QuickSay.pro -spec win32-g++
D:/Programs/DevEnvironments/Qt/Tools/mingw1120_64/bin/mingw32-make.exe -j8
./QuickSay.exe
```

- **PATH 里必须把 `mingw1120_64/bin` 排在最前面**。Makefile 调的是裸 `g++`，而这台机器上还装着另一套 MinGW（`D:/Program Files/Programming/C,C++/mingw64`，gcc 8.1.0）。撞上它的表现是在 `bits/fs_path.h` 里报一大堆 `no match for 'operator!='`、`'value_type' was not declared` 之类看不懂的模板错误——那不是代码问题，是编译器拿错了。
- **链接前先关掉正在运行的 QuickSay.exe**，否则 `ld.exe: cannot open output file QuickSay.exe: Permission denied`。
- **`.pro` 必须保持 CRLF 行尾**。存成 LF 之后 qmake 会把 `include(.\QHotkey-1.5.0\qhotkey.pri)` 解析坏，`INCPATH` 里变成 `-I/include`，编译报 `QHotkey: No such file or directory`。用脚本批量改 `.pro` / `main.cpp` 时，读写都要显式保住 CRLF（`main.cpp` 还带 UTF-8 BOM，也要一起保住）。
- **必须关闭 Shadow build**（`.pro` 里 `CONFIG -= debug_and_release`，Qt Creator 项目页也要取消影子构建）。exe 必须和 `icons/` 同级，否则图标全丢；`config.json` / `data.json` / `tab.json` 也写在 exe 同目录。
- 生成物（`Makefile`、`*.o`、`moc_*`、`ui_*.h`、`QuickSay.exe`、`build/`）全部被 `QuickSay/.gitignore` 忽略，不要提交。
- 没有测试，没有 lint。编译由 Claude 跑，功能验证靠作者手测，见下面「验证与测试」。

### 验证与测试

**编译由 Claude 来跑**：改完代码就照上面那段命令编一遍，别把语法错误留给作者。链接前直接 `taskkill /IM QuickSay.exe /F` 把正在跑的那个关掉——作者认这个代价，重开一下就是了。作者自己也会在 Qt Creator 里编，两边共用同一份 `Makefile` 和 `.o`（Shadow build 是关的）。

**功能验证只能作者手测。** UI 全是手写代码构造的，再加上低级键盘钩子、`SendInput`、剪贴板、UAC 弹窗这些东西，没法脚本化点击，也没有任何自动化测试可写可跑。

Claude 唯一能自己核对的是「进程级事实」——不点界面也能查到的那些。改自启、单实例、提权、持久化这类东西时值得核一遍：

| 想确认什么 | 怎么看 |
|---|---|
| 起没起来、有没有开出第二个实例 | `tasklist` 里 `QuickSay.exe` 有几个、PID 变没变 |
| 有没有弹 UAC | 弹窗那几秒里 `consent.exe` 在不在进程列表里 |
| 设置有没有落盘、老字段有没有迁移 | 直接读 exe 同目录的 `config.json` / `data.json` / `tab.json` |
| 开机自启的真实状态 | `reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v QuickSay`、`Get-ScheduledTask -TaskName 'QuickSay 管理员自启'` |

界面长什么样、快捷键灵不灵、短语输出对不对，一律交给作者手测，别自己下「应该没问题」的结论。

### 开发目录里跑不通的东西

开发目录里 exe 旁边**没有 Qt DLL**，Qt 的 bin 也**不在持久 PATH 里**（只有 Qt Creator、或上面那句 `$env:PATH` 临时注入）。所以凡是「绕过当前 shell、由 Windows 自己去拉起 QuickSay.exe」的功能，在开发目录里一定失败，退出码 `-1073741515`（`0xC0000135` STATUS_DLL_NOT_FOUND）：

- 开机自启（注册表 Run 项、计划任务）
- `ShellExecuteEx` + `runas` 提权重新拉起自己

**这不是代码 bug，别去改代码**。注意 `$env:PATH` 只在当前进程有效，提权（经 AppInfo 服务）和计划任务拉起的子进程都拿不到它。要手测这类功能，先 windeployqt 出一份完整副本，在副本上测：

```powershell
$d="$env:TEMP\qs-test"; mkdir $d -Force
copy QuickSay.exe $d; copy icons $d -Recurse
D:\Programs\DevEnvironments\Qt\6.5.3\mingw_64\bin\windeployqt.exe --no-translations --no-opengl-sw "$d\QuickSay.exe"
```

测完记得清干净：注册表 Run 项要还原成开发目录的路径（别留着临时目录的）、计划任务要删掉、临时目录要删掉——这些是真写进系统的，不是沙盒。

## 代码结构

**整个程序就是 [QuickSay/main.cpp](QuickSay/main.cpp) 一个文件（约 3500 行）**，UI 全部手写代码构造，没有 .ui 布局。

`.pro` 里列的 `mainwindow.cpp/h/ui` 是 Qt Creator 新建项目时的模板残留（被 gitignore，不在仓库里，克隆后第一次 qmake 前要自己建空壳，见 README 的复现步骤）。`MainWindow` 类没有任何人用，改代码不要碰它。

`QHotkey-1.5.0/` 是 vendored 的第三方库，通过 `.pri` include 进来，不要改。

### main.cpp 的分层（按行号顺序）

1. **全局状态**：`config`（QJsonObject，全部设置）+ 一堆 `g_` / `p` 裸指针（`pchuangkou` 主窗口、`g_liebiao`、`g_tabBar`、`g_search`、`g_tuding`、`g_keyboardHook`…）。所有窗口和控件都是 `main()` 里的栈对象，靠这些全局指针给自由函数和事件过滤器用。
2. **持久化**：`saveConfig`/`loadConfig`、`saveListToJson`/`loadListFromJson`、`saveTabToJson`/`loadTabFromJson`。
3. **开机自启**：`applyZiqidong` + 计划任务那一套 COM 调用，见下面「开机自启」一节。
4. **输出流水线**：`parseQuickSayOutputActions` → `QVector<QuickSayOutputAction>` → `QuickSayOutputRunner`。
5. **键盘钩子 + 免激活窗口**：`quickSayKeyboardProc` / `handleQuickSayBrowseKey` / `showMainWindowNoActivate` / `enterSearchMode`。
6. **事件过滤器类**：`WindowMoveFilter`（记录窗口位置）、`MyEventFilter`（Esc/回车/左右键）、`PhraseItemToolTipFilter`、`HotkeyEditFilter` / `KjjHotkeyEditFilter`（编辑快捷键时临时注销全局快捷键）、`BadgeDelegate`（画角标）、`MyTabBar`（滚轮切分组）。
7. **`main()`**（2297 行起，行号会漂，用 `grep -n "^int main"` 找）：全局 QSS 样式表 → 主窗口 → 设置窗口 → 添加窗口 → 修改窗口 → 托盘 → 装过滤器 → `adjustAllWindows`。

### 几个必须知道的约定

**短语项数据存在 `Qt::UserRole` 偏移里**，改动任何一处都要同步 `saveListToJson`/`loadListFromJson`：

| 角色 | 内容 | data.json 字段 |
|---|---|---|
| `UserRole` | 短语正文 | `text` |
| `UserRole+1` | 备注（非空则代替短语显示，绿色） | `remark` |
| `UserRole+2` | 所属分组名 | `tab` |
| `UserRole+3` | 短语快捷键字符串 | `hotkey` |
| `UserRole+4` | 角标字符（运行期算出，不落盘） | — |

分组的短语项高度塞在 `QTabBar::tabData()` 里（`tab.json` 的 `item_height`）。

**分组是用名字字符串关联的**，不是 id。改分组名要遍历所有短语项把 `UserRole+2` 一起改（`main.cpp` 2732 行附近，行号会漂，用 `grep -n "UserRole+2,newName"` 找），删分组要连带删该分组下的短语项。

**过滤/角标只有一个入口**：`filterListByTab(liebiao, 当前分组名, 搜索框文字)`。任何会改变可见项集合的操作（切分组、搜索、拖动排序、增删改）之后都必须调它一次，否则角标错位。

**布局是绝对坐标**，全部集中在 `adjustAllWindows()`。加控件就在那个函数里加一行 `move` + `setFixedSize`，不要引入 layout（除了设置窗口的 `QFormLayout`）。

**兼容旧版本的方式**：`loadConfig` 里逐个 `if(!config.contains("xxx")) config["xxx"]=默认值`；启动时 load 完立刻 save 一次把新字段落盘。加新设置项照抄这个模式，同时在 `loadConfig` 的 else 分支（config.json 不存在）里补上默认值——两处都要写。

**术语**：主窗口左上角用来归类短语的选项卡叫「**分组**」（代码里变量名仍是 `tab`/`tabBar`）；「标签」专指短语正文里的 `<Enter>` 这类高级输入标签，两者不要混。

### 输出流水线细节

`shuchu()` → `startQuickSayOutput(text)` → `parseQuickSayOutputActions` 把短语切成 Text / Press / Sleep / Image 四种 action，交给 `QuickSayOutputRunner` 用 `QTimer::singleShot` 串行执行（间隔 = `config["delay"]`）。

标签语法在 `parseQuickSayTag`：`<Enter>` `<Tab>` `<Ctrl+C>` 之类的按键、`<press X>`、`<sleep>` / `<sleep 500>` / `<sleep 1.5s>`、`<img 绝对路径>` / `<file 绝对路径>`；`\<` 转义；解析失败的标签当普通文字原样输出。

几条踩过坑的硬约束，改这块前先看懂注释：
- 写剪贴板后要等 50ms 再 `moniCtrlV()`，否则目标程序读到旧内容 → 粘成空行。
- `<img>` 前额外等 1000ms（微信 bug）。
- 输出开始时 `releasePressedPhysicalModifiers()` 只抬起用户按着的修饰键，**结束后绝不按回去**——按回去会永久卡键。
- Press 动作期间 `beginQuickSayPressBlock()` 挡住自家键盘钩子，避免模拟出来的按键被自己当成用户输入。

### 窗口激活模型

主窗口默认带 `WS_EX_NOACTIVATE` + `SW_SHOWNOACTIVATE`，**不抢前台焦点**，所以点短语能直接往前台程序里粘。代价是主窗口收不到正常键盘事件，键盘浏览（角标键、↑↓←→、Enter、Esc、`` ` ``、Tab）全靠低级键盘钩子 `quickSayKeyboardProc` 拦截。

搜索框是唯一例外：`enterSearchMode()` 会摘掉 `WS_EX_NOACTIVATE`、卸掉钩子、真正激活窗口；`leaveSearchMode()` 反向恢复并把前台还给 `g_lastForegroundBeforeSearch`。加任何新的「需要真正打字」的控件都得走这套。

钩子里加新按键处理时注意 `hasQuickSayBlockingWindow()`：设置/添加/修改窗口或弹出菜单打开时必须放行按键。

### 开机自启 / 以管理员权限启动

config 里两个开关**互相独立**：`ziqidong`（开机自启动）、`guanliyuan`（以管理员权限启动）。老版本的 `ziqidong_guanliyuan` 在 `loadConfig` 里迁移成 `guanliyuan` 后删掉。

| ziqidong | guanliyuan | 系统里留下什么 |
|---|---|---|
| 关 | 关 | 什么都没有 |
| 关 | 开 | 什么都没有；手动启动时自己提权 |
| 开 | 关 | 注册表 `HKCU\...\CurrentVersion\Run` 的 `QuickSay` 键，值是 `"exe路径" --autostart` |
| 开 | 开 | 计划任务「QuickSay 管理员自启」（根目录），LogonTrigger + `RunLevel=HighestAvailable` |

开机自启这一路由 `applyZiqidong(bufanrao)` 收口（`main.cpp` 开头「开机自启动」那一节）：

- **注册表项和计划任务绝不能同时存在**，否则开机会启动两个实例。`applyZiqidong` 每次都会把另一条清掉。
- 建计划任务需要管理员权限，所以 `createAdminRenwuWithUac()` 用 `ShellExecuteEx`+`runas` 把自己再拉一份带 `--create-admin-task` 的提权实例，那份只建任务就 `return`。**这个参数必须在单实例检测之前处理掉**，否则会被单实例检测拦下来。
- 任务的 SDDL 除了 SY/BA 还额外给当前用户 SID 完全控制，所以**删任务不需要提权**——关闭这个选项时不会弹第二次 UAC。这是抄 PowerToys `auto_start_helper.cpp` 的关键一笔，别删。
- MinGW 的 `taskschd.h` 里**没有 `ILogonTrigger`**（也没有 `IPrincipal` 之外的一堆触发器接口），所以是拼一整份任务 XML 丢给 `ITaskFolder::RegisterTask`，而不是像 PowerToys 那样一个个接 COM 接口。改任务定义就改那段 XML 字符串。
- 启动时调的是 `applyZiqidong(true)`（不打扰模式）：**它自己绝不弹 UAC**。真需要提权时，提权早在 `main()` 开头就做完了，那份提权实例跑到这里就能不弹窗地把任务补上；没提权就先不建，退回注册表 Run 项，留到下次启动再修。设置窗口里改开关时调 `applyZiqidong(false)`，那次才允许弹 UAC。

以管理员权限启动这一路（`main.cpp`「以管理员权限启动」那一节）：

- 只有管理员权限的 QuickSay 才能往同样是管理员权限的窗口里输入（以管理员权限运行的记事本、任务管理器之类），这就是这个选项的意义。
- 手动启动时 `duGuanliyuanKaiguan()` 直接从 config.json 里读 `guanliyuan`（那时还没有 `QApplication`，用不了 `loadConfig`——config.json 不存在时它要拿屏幕尺寸算默认窗口位置），开着而自己没提权就 `tiquanChongqiZishen()` 用 `runas` 重开一份，本份 `return 0`。**每次手动启动都要过一次 UAC**，这是这个选项的代价。
- 顺序卡得很死：`--create-admin-task` → 单实例检测 → 提权重启 → `QApplication`。放在单实例检测**之后**，是因为 QuickSay 已经在跑时双击 exe 只是想把窗口叫出来，不该再弹一次 UAC；提权之前必须 `shifangDanshili()` 让出互斥体，否则新起来的那份会被单实例检测拦下，结果谁也没起来。
- 带 `--autostart` 的实例**绝不走这条路**：开机时由计划任务直接以管理员权限起来，一次 UAC 都不弹；万一任务没建成、退回了注册表 Run 项，那也宁可这次以普通权限跑，绝不在开机时弹 UAC。
- 普通权限进程同时打开了「开机自启动」时，勾选管理员选项已经为建计划任务弹过一次 UAC；随后点「立即重启」会用 `yongRenwuChongqiGuanliyuan()` 直接运行这份已授权任务，不再弹第二次 UAC。任务参数仍是 `--autostart`，所以旧进程会预先置位 `QuickSay_ShowWindow` 事件，让新实例进入事件循环后显示主窗口。

### 单实例

`tongzhiYiyouShili()`（`main.cpp` 开头「单实例检测」那一节），在 `main()` 里**必须抢在 `QApplication` 构造之前**调用：后启动的那个实例要在什么 Qt 对象都还没创建的时候就 `return 0`。

- 命名互斥体 `Local\QuickSay_SingleInstance` 抢占，`GetLastError()==ERROR_ALREADY_EXISTS` 就说明已经有一个 QuickSay 在跑了；命名事件 `Local\QuickSay_ShowWindow` 用来通知先起来的那个实例把主窗口显示出来，第一个实例用 `QWinEventNotifier` 监听它。两个句柄平时都不用关（进程退出时系统自动回收），只有提权重启自己之前要 `shifangDanshili()` 主动关掉，好让新起来的那份抢得到互斥体。
- 两个内核对象的安全描述符是 `D:(A;;GA;;;WD)S:(ML;;NW;;;LW)`。**`S:` 那半句（把对象完整性标签降到 Low）绝不能删**：不加的话，管理员权限跑着的 QuickSay 建出来的是高完整性对象，后面手动双击 exe 起来的中完整性实例根本访问不了，检测就失效、开出第二个实例。这正是原来 `SingleApplication` 的毛病——它的 `QSharedMemory` / `QLocalServer` 是默认安全描述符，跨完整性级别连不上，所以整个换掉了。
- 事件是**手动重置**的，处理完必须 `ResetEvent()`，否则会一直触发。

## 风格

代码是照着「Qt 新手能读懂」写的：标识符大量用拼音（`chuangkou` 窗口、`liebiao` 列表、`shuchu` 输出、`shezhi` 设置、`tianjia` 添加、`xiugai` 修改、`tuding` 图钉、`jiaobiao` 角标、`kjj` 快捷键、`beizhu` 备注），几乎每行都有中文行尾注释解释「为什么」。**改代码时保持同样的注释密度和命名风格**，不要重构成英文命名或抽象出新层次。

`【【【注：...】】】` 标记的是「想改这里就改这一行」的调参点，`【【【【【` 标记的是作者留的待办，别顺手清掉。

发版时更新 [QuickSay/main.cpp](QuickSay/main.cpp) 开头的版本号和更新日志注释块，以及 README.md 里的下载链接版本号。

commit message 用中文，跟着现有风格走。
