#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace AetherSDR::icom {

// An audit session never inherits production authority from a caller or a GUI.
class IcomOutboundPolicy {
public:
    enum class Mode { Deny, Production, RxAudit };
    enum class Role { Control, Serial, Audio };
    struct Context {
        Role role = Role::Control;
        std::uint32_t localSid = 0;
        std::uint32_t remoteSid = 0;
        bool remoteKnown = false;
        bool ready = false;
        bool identified = false;
        // Only IcomSession may register canonical login/auth/stream requests.
        std::span<const std::vector<std::uint8_t>> controlPermits;
    };

    constexpr IcomOutboundPolicy() = default; // missing policy denies everything
    static constexpr IcomOutboundPolicy production() { return IcomOutboundPolicy(Mode::Production); }
    static constexpr IcomOutboundPolicy rxAudit() { return IcomOutboundPolicy(Mode::RxAudit); }
    [[nodiscard]] constexpr bool isAudit() const { return m_mode == Mode::RxAudit; }
    [[nodiscard]] constexpr bool isProduction() const { return m_mode == Mode::Production; }
    [[nodiscard]] bool allowsCiv(std::span<const std::uint8_t> frame) const;
    [[nodiscard]] bool allowsDatagram(std::span<const std::uint8_t> packet, const Context& context) const;

private:
    explicit constexpr IcomOutboundPolicy(Mode mode) : m_mode(mode) {}
    Mode m_mode = Mode::Deny;
};

} // namespace AetherSDR::icom
