#include "stream_receiver.h"
#include "wire_protocol.h"
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

namespace smartcam {

// Big-endian helpers
static uint16_t htons_val(uint16_t v) { return htons(v); }
static uint32_t htonl_val(uint32_t v) { return htonl(v); }
static uint16_t ntohs_val(uint16_t v) { return ntohs(v); }
static uint32_t ntohl_val(uint32_t v) { return ntohl(v); }

StreamReceiver::~StreamReceiver()
{
    Stop();
}

void StreamReceiver::SetCallbacks(NalCallback onNal, ConnectCallback onConnect, DisconnectCallback onDisconnect)
{
    m_onNal = std::move(onNal);
    m_onConnect = std::move(onConnect);
    m_onDisconnect = std::move(onDisconnect);
}

bool StreamReceiver::Start(int port)
{
    if (m_running.load()) return false;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("[Receiver] WSAStartup failed\n");
        return false;
    }

    m_running.store(true);
    m_thread = std::thread(&StreamReceiver::ReceiveLoop, this, port);
    return true;
}

void StreamReceiver::Stop()
{
    m_running.store(false);
    if (m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    if (m_thread.joinable())
        m_thread.join();
    WSACleanup();
}

void StreamReceiver::ReceiveLoop(int port)
{
    while (m_running.load())
    {
        // Connect to localhost:port (ADB reverse forwards to phone)
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET)
        {
            printf("[Receiver] socket() failed\n");
            Sleep(1000);
            continue;
        }

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<u_short>(port));
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        printf("[Receiver] Connecting to localhost:%d...\n", port);
        if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            closesocket(sock);
            Sleep(2000); // Retry after 2 seconds
            continue;
        }

        // Disable Nagle for low latency
        int flag = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flag), sizeof(flag));

        m_socket = sock;
        printf("[Receiver] Connected\n");

        if (!DoHandshake(sock))
        {
            printf("[Receiver] Handshake failed\n");
            closesocket(sock);
            m_socket = INVALID_SOCKET;
            Sleep(2000);
            continue;
        }

        m_connected.store(true);
        if (m_onConnect)
            m_onConnect(m_streamInfo);

        printf("[Receiver] Streaming: %dx%d @ %dfps\n",
               m_streamInfo.width, m_streamInfo.height, m_streamInfo.fps);

        // Receive NAL units: [4-byte BE length][data]
        int64_t timestamp = 0;
        int64_t frameDuration = 333333; // ~30fps in 100ns
        std::vector<uint8_t> nalBuf;

        while (m_running.load())
        {
            uint32_t nalSize = 0;
            if (!ReceiveExact(sock, &nalSize, 4))
                break;

            nalSize = ntohl_val(nalSize);
            if (nalSize == 0 || nalSize > 10 * 1024 * 1024) // sanity: max 10MB
            {
                printf("[Receiver] Invalid NAL size: %u\n", nalSize);
                break;
            }

            nalBuf.resize(nalSize);
            if (!ReceiveExact(sock, nalBuf.data(), nalSize))
                break;

            if (m_onNal)
                m_onNal(nalBuf.data(), nalSize, timestamp);

            timestamp += frameDuration;
        }

        m_connected.store(false);
        closesocket(sock);
        m_socket = INVALID_SOCKET;

        if (m_onDisconnect)
            m_onDisconnect();

        printf("[Receiver] Disconnected\n");
        if (m_running.load())
            Sleep(2000); // Wait before reconnect
    }
}

bool StreamReceiver::DoHandshake(SOCKET sock)
{
    namespace proto = protocol;

    // 1. Send Hello
    proto::Hello hello = {};
    hello.magic = htonl_val(proto::MAGIC_HELLO);
    hello.version = htons_val(proto::VERSION);
    hello.reserved = 0;

    if (send(sock, reinterpret_cast<char*>(&hello), sizeof(hello), 0) != sizeof(hello))
        return false;

    // 2. Receive Capabilities
    proto::Capabilities caps = {};
    if (!ReceiveExact(sock, &caps, sizeof(caps)))
        return false;

    if (ntohl_val(caps.magic) != proto::MAGIC_CAPS)
    {
        printf("[Receiver] Bad caps magic: 0x%08X\n", ntohl_val(caps.magic));
        return false;
    }

    printf("[Receiver] Phone supports %d resolutions\n", caps.count);

    // Read resolution entries
    std::vector<proto::ResolutionEntry> resolutions(caps.count);
    if (caps.count > 0)
    {
        if (!ReceiveExact(sock, resolutions.data(),
                          caps.count * static_cast<int>(sizeof(proto::ResolutionEntry))))
            return false;
    }

    // Pick the best resolution (first = highest)
    uint16_t selWidth = 1280, selHeight = 720, selFps = 30;
    uint32_t selBitrate = 4000; // kbps
    if (!resolutions.empty())
    {
        selWidth = ntohs_val(resolutions[0].width);
        selHeight = ntohs_val(resolutions[0].height);
        selFps = ntohs_val(resolutions[0].maxFps);

        // Bitrate heuristic based on resolution
        uint32_t pixels = static_cast<uint32_t>(selWidth) * selHeight;
        if (pixels >= 3840 * 2160) selBitrate = 25000;
        else if (pixels >= 2560 * 1440) selBitrate = 15000;
        else if (pixels >= 1920 * 1080) selBitrate = 8000;
        else selBitrate = 4000;
    }

    printf("[Receiver] Selecting %dx%d @ %dfps, %dkbps\n",
           selWidth, selHeight, selFps, selBitrate);

    // 3. Send SelectFormat
    proto::SelectFormat sel = {};
    sel.magic = htonl_val(proto::MAGIC_SEL);
    sel.width = htons_val(selWidth);
    sel.height = htons_val(selHeight);
    sel.fps = htons_val(selFps);
    sel.bitrate = htonl_val(selBitrate);

    if (send(sock, reinterpret_cast<char*>(&sel), sizeof(sel), 0) != sizeof(sel))
        return false;

    // 4. Receive StreamStart
    proto::StreamStart start = {};
    if (!ReceiveExact(sock, &start, sizeof(start)))
        return false;

    if (ntohl_val(start.magic) != proto::MAGIC_START)
    {
        printf("[Receiver] Bad start magic: 0x%08X\n", ntohl_val(start.magic));
        return false;
    }

    m_streamInfo.width = ntohs_val(start.width);
    m_streamInfo.height = ntohs_val(start.height);
    m_streamInfo.fps = ntohs_val(start.fps);

    return true;
}

bool StreamReceiver::ReceiveExact(SOCKET sock, void* buf, int len)
{
    char* p = static_cast<char*>(buf);
    int remaining = len;
    while (remaining > 0 && m_running.load())
    {
        int received = recv(sock, p, remaining, 0);
        if (received <= 0)
            return false;
        p += received;
        remaining -= received;
    }
    return remaining == 0;
}

} // namespace smartcam
