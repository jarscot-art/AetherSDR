#include "core/backends/icom/IcomOutboundPolicy.h"
#include "core/backends/icom/IcomProtocol.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace AetherSDR::icom;
namespace {
int failures = 0;
void check(bool value, const char* label)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", label); ++failures; }
}
std::vector<std::uint8_t> civ(std::initializer_list<std::uint8_t> body)
{
    std::vector<std::uint8_t> result{0xFE, 0xFE, 0xB6, 0xE0};
    result.insert(result.end(), body.begin(), body.end());
    result.push_back(0xFD);
    return result;
}
}
int main()
{
    const auto policy = IcomOutboundPolicy::rxAudit();
    IcomOutboundPolicy::Context context;
    context.role = IcomOutboundPolicy::Role::Serial;
    context.localSid = 0x12345678;
    context.remoteSid = 0x87654321;
    context.remoteKnown = context.ready = context.identified = true;
    const auto wrap = [&](const auto& frame) {
        return buildSerialData(context.localSid, context.remoteSid, 0, 1, frame);
    };
    const std::array approved{civ({0x19,0}), civ({0x0F}), civ({0x1C,3}), civ({3}), civ({4}),
                              civ({0x26,0}), civ({0x21,0}), civ({0x21,1}), civ({0x21,2})};
    int mutations = 0;
    for (const auto& frame : approved) {
        check(policy.allowsCiv(frame), "exact approved query");
        check(policy.allowsDatagram(wrap(frame), context), "approved query at UDP boundary");
        for (int byte = 0; byte < 256; ++byte) {
            auto write = frame;
            write.insert(write.end() - 1, static_cast<std::uint8_t>(byte));
            check(!policy.allowsCiv(write), "additional data byte is not a query");
            check(!policy.allowsDatagram(wrap(write), context), "setting cannot cross UDP boundary");
            ++mutations;
        }
        for (std::size_t field : {std::size_t(0), std::size_t(1), std::size_t(2), std::size_t(3)}) {
            auto foreign = frame; foreign[field] ^= 1;
            check(!policy.allowsDatagram(wrap(foreign), context), "foreign envelope");
        }
        auto invalid = frame; invalid.back() = 0xFC;
        check(!policy.allowsDatagram(wrap(invalid), context), "FC terminator");
        invalid = frame; invalid.insert(invalid.end(), frame.begin(), frame.end());
        check(!policy.allowsDatagram(wrap(invalid), context), "concatenated CI-V");
    }
    for (const auto& frame : {civ({0x0F,1}), civ({0x1C,0,0}), civ({0x1C,0,1}), civ({0x1C,2,2}),
                              civ({0x1C,4,1}), civ({0x17,0xFD}), civ({0x18,1}), civ({5,0,0,0,0,0}),
                              civ({7,0}), civ({0x27,0x10,1}), civ({0x21,0,0,0,0})}) {
        check(!policy.allowsDatagram(wrap(frame), context), "PTT, CW, tuner, VFO, wake, scope, offset denied");
    }
    auto datagram = wrap(civ({0x0F}));
    for (std::size_t field : {std::size_t(0), std::size_t(8), std::size_t(12), std::size_t(17)}) {
        auto invalid = datagram; invalid[field] ^= 1;
        check(!policy.allowsDatagram(invalid, context), "bad length or SID");
    }
    context.identified = false;
    check(!policy.allowsDatagram(datagram, context), "no polling before identity");
    check(policy.allowsDatagram(wrap(civ({0x19,0})), context), "directed ID before polling");
    context.identified = true;
    check(!policy.allowsDatagram(buildSerialRestart(context.localSid, context.remoteSid, 0), context),
          "no automatic pipe restart");
    check(policy.allowsDatagram(buildSerialOpen(context.localSid, context.remoteSid, 0, true), context),
          "open own CI-V pipe");
    context.role = IcomOutboundPolicy::Role::Audio;
    for (const auto& pcm : {std::vector<std::uint8_t>{}, std::vector<std::uint8_t>(556),
                           std::vector<std::uint8_t>(1364, 0x55)}) {
        check(!policy.allowsDatagram(buildAudio(context.localSid, context.remoteSid, 0, 1, pcm), context),
              "all audio payloads denied including silence and empty header");
    }
    for (auto role : {IcomOutboundPolicy::Role::Control, IcomOutboundPolicy::Role::Serial,
                     IcomOutboundPolicy::Role::Audio}) {
        context.role = role;
        for (const auto& packet : {buildIdle(context.localSid, context.remoteSid, 1),
                                  buildPingRequest(context.localSid, context.remoteSid, 1, {}),
                                  buildRetransmitRequest(context.localSid, context.remoteSid, 1),
                                  buildDisconnect(context.localSid, context.remoteSid)}) {
            check(policy.allowsDatagram(packet, context), "own transport maintenance");
        }
        context.ready = context.remoteKnown = false;
        check(policy.allowsDatagram(buildAreYouThere(context.localSid), context), "first handshake");
        check(!policy.allowsDatagram(datagram, context), "payload before handshake denied");
        context.remoteKnown = true;
        check(policy.allowsDatagram(buildAreYouThere(context.localSid), context), "own handshake retry after IAmHere");
        check(policy.allowsDatagram(buildAreYouReady(context.localSid, context.remoteSid), context), "ready handshake");
        check(policy.allowsDatagram(buildPingRequest(context.localSid, context.remoteSid, 1, {}), context),
              "own peer ping while waiting for IAmReady");
        context.ready = true;
    }
    context.role = IcomOutboundPolicy::Role::Control;
    StreamRequest request;
    request.localSid = context.localSid; request.remoteSid = context.remoteSid;
    request.enableTx = false;
    auto rx = buildStreamRequest(request);
    std::vector<std::vector<std::uint8_t>> permits{rx};
    context.controlPermits = permits;
    check(policy.allowsDatagram(rx, context), "approved RX-only negotiation");
    check(rx[0x71] == 0 && rx[0x73] == 0, "TX disabled and TX codec absent");
    request.enableTx = true;
    auto tx = buildStreamRequest(request);
    permits[0] = tx;
    check(!policy.allowsDatagram(tx, context), "TX negotiation denied even if registered");
    context.controlPermits = {};
    check(!policy.allowsDatagram(rx, context), "unregistered control payload denied");
    auto login = buildLogin(context.localSid, context.remoteSid, 1, 2, "test", "test");
    permits = {login}; context.controlPermits = permits;
    check(policy.allowsDatagram(login, context), "canonical own login");
    login[0x40] ^= 1;
    check(!policy.allowsDatagram(login, context), "tampered control payload denied");
    check(!IcomOutboundPolicy{}.allowsDatagram(rx, context), "absent policy fails closed");
    check(!IcomOutboundPolicy{}.allowsCiv(civ({0x0F})), "absent CI-V policy fails closed");
    check(IcomOutboundPolicy::production().allowsDatagram(tx, context), "production path unchanged");
    std::printf("policy: 9 exact queries; %d added-data mutations; failures=%d\n", mutations, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
