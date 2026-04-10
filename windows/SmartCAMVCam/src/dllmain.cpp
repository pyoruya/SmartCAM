#include <windows.h>
#include <new>
#include <initguid.h>
#include "vcam_guids.h"
#include "class_factory.h"

static HMODULE g_hModule = nullptr;
static long g_lockCount = 0;

BOOL WINAPI DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (rclsid != CLSID_SmartCAMMediaSource)
        return CLASS_E_CLASSNOTAVAILABLE;

    auto* factory = new (std::nothrow) smartcam::SmartCAMClassFactory();
    if (!factory)
        return E_OUTOFMEMORY;

    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return (g_lockCount == 0) ? S_OK : S_FALSE;
}
