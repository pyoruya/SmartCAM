# SmartCAM Wire Protocol v1

TCP single channel over ADB reverse port forwarding.

## Connection

- Phone listens on `tcp://localhost:<PORT>` (default: 38247)
- Host connects via ADB reverse: `adb reverse tcp:38247 tcp:38247`
- Host connects to `localhost:38247`

## Handshake

### 1. Hello (Host -> Phone)

| Offset | Size | Value |
|--------|------|-------|
| 0 | 4 | Magic: `SCAM` (0x5343414D) |
| 4 | 2 | Protocol version (uint16 BE): `1` |
| 6 | 2 | Reserved: `0` |

### 2. Capabilities (Phone -> Host)

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 4 | Magic: `SCAP` (0x53434150) |
| 4 | 1 | Number of supported resolutions (N) |
| 5 | N*8 | For each resolution: width(u16 BE), height(u16 BE), maxFps(u16 BE), reserved(u16) |

### 3. Select Format (Host -> Phone)

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 4 | Magic: `SSEL` (0x5353454C) |
| 4 | 2 | Width (uint16 BE) |
| 6 | 2 | Height (uint16 BE) |
| 8 | 2 | FPS (uint16 BE) |
| 10 | 4 | Target bitrate in kbps (uint32 BE) |

### 4. Stream Start (Phone -> Host)

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 4 | Magic: `SSTR` (0x53535452) |
| 4 | 2 | Actual width |
| 6 | 2 | Actual height |
| 8 | 2 | Actual FPS |

## Video Frames

After stream start, phone sends a continuous stream of NAL units:

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 4 | NAL unit size in bytes (uint32 BE) |
| 4 | N | H.264 NAL unit data (Annex-B start code stripped) |

The first NAL units sent are SPS and PPS parameter sets.

## Teardown

Either side closes the TCP connection. No explicit teardown message.
