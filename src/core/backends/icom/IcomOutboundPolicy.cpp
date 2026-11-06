#include "core/backends/icom/IcomOutboundPolicy.h"
#include "core/backends/icom/IcomProtocol.h"

#include <algorithm>
#include <array>

namespace AetherSDR::icom {
namespace {
std::uint16_t le16(std::span<const std::uint8_t> p, std::size_t at)
{
    return static_cast<std::uint16_t>(p[at] | (p[at + 1] << 8));
}
std::uint32_t be32(std::span<const std::uint8_t> p, std::size_t at)
{
    return (std::uint32_t(p[at]) << 24) | (std::uint32_t(p[at + 1]) << 16)
         | (std::uint32_t(p[at + 2]) << 8) | p[at + 3];
}
bool canonicalControl(std::span<const std::uint8_t> packet,
                      std::span<const std::vector<std::uint8_t>> permits)
{
    for (const auto& permit : permits) {
        if (permit.size() != packet.size()) { continue; }
        bool equal = true;
        for (std::size_t i = 0; i < packet.size(); ++i) {
            // The stream, not the builder, stamps the outer sequence number.
            if (i != 6 && i != 7 && packet[i] != permit[i]) { equal = false; break; }
        }
        if (equal) { return true; }
    }
    return false;
}
}

bool IcomOutboundPolicy::allowsCiv(std::span<const std::uint8_t> frame) const
{
    if (isProduction()) { return true; }
    if (!isAudit() || frame.size() < 6 || frame[0] != 0xFE || frame[1] != 0xFE
        || frame[2] != 0xB6 || frame[3] != 0xE0 || frame.back() != 0xFD) {
        return false;
    }
    if (frame.size() == 6) {
        return frame[4] == 0x0F || frame[4] == 0x03 || frame[4] == 0x04;
    }
    if (frame.size() != 7) { return false; }
    return (frame[4] == 0x19 && frame[5] == 0x00)
        || (frame[4] == 0x1C && frame[5] == 0x03)
        || (frame[4] == 0x26 && (frame[5] == 0x00 || frame[5] == 0x01))
        || (frame[4] == 0x25 && frame[5] == 0x01)
        || (frame[4] == 0x21 && frame[5] <= 0x02);
}

bool IcomOutboundPolicy::allowsDatagram(std::span<const std::uint8_t> p, const Context& c) const
{
    if (isProduction()) { return true; }
    if (!isAudit() || p.size() < kHeaderSize || c.localSid == 0) { return false; }
    const Header h = parseHeader(p);
    if (h.len != p.size() || h.sentId != c.localSid) { return false; }
    switch (c.role) {
    case Role::Control: case Role::Serial: case Role::Audio: break;
    default: return false;
    }
    if (h.type == static_cast<std::uint16_t>(PacketType::AreYouThere)) {
        // beginHandshake retries AreYouThere while waiting for IAmReady, even
        // after IAmHere has established the peer SID. This is still our handshake.
        return !c.ready && h.rcvdId == 0 && p.size() == kLenControl;
    }
    if (!c.remoteKnown || c.remoteSid == 0 || h.rcvdId != c.remoteSid) { return false; }
    if (h.type == static_cast<std::uint16_t>(PacketType::AreYouReady)) {
        return !c.ready && p.size() == kLenControl;
    }
    if (h.type == static_cast<std::uint16_t>(PacketType::Disconnect)) {
        return p.size() == kLenControl;
    }
    if (h.type == static_cast<std::uint16_t>(PacketType::Ping)) {
        return p.size() == kLenPing && p[0x10] <= 1;
    }
    if (!c.ready) { return false; }
    if (h.type == static_cast<std::uint16_t>(PacketType::Retransmit)) {
        return p.size() == kLenRetransmitOne; // only the implemented single-sequence request
    }
    if (h.type != static_cast<std::uint16_t>(PacketType::Idle)) { return false; }
    if (p.size() == kLenControl) { return true; }
    if (c.role == Role::Audio) { return false; } // includes PCM, silence and padding
    if (c.role == Role::Serial) {
        if (p.size() == kLenOpenClose && p[0x10] == 0xC0) {
            return p[0x11] == 1 && p[0x12] == 0 && (p[0x15] == 5 || p[0x15] == 0);
        }
        if (p.size() <= kSerialPayloadOffset || p[0x10] != 0xC1
            || le16(p, 0x11) != p.size() - kSerialPayloadOffset) {
            return false;
        }
        const auto civ = p.subspan(kSerialPayloadOffset);
        if (!allowsCiv(civ)) { return false; }
        return c.identified || (civ.size() == 7 && civ[4] == 0x19 && civ[5] == 0);
    }
    if (!canonicalControl(p, c.controlPermits)) { return false; }
    if (p.size() != kLenLogin && p.size() != kLenToken && p.size() != kLenConnInfo) {
        return false;
    }
    if (be32(p, 0x10) != p.size() - kHeaderSize || p[0x14] != 1) { return false; }
    if (p.size() == kLenLogin) { return p[0x15] == 0; }
    if (p.size() == kLenToken) {
        return p[0x15] == 1 || p[0x15] == 2 || p[0x15] == 5;
    }
    return p[0x15] == 3 && p[0x70] == 1 && p[0x71] == 0 && p[0x73] == 0;
}
} // namespace AetherSDR::icom
