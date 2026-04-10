#include "adb_manager.h"
#include <sstream>
#include <algorithm>
#include <stdio.h>

namespace smartcam {

AdbManager::~AdbManager()
{
    StopMonitoring();
}

void AdbManager::SetAdbPath(const std::wstring& path)
{
    m_adbPath = path;
}

bool AdbManager::StartMonitoring(DeviceCallback callback)
{
    if (m_monitoring.load()) return false;
    m_deviceCallback = std::move(callback);
    m_monitoring.store(true);
    m_monitorThread = std::thread(&AdbManager::MonitorLoop, this);
    return true;
}

void AdbManager::StopMonitoring()
{
    m_monitoring.store(false);
    if (m_monitorThread.joinable())
        m_monitorThread.join();
}

void AdbManager::MonitorLoop()
{
    while (m_monitoring.load())
    {
        std::string output = RunAdb("devices");

        // Parse "adb devices" output
        std::vector<std::string> currentDevices;
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line))
        {
            if (line.find("device") != std::string::npos && line.find("List") == std::string::npos)
            {
                // Line format: "SERIAL\tdevice"
                auto tab = line.find('\t');
                if (tab != std::string::npos)
                {
                    std::string status = line.substr(tab + 1);
                    if (status.find("device") == 0)
                    {
                        std::string serial = line.substr(0, tab);
                        currentDevices.push_back(serial);
                    }
                }
            }
        }

        // Detect new devices
        for (const auto& dev : currentDevices)
        {
            auto it = std::find(m_knownDevices.begin(), m_knownDevices.end(), dev);
            if (it == m_knownDevices.end())
            {
                printf("[ADB] Device connected: %s\n", dev.c_str());
                if (m_deviceCallback)
                    m_deviceCallback(dev, true);
            }
        }

        // Detect removed devices
        for (const auto& dev : m_knownDevices)
        {
            auto it = std::find(currentDevices.begin(), currentDevices.end(), dev);
            if (it == currentDevices.end())
            {
                printf("[ADB] Device disconnected: %s\n", dev.c_str());
                if (m_deviceCallback)
                    m_deviceCallback(dev, false);
            }
        }

        m_knownDevices = currentDevices;
        Sleep(3000); // Poll every 3 seconds
    }
}

bool AdbManager::SetupReverse(const std::string& serial, int port)
{
    std::string args = "reverse tcp:" + std::to_string(port) + " tcp:" + std::to_string(port);
    std::string result = RunAdbForDevice(serial, args);
    printf("[ADB] Reverse: %s\n", result.c_str());
    return result.find("error") == std::string::npos;
}

bool AdbManager::RemoveReverse(const std::string& serial, int port)
{
    std::string args = "reverse --remove tcp:" + std::to_string(port);
    RunAdbForDevice(serial, args);
    return true;
}

bool AdbManager::LaunchApp(const std::string& serial)
{
    if (IsAppRunning(serial))
    {
        printf("[ADB] SmartCAM already running on %s\n", serial.c_str());
        return true;
    }

    std::string result = RunAdbForDevice(serial,
        "shell am start -n com.smartcam/.MainActivity");
    printf("[ADB] Launch: %s\n", result.c_str());
    return result.find("Error") == std::string::npos;
}

bool AdbManager::IsAppRunning(const std::string& serial)
{
    std::string result = RunAdbForDevice(serial, "shell pidof com.smartcam");
    // pidof returns PID if running, empty if not
    return !result.empty() && result[0] >= '0' && result[0] <= '9';
}

std::string AdbManager::GetFirstDevice()
{
    std::string output = RunAdb("devices");
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line))
    {
        if (line.find("\tdevice") != std::string::npos)
        {
            auto tab = line.find('\t');
            if (tab != std::string::npos)
                return line.substr(0, tab);
        }
    }
    return "";
}

std::string AdbManager::RunAdb(const std::string& args)
{
    if (m_adbPath.empty()) return "";

    // Build command: "adb.exe" <args>
    std::wstring cmd = L"\"" + m_adbPath + L"\" " +
        std::wstring(args.begin(), args.end());

    // Execute and capture stdout
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return "";

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
                              CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);

    std::string result;
    if (ok)
    {
        char buf[4096];
        DWORD bytesRead;
        while (ReadFile(hRead, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0)
        {
            buf[bytesRead] = 0;
            result += buf;
        }
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    CloseHandle(hRead);

    // Trim trailing whitespace
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
        result.pop_back();

    return result;
}

std::string AdbManager::RunAdbForDevice(const std::string& serial, const std::string& args)
{
    return RunAdb("-s " + serial + " " + args);
}

} // namespace smartcam
