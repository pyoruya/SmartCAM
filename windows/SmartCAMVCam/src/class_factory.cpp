#include "class_factory.h"
#include "media_source.h"
#include "vcam_guids.h"

namespace smartcam {

IFACEMETHODIMP SmartCAMClassFactory::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IClassFactory)
    {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) SmartCAMClassFactory::AddRef() { return InterlockedIncrement(&m_refCount); }
IFACEMETHODIMP_(ULONG) SmartCAMClassFactory::Release()
{
    long ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) delete this;
    return ref;
}

IFACEMETHODIMP SmartCAMClassFactory::CreateInstance(IUnknown* pOuter, REFIID riid, void** ppv)
{
    if (pOuter) return CLASS_E_NOAGGREGATION;
    auto source = Microsoft::WRL::Make<SmartCAMMediaSource>();
    return source->QueryInterface(riid, ppv);
}

IFACEMETHODIMP SmartCAMClassFactory::LockServer(BOOL) { return S_OK; }

} // namespace smartcam
