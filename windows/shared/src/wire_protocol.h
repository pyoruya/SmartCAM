#pragma once
/// Wire protocol constants matching protocol/wire_format.md

#include <cstdint>

namespace smartcam {
namespace protocol {

static constexpr uint16_t VERSION = 1;
static constexpr int      DEFAULT_PORT = 38247;

// Magic values (big-endian in wire, but we store as host-order constants)
static constexpr uint32_t MAGIC_HELLO = 0x5343414D; // "SCAM"
static constexpr uint32_t MAGIC_CAPS  = 0x53434150; // "SCAP"
static constexpr uint32_t MAGIC_SEL   = 0x5353454C; // "SSEL"
static constexpr uint32_t MAGIC_START = 0x53535452; // "SSTR"

#pragma pack(push, 1)

struct Hello
{
    uint32_t magic;     // MAGIC_HELLO (BE)
    uint16_t version;   // protocol version (BE)
    uint16_t reserved;
};

struct ResolutionEntry
{
    uint16_t width;     // BE
    uint16_t height;    // BE
    uint16_t maxFps;    // BE
    uint16_t reserved;
};

struct Capabilities
{
    uint32_t magic;     // MAGIC_CAPS (BE)
    uint8_t  count;     // number of resolutions
    // followed by count * ResolutionEntry
};

struct SelectFormat
{
    uint32_t magic;     // MAGIC_SEL (BE)
    uint16_t width;     // BE
    uint16_t height;    // BE
    uint16_t fps;       // BE
    uint32_t bitrate;   // kbps (BE)
};

struct StreamStart
{
    uint32_t magic;     // MAGIC_START (BE)
    uint16_t width;     // BE
    uint16_t height;    // BE
    uint16_t fps;       // BE
};

#pragma pack(pop)

} // namespace protocol
} // namespace smartcam
