#include "tray.h"
#include <stdio.h>

namespace smartcam {

static TrayIcon* g_trayInstance = nullptr;

bool TrayIcon::Initialize(HINSTANCE hInstance, QuitCallback onQuit)
{
    m_hInstance = hInstance;
    m_onQuit = std::move(onQuit);
    g_trayInstance = this;

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"SmartCAMTrayWnd";
    RegisterClassExW(&wc);

    // Create hidden message window
    m_hwnd = CreateWindowExW(0, L"SmartCAMTrayWnd", L"SmartCAM",
                              0, 0, 0, 0, 0,
                              HWND_MESSAGE, nullptr, hInstance, nullptr);
    if (!m_hwnd) return false;

    // Add tray icon
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(m_nid.szTip, L"SmartCAM - Idle");

    Shell_NotifyIconW(NIM_ADD, &m_nid);

    printf("[Tray] Initialized\n");
    return true;
}

void TrayIcon::SetStatus(Status status, const wchar_t* detail)
{
    m_status = status;

    const wchar_t* statusStr = L"Idle";
    switch (status)
    {
    case Status::Idle:      statusStr = L"Idle"; break;
    case Status::Waiting:   statusStr = L"Waiting for phone..."; break;
    case Status::Connected: statusStr = L"Streaming"; break;
    case Status::Error:     statusStr = L"Error"; break;
    }

    if (detail)
        swprintf_s(m_nid.szTip, L"SmartCAM - %s: %s", statusStr, detail);
    else
        swprintf_s(m_nid.szTip, L"SmartCAM - %s", statusStr);

    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayIcon::Shutdown()
{
    Shell_NotifyIconW(NIM_DELETE, &m_nid);
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    g_trayInstance = nullptr;
}

void TrayIcon::ProcessMessages()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void TrayIcon::ShowContextMenu()
{
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    const wchar_t* statusStr = L"Idle";
    switch (m_status)
    {
    case Status::Idle:      statusStr = L"Status: Idle"; break;
    case Status::Waiting:   statusStr = L"Status: Waiting for phone"; break;
    case Status::Connected: statusStr = L"Status: Streaming"; break;
    case Status::Error:     statusStr = L"Status: Error"; break;
    }

    AppendMenuW(menu, MF_STRING | MF_GRAYED, IDM_STATUS, statusStr);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_QUIT, L"Quit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
    DestroyMenu(menu);
}

LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_TRAYICON && g_trayInstance)
    {
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_LBUTTONUP)
        {
            g_trayInstance->ShowContextMenu();
        }
        return 0;
    }

    if (msg == WM_COMMAND && g_trayInstance)
    {
        if (LOWORD(wParam) == IDM_QUIT)
        {
            if (g_trayInstance->m_onQuit)
                g_trayInstance->m_onQuit();
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace smartcam
