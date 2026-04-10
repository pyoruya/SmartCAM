#include <windows.h>
#include <mfapi.h>
#include <stdio.h>
#include <string>

#include "tray.h"
#include "adb_manager.h"
#include "stream_receiver.h"
#include "h264_decoder.h"
#include "shared_frame_buffer.h"
#include "wire_protocol.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "ole32.lib")

using namespace smartcam;

static TrayIcon         g_tray;
static AdbManager       g_adb;
static StreamReceiver   g_receiver;
static H264Decoder      g_decoder;
static FrameBufferWriter g_frameBuffer;
static bool             g_running = true;

/// Find adb.exe path: look next to our .exe, then in third_party/adb/
static std::wstring FindAdbPath()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // Strip filename to get directory
    std::wstring dir(exePath);
    auto pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        dir = dir.substr(0, pos + 1);

    // Check next to exe
    std::wstring candidate = dir + L"adb.exe";
    if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES)
        return candidate;

    // Check third_party/adb/ relative to exe
    candidate = dir + L"..\\..\\third_party\\adb\\adb.exe";
    if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES)
        return candidate;

    // Check platform-tools in local Android SDK
    wchar_t localAppData[MAX_PATH];
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH))
    {
        candidate = std::wstring(localAppData) + L"\\Android\\Sdk\\platform-tools\\adb.exe";
        if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES)
            return candidate;
    }

    printf("[Main] adb.exe not found\n");
    return L"";
}

static void OnDeviceConnected(const std::string& serial, bool connected)
{
    if (connected)
    {
        printf("[Main] Device %s connected, setting up...\n", serial.c_str());
        g_tray.SetStatus(TrayIcon::Status::Waiting, L"Setting up device...");

        // Setup ADB reverse port forwarding
        if (!g_adb.SetupReverse(serial, protocol::DEFAULT_PORT))
        {
            printf("[Main] Failed to setup ADB reverse\n");
            g_tray.SetStatus(TrayIcon::Status::Error, L"ADB reverse failed");
            return;
        }

        // Launch SmartCAM app on phone
        g_adb.LaunchApp(serial);

        // Wait a moment for app to start, then connect
        Sleep(2000);

        // Start receiver (connects to phone via ADB reverse)
        g_receiver.Start(protocol::DEFAULT_PORT);
    }
    else
    {
        printf("[Main] Device %s disconnected\n", serial.c_str());
        g_receiver.Stop();
        g_decoder.Flush();
        g_tray.SetStatus(TrayIcon::Status::Idle);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    // Allocate console for debug output
    #ifdef _DEBUG
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    #endif

    printf("[Main] SmartCAM Host starting...\n");

    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        printf("[Main] CoInitializeEx failed: 0x%08X\n", hr);
        return 1;
    }

    // Initialize Media Foundation
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
    {
        printf("[Main] MFStartup failed: 0x%08X\n", hr);
        CoUninitialize();
        return 1;
    }

    // Initialize shared frame buffer (for VCam to read)
    if (!g_frameBuffer.Create())
    {
        printf("[Main] Failed to create shared frame buffer\n");
        MFShutdown();
        CoUninitialize();
        return 1;
    }

    // Initialize H.264 decoder
    hr = g_decoder.Initialize();
    if (FAILED(hr))
    {
        printf("[Main] Decoder init failed: 0x%08X\n", hr);
        g_frameBuffer.Destroy();
        MFShutdown();
        CoUninitialize();
        return 1;
    }

    // Decoder outputs NV12 frames -> write to shared buffer
    g_decoder.SetFrameCallback([](const H264Decoder::DecodedFrame& frame) {
        g_frameBuffer.WriteFrame(
            frame.width, frame.height, frame.stride,
            frame.data, frame.size, frame.timestamp);
    });

    // Set up stream receiver callbacks
    g_receiver.SetCallbacks(
        // On NAL unit received
        [](const uint8_t* data, uint32_t size, int64_t timestamp) {
            g_decoder.DecodeNalUnit(data, size, timestamp);
        },
        // On stream connected
        [](const StreamReceiver::StreamInfo& info) {
            printf("[Main] Stream started: %dx%d @ %dfps\n", info.width, info.height, info.fps);
            g_tray.SetStatus(TrayIcon::Status::Connected);
        },
        // On stream disconnected
        []() {
            printf("[Main] Stream disconnected\n");
            g_tray.SetStatus(TrayIcon::Status::Waiting, L"Reconnecting...");
        }
    );

    // Initialize tray icon
    g_tray.Initialize(hInstance, [&]() {
        g_running = false;
    });
    g_tray.SetStatus(TrayIcon::Status::Idle);

    // Find and setup ADB
    std::wstring adbPath = FindAdbPath();
    if (!adbPath.empty())
    {
        printf("[Main] Using ADB: ");
        wprintf(L"%s\n", adbPath.c_str());
        g_adb.SetAdbPath(adbPath);
        g_adb.StartMonitoring(OnDeviceConnected);
        g_tray.SetStatus(TrayIcon::Status::Waiting, L"Waiting for phone...");
    }
    else
    {
        g_tray.SetStatus(TrayIcon::Status::Error, L"adb.exe not found");
    }

    printf("[Main] Running. Right-click tray icon to quit.\n");

    // Message loop
    MSG msg;
    while (g_running)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Sleep(50); // Avoid busy-wait
        }
    }

    // Cleanup
    printf("[Main] Shutting down...\n");
    g_adb.StopMonitoring();
    g_receiver.Stop();
    g_decoder.Shutdown();
    g_frameBuffer.Destroy();
    g_tray.Shutdown();

    MFShutdown();
    CoUninitialize();

    return 0;
}
