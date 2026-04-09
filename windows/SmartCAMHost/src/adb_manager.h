#pragma once
#include <string>
#include <functional>

namespace smartcam {

/// Manages ADB operations: device detection, reverse port, app launch
class AdbManager
{
public:
    using DeviceCallback = std::function<void(const std::string& serial, bool connected)>;

    /// Set path to bundled adb.exe
    void SetAdbPath(const std::wstring& path);

    /// Start polling for connected devices
    bool StartMonitoring(DeviceCallback callback);
    void StopMonitoring();

    /// Set up reverse port forwarding: PC:port <- Phone:port
    bool SetupReverse(const std::string& serial, int port);

    /// Launch SmartCAM app on device (skip if already running)
    bool LaunchApp(const std::string& serial);

private:
    std::wstring m_adbPath;
    // TODO: monitoring thread, device state tracking
};

} // namespace smartcam
