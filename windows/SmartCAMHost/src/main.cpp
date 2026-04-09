#include <windows.h>
#include <mfapi.h>
#include "tray.h"
#include "adb_manager.h"
#include "stream_receiver.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return 1;

    // Initialize Media Foundation
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
    {
        CoUninitialize();
        return 1;
    }

    // TODO: Initialize tray icon
    // TODO: Start ADB device monitoring
    // TODO: On device connected: adb reverse, launch app, start stream receiver
    // TODO: Create virtual camera via MFCreateVirtualCamera
    // TODO: Message loop

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    MFShutdown();
    CoUninitialize();
    return 0;
}
