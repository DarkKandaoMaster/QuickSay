#include "platform/nativeui.h"

#include "core/files.h"

#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QWidget>

#include <windows.h>
#include <dwmapi.h>

namespace WindowStyle {

void addNoActivate(WId window) {
    HWND hwnd = reinterpret_cast<HWND>(window);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_NOACTIVATE);
}

void setNoActivate(WId window, bool enabled) {
    HWND hwnd = reinterpret_cast<HWND>(window);
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (enabled) exStyle |= WS_EX_NOACTIVATE;
    else exStyle &= ~WS_EX_NOACTIVATE;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

bool isFrontmost(QWidget *widget) {
    HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) return true;
    const POINT center{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
    HWND top = WindowFromPoint(center);
    if (!top) return false;
    return GetAncestor(top, GA_ROOT) == hwnd;
}

void showNoActivate(WId window, bool topmost, WId insertAfter) {
    HWND hwnd = reinterpret_cast<HWND>(window);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    HWND after = topmost ? HWND_TOPMOST : (insertAfter ? reinterpret_cast<HWND>(insertAfter) : HWND_TOP);
    SetWindowPos(hwnd, after, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void setTransitionsDisabled(WId window, bool disabled) {
    BOOL value = disabled ? TRUE : FALSE;
    DwmSetWindowAttribute(reinterpret_cast<HWND>(window), DWMWA_TRANSITIONS_FORCEDISABLED, &value, sizeof(value));
}

} // namespace WindowStyle

namespace {

bool g_open = false;

enum : UINT {
    kSettingsCommand = 1,
    kQuitCommand = 2,
};

// 把 .ico 转成菜单项要的 32 位预乘 alpha 位图。size 已经是按 DPI 算好的物理像素，所以按 1 倍取图。
HBITMAP menuBitmap(const QString &iconPath, int size) {
    const QImage image = QIcon(iconPath).pixmap(QSize(size, size), 1.0).toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) return nullptr;
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = image.width();
    info.bmiHeader.biHeight = -image.height(); // 自上而下，和 QImage 行序一致
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap) return nullptr;
    memcpy(pixels, image.constBits(), static_cast<size_t>(image.sizeInBytes()));
    return bitmap;
}

// TrackPopupMenu 需要一个宿主窗口，弹出前还得把它设为前台，菜单才会在点别处时消失。
// 1×1、放在屏幕外的工具窗口，不进任务栏和 Alt+Tab；必须真的显示，否则 SetForegroundWindow 无效。
HWND menuHostWindow() {
    static HWND host = nullptr;
    if (host) return host;
    static const wchar_t *className = L"QuickSayTrayMenuHost";
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = className;
    RegisterClassW(&windowClass);
    host = CreateWindowExW(WS_EX_TOOLWINDOW, className, L"", WS_POPUP, -32000, -32000, 1, 1, nullptr, nullptr, windowClass.hInstance, nullptr);
    ShowWindow(host, SW_SHOWNA);
    return host;
}

void appendItem(HMENU menu, UINT id, const QString &text, HBITMAP bitmap) {
    AppendMenuW(menu, MF_STRING, id, reinterpret_cast<const wchar_t *>(text.utf16()));
    MENUITEMINFOW item = {};
    item.cbSize = sizeof(item);
    item.fMask = MIIM_BITMAP;
    item.hbmpItem = bitmap;
    SetMenuItemInfoW(menu, id, FALSE, &item);
}

} // namespace

namespace TrayMenu {

Choice exec() {
    HWND host = menuHostWindow();
    HMENU menu = CreatePopupMenu();
    const int iconSize = GetSystemMetrics(SM_CXSMICON); // 跟随系统 DPI，100% 缩放时是 16
    HBITMAP settingsBitmap = menuBitmap(Paths::icon("setting.ico"), iconSize);
    HBITMAP quitBitmap = menuBitmap(Paths::icon("exit.ico"), iconSize);
    appendItem(menu, kSettingsCommand, "设置", settingsBitmap);
    appendItem(menu, kQuitCommand, "退出", quitBitmap);

    POINT cursor;
    GetCursorPos(&cursor);
    SetForegroundWindow(host); // MSDN 要求：否则点菜单外面菜单不消失
    g_open = true;
    const int command = TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, cursor.x, cursor.y, 0, host, nullptr);
    g_open = false;
    PostMessageW(host, WM_NULL, 0, 0); // MSDN 要求：否则下次可能弹不出来
    DestroyMenu(menu);
    if (settingsBitmap) DeleteObject(settingsBitmap);
    if (quitBitmap) DeleteObject(quitBitmap);

    if (command == kSettingsCommand) return Choice::Settings;
    if (command == kQuitCommand) return Choice::Quit;
    return Choice::None;
}

bool isOpen() {
    return g_open;
}

} // namespace TrayMenu
