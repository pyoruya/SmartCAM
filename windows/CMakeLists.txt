cmake_minimum_required(VERSION 3.24)
project(SmartCAM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 11 SDK target (22000+)
add_compile_definitions(
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    NTDDI_VERSION=0x0A00000C   # NTDDI_WIN10_NI (Win11 22H2)
    UNICODE
    _UNICODE
)

add_subdirectory(SmartCAMHost)
add_subdirectory(SmartCAMVCam)
