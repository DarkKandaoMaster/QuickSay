// 版本：2.1.0
// 更新内容：
// 1. 注意：设置和短语改为保存在程序目录下的 data 文件夹里，首次启动时自动从旧文件迁移。旧版本读不了新的数据文件和新版导出的备份，因此升级后无法再退回旧版本；旧版本导出的备份仍可导入。
// 2. 新增“把选中的文字存为新短语”全局快捷键，默认不启用。
// 3. 短语右键菜单新增“选取文字”，可以用鼠标拖选短语的一部分来输入。
// 4. 分组右键菜单新增“批量添加短语”，可以按行添加多条短语。
// 5. 短语右键菜单新增“移动到分组”。
// 6. 快捷键支持并区分主键盘按键与小键盘按键。
// 7. <press> 支持 = - [ ] ; ' , . / \ ` 这些标点键，例如 <press Ctrl+Shift+=> 。
// 8. 新增“高级输入过程中切换到了别的程序，就停止输入”设置选项，默认开启。
// 9. 鼠标悬停提示支持显示短语里 <img> 的图片。
// 10. 设置里新增“实验”选项卡，新增实验功能“呼出主窗口后直接进入搜索”。
// 11. 优化了“批量编辑短语”的使用体验，删除了一些限制。
// 12. 鼠标悬停提示不再自动消失。新增“鼠标悬停多久后显示提示”设置选项，默认立即显示（以前固定 250 毫秒）。
// 13. 添加/修改短语窗口里按 Ctrl+回车 等同于点“添加”/“修改”。
// 14. 主窗口和分组面板呼出时不再播放弹出动画；分组面板收起时新增关闭动画。
// 15. 分组面板改为圆角（仅 Windows 11）。
// 16. 修复主窗口搜索框和新建/修改分组窗口里，输入中文时字的下半截被截断的问题。
// 17. 修复用左右方向键切换短语时，列表总会把所在行滚动到正中间的问题。
// 18. 多列分组里，在第一行按上方向键、在最后一行按下方向键时，不再跳到第一条/最后一条短语。
// 19. 修复分组面板有时不收起的问题。
// 20. 修复取消“主窗口始终置顶”后，按呼出快捷键有时不能把主窗口拉到最前的问题。
// 21. 修复呼出快捷键设为 Ctrl+` 时，主窗口显示后按它不能关闭窗口、反而切换了钉住状态的问题。
// 22. 修复分组名里带 & 时分组按钮显示不正确的问题。
// 23. 修复极端情况下（比如修改分组名时程序被强行结束）部分短语再也显示不出来的问题。

#include "app/quicksayapp.h"
#include "core/files.h"
#include "core/settings.h"
#include "platform/startup.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    bool autostarted = false; // 注册表 Run 项和计划任务都会带上 --autostart
    for (int i = 1; i < argc; ++i) {
        // 提权去建计划任务的那份实例：建完就退出，退出码告诉等着它的实例成没成。
        // 必须在单实例检测之前处理，否则会被当成第二个实例拦下
        if (QString(argv[i]) == "--create-admin-task") return AutoStart::createAdminTask() ? 0 : 1;
        if (QString(argv[i]) == "--autostart") autostarted = true;
    }

    // 必须在 QApplication 之前：后启动的实例要在什么 Qt 对象都还没建的时候就退出
    if (SingleInstance::notifyIfRunning()) return 0;

    // 2.1.0 以前的数据文件搬进 data/。要赶在下面读“以管理员权限启动”之前
    DataFile::migrateLegacyFiles();

    // “以管理员权限启动”：手动启动时自己提权重开一份。放在单实例检测之后——已经在跑时双击 exe 只是想叫出窗口，不该弹 UAC。
    // 开机自启的那份绝不走这里：要么由计划任务直接以管理员权限起来，要么宁可这次以普通权限跑，也不在开机时弹 UAC
    if (Settings::readRunAsAdminFromFile() && !Elevation::isProcessElevated() && !autostarted) {
        SingleInstance::release(); // 先让出互斥体，否则提权的那份会被单实例检测拦下
        if (Elevation::relaunchElevated()) return 0;
        SingleInstance::notifyIfRunning(); // 用户拒绝了 UAC：抢回互斥体，以普通权限接着跑
    }

    QApplication app(argc, argv);
    QuickSayApp quickSay(autostarted);
    return app.exec();
}
