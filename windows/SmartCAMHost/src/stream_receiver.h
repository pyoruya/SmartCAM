#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace smartcam {

/// Receives H.264 NAL units from phone over TCP
class StreamReceiver
{
public:
    using FrameCallback = std::function<void(const uint8_t* data, size_t size)>;

    /// Start listening for incoming connection on given port
    bool Start(int port, FrameCallback onFrame);
    void Stop();

    bool IsConnected() const { return m_connected; }

private:
    bool m_connected = false;
    // TODO: socket, receive thread, frame parsing
};

} // namespace smartcam
