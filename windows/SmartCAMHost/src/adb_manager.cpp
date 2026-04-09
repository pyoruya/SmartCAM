#include "adb_manager.h"

namespace smartcam {

void AdbManager::SetAdbPath(const std::wstring& path)
{
    m_adbPath = path;
}

bool AdbManager::StartMonitoring(DeviceCallback callback)
{
    // TODO: Spawn thread that runs "adb devices" periodically
    // or uses "adb track-devices" for push-based notification
    return false;
}

void AdbManager::StopMonitoring()
{
    // TODO: Stop monitoring thread
}

bool AdbManager::SetupReverse(const std::string& serial, int port)
{
    // TODO: Execute "adb -s <serial> reverse tcp:<port> tcp:<port>"
    return false;
}

bool AdbManager::LaunchApp(const std::string& serial)
{
    // TODO: Check if app is running via "adb shell pidof com.smartcam"
    // If not, launch via "adb shell am start -n com.smartcam/.MainActivity"
    return false;
}

} // namespace smartcam
