#pragma once
/// System tray icon for SmartCAM host.

#include <windows.h>
#include <shellapi.h>
#include <functional>

namespace smartcam {

class TrayIcon
{
public:
    enum class Status { Idle, Waiting, Connected, Error };

    using QuitCallback = std::function<void()>;

    bool Initialize(HINSTANCE hInstance, QuitCallback onQuit);
    void SetStatus(Status status, const wchar_t* detail = nullptr);
    void Shutdown();

    /// Process Windows messages. Call from message loop.
    void ProcessMessages();

    HWND GetHwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void ShowContextMenu();

    HWND              m_hwnd = nullptr;
    NOTIFYICONDATAW   m_nid = {};
    HINSTANCE         m_hInstance = nullptr;
    QuitCallback      m_onQuit;
    Status            m_status = Status::Idle;

    static constexpr UINT WM_TRAYICON = WM_APP + 1;
    static constexpr UINT IDM_QUIT = 1001;
    static constexpr UINT IDM_STATUS = 1002;
};

} // namespace smartcam
