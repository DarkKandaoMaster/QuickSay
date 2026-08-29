# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目

QuickSay：Windows 上的快捷短语工具。Qt 6.5.3 + MinGW，qmake 构建。只支持 Windows 10/11，大量直接调用 Win32 API（`SendInput`、`WH_KEYBOARD_LL` 钩子、剪贴板、注册表 Run 项），不考虑跨平台。

## 构建与运行

工具链固定在 `D:/Programs/DevEnvironments/Qt/6.5.3/mingw_64` + `D:/Programs/DevEnvironments/Qt/Tools/mingw1120_64`。

```powershell
cd QuickSay
D:/Programs/DevEnvironments/Qt/6.5.3/mingw_64/bin/qmake.exe QuickSay.pro -spec win32-g++
D:/Programs/DevEnvironments/Qt/Tools/mingw1120_64/bin/mingw32-make.exe -j8
./QuickSay.exe
```

- **必须关闭 Shadow build**（`.pro` 里 `CONFIG -= debug_and_release`，Qt Creator 项目页也要取消影子构建）。exe 必须和 `icons/` 同级，否则图标全丢；`config.json` / `data.json` / `tab.json` 也写在 exe 同目录。
- 生成物（`Makefile`、`*.o`、`moc_*`、`ui_*.h`、`QuickSay.exe`、`build/`）全部被 `QuickSay/.gitignore` 忽略，不要提交。
- 没有测试，没有 lint。验证方式就是跑起来手测。

## 代码结构

**整个程序就是 [QuickSay/main.cpp](QuickSay/main.cpp) 一个文件（约 3000 行）**，UI 全部手写代码构造，没有 .ui 布局。

`.pro` 里列的 `mainwindow.cpp/h/ui` 是 Qt Creator 新建项目时的模板残留（被 gitignore，不在仓库里，克隆后第一次 qmake 前要自己建空壳，见 README 的复现步骤）。`MainWindow` 类没有任何人用，改代码不要碰它。

`QHotkey-1.5.0/` 和 `SingleApplication-3.5.3/` 是 vendored 的第三方库，通过 `.pri` include 进来，不要改。

### main.cpp 的分层（按行号顺序）

1. **全局状态**：`config`（QJsonObject，全部设置）+ 一堆 `g_` / `p` 裸指针（`pchuangkou` 主窗口、`g_liebiao`、`g_tabBar`、`g_search`、`g_tuding`、`g_keyboardHook`…）。所有窗口和控件都是 `main()` 里的栈对象，靠这些全局指针给自由函数和事件过滤器用。
2. **持久化**：`saveConfig`/`loadConfig`、`saveListToJson`/`loadListFromJson`、`saveTabToJson`/`loadTabFromJson`。
3. **输出流水线**：`parseQuickSayOutputActions` → `QVector<QuickSayOutputAction>` → `QuickSayOutputRunner`。
4. **键盘钩子 + 免激活窗口**：`quickSayKeyboardProc` / `handleQuickSayBrowseKey` / `showMainWindowNoActivate` / `enterSearchMode`。
5. **事件过滤器类**：`WindowMoveFilter`（记录窗口位置）、`MyEventFilter`（Esc/回车/左右键）、`PhraseItemToolTipFilter`、`HotkeyEditFilter` / `KjjHotkeyEditFilter`（编辑快捷键时临时注销全局快捷键）、`BadgeDelegate`（画角标）、`MyTabBar`（滚轮切分组）。
6. **`main()`**（1941 行起）：全局 QSS 样式表 → 主窗口 → 设置窗口 → 添加窗口 → 修改窗口 → 托盘 → 装过滤器 → `adjustAllWindows`。

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

**分组是用名字字符串关联的**，不是 id。改分组名要遍历所有短语项把 `UserRole+2` 一起改（`main.cpp` 2361 行附近），删分组要连带删该分组下的短语项。

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

## 风格

代码是照着「Qt 新手能读懂」写的：标识符大量用拼音（`chuangkou` 窗口、`liebiao` 列表、`shuchu` 输出、`shezhi` 设置、`tianjia` 添加、`xiugai` 修改、`tuding` 图钉、`jiaobiao` 角标、`kjj` 快捷键、`beizhu` 备注），几乎每行都有中文行尾注释解释「为什么」。**改代码时保持同样的注释密度和命名风格**，不要重构成英文命名或抽象出新层次。

`【【【注：...】】】` 标记的是「想改这里就改这一行」的调参点，`【【【【【` 标记的是作者留的待办，别顺手清掉。

发版时更新 [QuickSay/main.cpp](QuickSay/main.cpp) 开头的版本号和更新日志注释块，以及 README.md 里的下载链接版本号。

commit message 用中文，跟着现有风格走。
