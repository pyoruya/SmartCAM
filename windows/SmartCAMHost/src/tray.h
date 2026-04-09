#pragma once
#include <windows.h>

namespace smartcam {

/// System tray icon management
class TrayIcon
{
public:
    bool Initialize(HINSTANCE hInstance);
    void SetStatus(const wchar_t* tooltip);
    void Shutdown();

private:
    HWND m_hwnd = nullptr;
    NOTIFYICONDATAW m_nid = {};
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
};

} // namespace smartcam
