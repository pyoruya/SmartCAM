#pragma once
/// TCP client that connects to the phone's StreamServer,
/// performs wire protocol handshake, and delivers H.264 NAL units.

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>

namespace smartcam {

class StreamReceiver
{
public:
    struct StreamInfo
    {
        uint16_t width;
        uint16_t height;
        uint16_t fps;
    };

    using NalCallback = std::function<void(const uint8_t* data, uint32_t size, int64_t timestamp)>;
    using ConnectCallback = std::function<void(const StreamInfo& info)>;
    using DisconnectCallback = std::function<void()>;

    StreamReceiver() = default;
    ~StreamReceiver();

    void SetCallbacks(NalCallback onNal, ConnectCallback onConnect, DisconnectCallback onDisconnect);

    /// Connect to phone on localhost:port, perform handshake, start receiving.
    bool Start(int port);
    void Stop();
    bool IsConnected() const { return m_connected.load(); }

private:
    void ReceiveLoop(int port);
    bool DoHandshake(SOCKET sock);
    bool ReceiveExact(SOCKET sock, void* buf, int len);

    NalCallback        m_onNal;
    ConnectCallback    m_onConnect;
    DisconnectCallback m_onDisconnect;
    std::atomic<bool>  m_running{false};
    std::atomic<bool>  m_connected{false};
    std::thread        m_thread;
    SOCKET             m_socket = INVALID_SOCKET;

    // Negotiated stream parameters
    StreamInfo m_streamInfo = {};
};

} // namespace smartcam
