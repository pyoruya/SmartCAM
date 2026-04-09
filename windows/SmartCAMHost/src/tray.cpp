#include "tray.h"
#include <shellapi.h>

namespace smartcam {

// TODO: Implement tray icon initialization, status updates, context menu

bool TrayIcon::Initialize(HINSTANCE hInstance)
{
    // TODO: Create hidden window, register NOTIFYICONDATA
    return false;
}

void TrayIcon::SetStatus(const wchar_t* tooltip)
{
    // TODO: Update tray icon tooltip
}

void TrayIcon::Shutdown()
{
    // TODO: Remove tray icon, destroy window
}

LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // TODO: Handle tray icon messages (click, context menu)
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace smartcam
