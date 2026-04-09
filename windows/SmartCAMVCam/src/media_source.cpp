#include "media_source.h"

namespace smartcam {

// IMFMediaEventGenerator
IFACEMETHODIMP SmartCAMMediaSource::GetEvent(DWORD, IMFMediaEvent**) { return E_NOTIMPL; }
IFACEMETHODIMP SmartCAMMediaSource::BeginGetEvent(IMFAsyncCallback*, IUnknown*) { return E_NOTIMPL; }
IFACEMETHODIMP SmartCAMMediaSource::EndGetEvent(IMFAsyncResult*, IMFMediaEvent**) { return E_NOTIMPL; }
IFACEMETHODIMP SmartCAMMediaSource::QueueEvent(MediaEventType, REFGUID, HRESULT, const PROPVARIANT*) { return E_NOTIMPL; }

// IMFMediaSource
IFACEMETHODIMP SmartCAMMediaSource::GetCharacteristics(DWORD* pdwCharacteristics)
{
    if (!pdwCharacteristics) return E_POINTER;
    *pdwCharacteristics = MFMEDIASOURCE_IS_LIVE;
    return S_OK;
}

IFACEMETHODIMP SmartCAMMediaSource::CreatePresentationDescriptor(IMFPresentationDescriptor**)
{
    // TODO: Create presentation descriptor with NV12 media type
    // Expose supported resolutions: 3840x2160, 2560x1440, 1920x1080, 1280x720 @ 30fps
    return E_NOTIMPL;
}

IFACEMETHODIMP SmartCAMMediaSource::Start(IMFPresentationDescriptor*, const GUID*, const PROPVARIANT*)
{
    // TODO: Begin delivering frames from shared buffer
    return E_NOTIMPL;
}

IFACEMETHODIMP SmartCAMMediaSource::Stop()
{
    // TODO: Stop delivering frames
    return E_NOTIMPL;
}

IFACEMETHODIMP SmartCAMMediaSource::Pause()
{
    return MF_E_INVALID_STATE_TRANSITION; // Live source does not support pause
}

IFACEMETHODIMP SmartCAMMediaSource::Shutdown()
{
    // TODO: Release all resources
    return S_OK;
}

} // namespace smartcam
