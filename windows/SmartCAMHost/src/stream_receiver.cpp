#include "stream_receiver.h"

namespace smartcam {

bool StreamReceiver::Start(int port, FrameCallback onFrame)
{
    // TODO: Create TCP socket, bind to localhost:port, accept connection
    // TODO: Receive loop: read [4-byte length][NAL unit], invoke onFrame
    return false;
}

void StreamReceiver::Stop()
{
    // TODO: Close socket, stop thread
    m_connected = false;
}

} // namespace smartcam
