#include "h264_decoder.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <codecapi.h>
#include <wmcodecdsp.h>
#include <stdio.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")

namespace smartcam {

H264Decoder::~H264Decoder()
{
    Shutdown();
}

HRESULT H264Decoder::Initialize()
{
    // Find H.264 decoder MFT
    MFT_REGISTER_TYPE_INFO inputType = { MFMediaType_Video, MFVideoFormat_H264 };

    IMFActivate** activates = nullptr;
    UINT32 count = 0;
    HRESULT hr = MFTEnumEx(
        MFT_CATEGORY_VIDEO_DECODER,
        MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT | MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
        &inputType,
        nullptr,
        &activates,
        &count);

    if (FAILED(hr) || count == 0)
    {
        printf("[Decoder] No H.264 decoder found\n");
        return hr != S_OK ? hr : E_FAIL;
    }

    hr = activates[0]->ActivateObject(IID_PPV_ARGS(&m_decoder));

    // Release activates
    for (UINT32 i = 0; i < count; i++)
        activates[i]->Release();
    CoTaskMemFree(activates);

    if (FAILED(hr))
    {
        printf("[Decoder] Failed to activate decoder: 0x%08X\n", hr);
        return hr;
    }

    // Set input type: H.264
    IMFMediaType* mediaType = nullptr;
    hr = MFCreateMediaType(&mediaType);
    if (FAILED(hr)) return hr;

    mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    hr = m_decoder->SetInputType(0, mediaType, 0);
    mediaType->Release();

    if (FAILED(hr))
    {
        printf("[Decoder] SetInputType failed: 0x%08X\n", hr);
        return hr;
    }

    // Set output type: NV12
    // Enumerate output types to find NV12
    for (DWORD i = 0; ; i++)
    {
        IMFMediaType* outType = nullptr;
        hr = m_decoder->GetOutputAvailableType(0, i, &outType);
        if (FAILED(hr)) break;

        GUID subtype;
        outType->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (subtype == MFVideoFormat_NV12)
        {
            hr = m_decoder->SetOutputType(0, outType, 0);
            outType->Release();
            if (SUCCEEDED(hr))
            {
                m_inputTypeSet = true;
                printf("[Decoder] Output type set to NV12\n");
            }
            break;
        }
        outType->Release();
    }

    if (!m_inputTypeSet)
    {
        printf("[Decoder] Failed to set NV12 output type\n");
        return E_FAIL;
    }

    // Send MFT_MESSAGE_NOTIFY_BEGIN_STREAMING
    m_decoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    m_decoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

    printf("[Decoder] Initialized successfully\n");
    return S_OK;
}

HRESULT H264Decoder::DecodeNalUnit(const uint8_t* data, uint32_t size, int64_t timestamp)
{
    if (!m_decoder || size == 0) return E_FAIL;

    // Build Annex-B NAL unit: prepend start code
    m_nalBuffer.clear();
    m_nalBuffer.reserve(4 + size);
    m_nalBuffer.push_back(0x00);
    m_nalBuffer.push_back(0x00);
    m_nalBuffer.push_back(0x00);
    m_nalBuffer.push_back(0x01);
    m_nalBuffer.insert(m_nalBuffer.end(), data, data + size);

    // Create input sample
    IMFSample* sample = nullptr;
    IMFMediaBuffer* buffer = nullptr;
    HRESULT hr;

    hr = MFCreateMemoryBuffer(static_cast<DWORD>(m_nalBuffer.size()), &buffer);
    if (FAILED(hr)) return hr;

    BYTE* bufData = nullptr;
    hr = buffer->Lock(&bufData, nullptr, nullptr);
    if (SUCCEEDED(hr))
    {
        memcpy(bufData, m_nalBuffer.data(), m_nalBuffer.size());
        buffer->Unlock();
        buffer->SetCurrentLength(static_cast<DWORD>(m_nalBuffer.size()));
    }

    hr = MFCreateSample(&sample);
    if (FAILED(hr)) { buffer->Release(); return hr; }

    sample->AddBuffer(buffer);
    sample->SetSampleTime(timestamp);
    sample->SetSampleDuration(333333); // ~30fps in 100ns units
    buffer->Release();

    // Feed to decoder
    hr = m_decoder->ProcessInput(0, sample, 0);
    sample->Release();

    if (hr == MF_E_NOTACCEPTING)
    {
        // Decoder is full, drain output first then retry
        ProcessOutput();
        hr = m_decoder->ProcessInput(0, sample, 0);
    }
    else if (SUCCEEDED(hr))
    {
        // Try to get output
        ProcessOutput();
    }

    return hr;
}

HRESULT H264Decoder::ProcessOutput()
{
    if (!m_decoder) return E_FAIL;

    HRESULT hr = S_OK;
    while (hr == S_OK)
    {
        MFT_OUTPUT_DATA_BUFFER outputBuffer = {};
        DWORD status = 0;

        // Check if decoder allocates its own samples
        MFT_OUTPUT_STREAM_INFO streamInfo = {};
        m_decoder->GetOutputStreamInfo(0, &streamInfo);

        IMFSample* outSample = nullptr;
        if (!(streamInfo.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)))
        {
            // We need to provide the output sample
            IMFMediaBuffer* outBuffer = nullptr;
            hr = MFCreateMemoryBuffer(streamInfo.cbSize ? streamInfo.cbSize : 4096 * 4096 * 3, &outBuffer);
            if (FAILED(hr)) return hr;

            hr = MFCreateSample(&outSample);
            if (FAILED(hr)) { outBuffer->Release(); return hr; }

            outSample->AddBuffer(outBuffer);
            outBuffer->Release();
            outputBuffer.pSample = outSample;
        }

        hr = m_decoder->ProcessOutput(0, 1, &outputBuffer, &status);

        if (hr == MF_E_TRANSFORM_STREAM_CHANGE)
        {
            // Output format changed — re-negotiate
            if (outSample) outSample->Release();

            for (DWORD i = 0; ; i++)
            {
                IMFMediaType* newType = nullptr;
                HRESULT hr2 = m_decoder->GetOutputAvailableType(0, i, &newType);
                if (FAILED(hr2)) break;

                GUID subtype;
                newType->GetGUID(MF_MT_SUBTYPE, &subtype);
                if (subtype == MFVideoFormat_NV12)
                {
                    m_decoder->SetOutputType(0, newType, 0);
                    newType->Release();
                    break;
                }
                newType->Release();
            }
            hr = S_OK; // retry
            continue;
        }

        if (FAILED(hr))
        {
            if (outSample) outSample->Release();
            break;
        }

        // Extract decoded frame
        IMFSample* resultSample = outputBuffer.pSample;
        if (resultSample && m_callback)
        {
            IMFMediaBuffer* mediaBuffer = nullptr;
            if (SUCCEEDED(resultSample->ConvertToContiguousBuffer(&mediaBuffer)))
            {
                BYTE* frameData = nullptr;
                DWORD frameLength = 0;
                if (SUCCEEDED(mediaBuffer->Lock(&frameData, nullptr, &frameLength)))
                {
                    // Get frame dimensions from current output type
                    IMFMediaType* currentType = nullptr;
                    uint32_t w = 0, h = 0, stride = 0;
                    if (SUCCEEDED(m_decoder->GetOutputCurrentType(0, &currentType)))
                    {
                        UINT32 tw = 0, th = 0;
                        MFGetAttributeSize(currentType, MF_MT_FRAME_SIZE, &tw, &th);
                        w = tw;
                        h = th;

                        // Try to get stride
                        UINT32 ts = 0;
                        if (FAILED(currentType->GetUINT32(MF_MT_DEFAULT_STRIDE, &ts)))
                            ts = w; // fallback
                        stride = ts;

                        currentType->Release();
                    }

                    int64_t sampleTime = 0;
                    resultSample->GetSampleTime(&sampleTime);

                    DecodedFrame frame = {};
                    frame.data = frameData;
                    frame.size = frameLength;
                    frame.width = w;
                    frame.height = h;
                    frame.stride = stride;
                    frame.timestamp = sampleTime;

                    m_callback(frame);

                    mediaBuffer->Unlock();
                }
                mediaBuffer->Release();
            }
        }

        if (outSample && outSample != outputBuffer.pSample)
            outSample->Release();
        if (outputBuffer.pSample && outputBuffer.pSample != outSample)
            outputBuffer.pSample->Release();
        if (outputBuffer.pEvents)
            outputBuffer.pEvents->Release();
    }

    return S_OK;
}

HRESULT H264Decoder::Flush()
{
    if (!m_decoder) return E_FAIL;
    m_decoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
    ProcessOutput();
    return S_OK;
}

void H264Decoder::Shutdown()
{
    if (m_decoder)
    {
        m_decoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        m_decoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
        m_decoder->Release();
        m_decoder = nullptr;
    }
    m_inputTypeSet = false;
}

} // namespace smartcam
