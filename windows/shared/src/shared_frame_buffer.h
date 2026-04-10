#pragma once
/// Shared memory IPC for passing decoded NV12 frames
/// from SmartCAMHost (producer) to SmartCAMVCam (consumer).
///
/// Layout of shared memory:
///   [FrameHeader][NV12 pixel data]
///
/// Signaling: Named event "SmartCAM_FrameReady" is set by producer
/// after writing a new frame.

#include <windows.h>
#include <cstdint>
#include <string>

namespace smartcam {

struct FrameHeader
{
    uint32_t width;
    uint32_t height;
    uint32_t stride;       // bytes per row (Y plane)
    uint32_t frameSize;    // total NV12 data size in bytes
    int64_t  timestamp;    // presentation time in 100ns units
    uint32_t sequence;     // monotonic frame counter
    uint32_t reserved;
};

// NV12: Y plane = width*height, UV plane = width*height/2
// Total = width * height * 3 / 2
// Max 4K: 3840*2160*1.5 = ~12.4 MB, round up to 16 MB
static constexpr size_t MAX_FRAME_DATA_SIZE = 16 * 1024 * 1024;
static constexpr size_t SHARED_MEM_SIZE = sizeof(FrameHeader) + MAX_FRAME_DATA_SIZE;

static constexpr const wchar_t* SHARED_MEM_NAME = L"Local\\SmartCAM_FrameBuffer";
static constexpr const wchar_t* FRAME_READY_EVENT = L"Local\\SmartCAM_FrameReady";
static constexpr const wchar_t* BUFFER_MUTEX_NAME = L"Local\\SmartCAM_BufferMutex";

/// Producer side (SmartCAMHost): creates shared memory and writes frames.
class FrameBufferWriter
{
public:
    bool Create()
    {
        m_mutex = CreateMutexW(nullptr, FALSE, BUFFER_MUTEX_NAME);
        if (!m_mutex) return false;

        m_event = CreateEventW(nullptr, FALSE, FALSE, FRAME_READY_EVENT);
        if (!m_event) return false;

        m_mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
                                        PAGE_READWRITE, 0,
                                        static_cast<DWORD>(SHARED_MEM_SIZE),
                                        SHARED_MEM_NAME);
        if (!m_mapping) return false;

        m_view = MapViewOfFile(m_mapping, FILE_MAP_WRITE, 0, 0, SHARED_MEM_SIZE);
        if (!m_view) return false;

        ZeroMemory(m_view, sizeof(FrameHeader));
        return true;
    }

    void WriteFrame(uint32_t width, uint32_t height, uint32_t stride,
                    const uint8_t* data, uint32_t dataSize, int64_t timestamp)
    {
        if (!m_view || dataSize > MAX_FRAME_DATA_SIZE) return;

        WaitForSingleObject(m_mutex, INFINITE);

        auto* header = static_cast<FrameHeader*>(m_view);
        header->width = width;
        header->height = height;
        header->stride = stride;
        header->frameSize = dataSize;
        header->timestamp = timestamp;
        header->sequence++;

        auto* dst = static_cast<uint8_t*>(m_view) + sizeof(FrameHeader);
        memcpy(dst, data, dataSize);

        ReleaseMutex(m_mutex);
        SetEvent(m_event);
    }

    void Destroy()
    {
        if (m_view) { UnmapViewOfFile(m_view); m_view = nullptr; }
        if (m_mapping) { CloseHandle(m_mapping); m_mapping = nullptr; }
        if (m_event) { CloseHandle(m_event); m_event = nullptr; }
        if (m_mutex) { CloseHandle(m_mutex); m_mutex = nullptr; }
    }

    ~FrameBufferWriter() { Destroy(); }

private:
    HANDLE m_mapping = nullptr;
    HANDLE m_event = nullptr;
    HANDLE m_mutex = nullptr;
    void*  m_view = nullptr;
};

/// Consumer side (SmartCAMVCam): opens shared memory and reads frames.
class FrameBufferReader
{
public:
    bool Open()
    {
        m_mutex = OpenMutexW(SYNCHRONIZE, FALSE, BUFFER_MUTEX_NAME);
        if (!m_mutex) return false;

        m_event = OpenEventW(SYNCHRONIZE, FALSE, FRAME_READY_EVENT);
        if (!m_event) return false;

        m_mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, SHARED_MEM_NAME);
        if (!m_mapping) return false;

        m_view = MapViewOfFile(m_mapping, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
        if (!m_view) return false;

        return true;
    }

    /// Wait for a new frame. Returns false on timeout.
    bool WaitForFrame(DWORD timeoutMs = 1000)
    {
        return WaitForSingleObject(m_event, timeoutMs) == WAIT_OBJECT_0;
    }

    /// Read the current frame header. Lock must be held.
    FrameHeader ReadHeader() const
    {
        FrameHeader h = {};
        if (!m_view) return h;
        WaitForSingleObject(m_mutex, INFINITE);
        memcpy(&h, m_view, sizeof(FrameHeader));
        ReleaseMutex(m_mutex);
        return h;
    }

    /// Copy frame data into caller buffer. Returns bytes copied.
    uint32_t ReadFrame(FrameHeader& header, uint8_t* dst, uint32_t dstSize)
    {
        if (!m_view) return 0;
        WaitForSingleObject(m_mutex, INFINITE);

        auto* hdr = static_cast<const FrameHeader*>(m_view);
        memcpy(&header, hdr, sizeof(FrameHeader));

        uint32_t toCopy = (header.frameSize <= dstSize) ? header.frameSize : 0;
        if (toCopy > 0)
        {
            auto* src = static_cast<const uint8_t*>(m_view) + sizeof(FrameHeader);
            memcpy(dst, src, toCopy);
        }

        ReleaseMutex(m_mutex);
        return toCopy;
    }

    void Close()
    {
        if (m_view) { UnmapViewOfFile(m_view); m_view = nullptr; }
        if (m_mapping) { CloseHandle(m_mapping); m_mapping = nullptr; }
        if (m_event) { CloseHandle(m_event); m_event = nullptr; }
        if (m_mutex) { CloseHandle(m_mutex); m_mutex = nullptr; }
    }

    ~FrameBufferReader() { Close(); }

private:
    HANDLE m_mapping = nullptr;
    HANDLE m_event = nullptr;
    HANDLE m_mutex = nullptr;
    const void* m_view = nullptr;
};

} // namespace smartcam
