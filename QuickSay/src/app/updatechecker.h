#pragma once

// 向 GitHub Releases API 要最新 release，和本地版本比大小。
// 请求用 WinHTTP 在后台线程里同步跑（整个程序只有这一处联网，不值得为它引入 Qt Network），结果抛回主线程弹窗。
namespace UpdateChecker {

// manual=false：启动后的自动检查，失败静默重试（10 秒后，连续失败 3 次放弃），被忽略的版本不提示。
// manual=true：设置里点“立即检查”，失败和“已是最新”都会告诉用户。
void check(bool manual);

} // namespace UpdateChecker
