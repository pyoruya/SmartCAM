#pragma once
/// Manages ADB operations: device detection, reverse port forwarding, app launch.

#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>

namespace smartcam {

class AdbManager
{
public:
    using DeviceCallback = std::function<void(const std::string& serial, bool connected)>;

    AdbManager() = default;
    ~AdbManager();

    /// Set path to bundled adb.exe
    void SetAdbPath(const std::wstring& path);

    /// Start monitoring for device connections. Calls callback on change.
    bool StartMonitoring(DeviceCallback callback);
    void StopMonitoring();

    /// Set up reverse port forwarding: PC:port <- Phone:port
    bool SetupReverse(const std::string& serial, int port);

    /// Remove reverse port forwarding
    bool RemoveReverse(const std::string& serial, int port);

    /// Launch SmartCAM app on device (skip if already running)
    bool LaunchApp(const std::string& serial);

    /// Check if app is running on device
    bool IsAppRunning(const std::string& serial);

    /// Get the serial of the first connected device, or empty string
    std::string GetFirstDevice();

private:
    /// Execute adb command and return stdout
    std::string RunAdb(const std::string& args);

    /// Execute adb command targeting specific device
    std::string RunAdbForDevice(const std::string& serial, const std::string& args);

    void MonitorLoop();

    std::wstring      m_adbPath;
    DeviceCallback    m_deviceCallback;
    std::thread       m_monitorThread;
    std::atomic<bool> m_monitoring{false};
    std::vector<std::string> m_knownDevices;
};

} // namespace smartcam
