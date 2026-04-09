#pragma once
#include <mfidl.h>
#include <mfapi.h>
#include <wrl/implements.h>

namespace smartcam {

/// IMFMediaSource implementation for the SmartCAM virtual camera.
/// This is loaded by Windows Frame Server when an app opens the "SmartCAM" camera.
class SmartCAMMediaSource
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFMediaSource,
          IMFMediaEventGenerator>
{
public:
    // IMFMediaEventGenerator
    IFACEMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent) override;
    IFACEMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState) override;
    IFACEMETHODIMP EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent) override;
    IFACEMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType,
                              HRESULT hrStatus, const PROPVARIANT* pvValue) override;

    // IMFMediaSource
    IFACEMETHODIMP GetCharacteristics(DWORD* pdwCharacteristics) override;
    IFACEMETHODIMP CreatePresentationDescriptor(IMFPresentationDescriptor** ppPD) override;
    IFACEMETHODIMP Start(IMFPresentationDescriptor* pPD, const GUID* pguidTimeFormat,
                         const PROPVARIANT* pvarStartPosition) override;
    IFACEMETHODIMP Stop() override;
    IFACEMETHODIMP Pause() override;
    IFACEMETHODIMP Shutdown() override;

private:
    // TODO: Event queue, stream, shared memory / pipe for frame data from host
};

} // namespace smartcam
