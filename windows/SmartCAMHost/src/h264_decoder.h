#pragma once
/// Media Foundation H.264 decoder wrapper.
/// Decodes H.264 NAL units to NV12 frames.

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#include <functional>
#include <vector>
#include <cstdint>

namespace smartcam {

class H264Decoder
{
public:
    struct DecodedFrame
    {
        const uint8_t* data;
        uint32_t       size;
        uint32_t       width;
        uint32_t       height;
        uint32_t       stride;
        int64_t        timestamp; // 100ns units
    };

    using FrameCallback = std::function<void(const DecodedFrame&)>;

    H264Decoder() = default;
    ~H264Decoder();

    /// Initialize the decoder MFT.
    HRESULT Initialize();

    /// Feed a NAL unit. May trigger zero or more frame callbacks.
    HRESULT DecodeNalUnit(const uint8_t* data, uint32_t size, int64_t timestamp);

    /// Flush remaining frames.
    HRESULT Flush();

    void SetFrameCallback(FrameCallback cb) { m_callback = std::move(cb); }

    void Shutdown();

private:
    HRESULT ProcessOutput();
    HRESULT SetInputType(uint32_t width, uint32_t height);

    IMFTransform*  m_decoder = nullptr;
    FrameCallback  m_callback;
    bool           m_inputTypeSet = false;
    std::vector<uint8_t> m_nalBuffer;
};

} // namespace smartcam
