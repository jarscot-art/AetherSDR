// Replays the observed initialization order through the real control stream
// dispatcher and final writer, without creating a UDP socket or fake radio.
#include "core/backends/icom/IcomSession.h"
#include <QCoreApplication>
#include <algorithm>
#include <chrono>
#include <cstdio>

using namespace AetherSDR::icom;
using Packet = std::vector<std::uint8_t>;
using Packets = std::vector<Packet>;
namespace {
int failures = 0;
void check(bool ok, const char* label)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) { ++failures; }
}
constexpr std::uint32_t local = 0x11223344, remote = 0x55667788;
const AuthId auth{1,2,3,4,5,6}, rotated{9,8,7,6,5,4};
const RadioId radio{7,8,9};
Packet envelope(std::size_t size, std::uint16_t type = 0)
{
    Packet p(size, 0);
    writeHeader(p, {static_cast<std::uint32_t>(size), type, 3, remote, local});
    return p;
}
Packet login(const AuthId& id = auth)
{
    auto p = envelope(kLenLoginReply); p[0x13] = 0x50; p[0x14] = 2;
    std::copy(id.begin(), id.end(), p.begin() + 0x1a);
    return p;
}
Packet caps()
{
    auto p = envelope(kLenCapabilities); p[0x13] = 0x98; p[0x14] = 3;
    std::copy(radio.begin(), radio.end(), p.begin() + 0x42);
    p[0x94] = 0xB6;
    return p;
}
Packet token(const AuthId& id = auth, bool rotate = false)
{
    auto p = buildAuth(remote, local, 2, id, AuthKind::Renew); p[0x14] = 2;
    if (rotate) { std::fill(p.begin() + 0x30, p.begin() + 0x34, 0xff); }
    return p;
}
Packet usage(const AuthId& id = auth, const std::string& owner = {})
{
    auto p = envelope(kLenConnInfo); p[0x13] = 0x80; p[0x14] = 3;
    std::copy(id.begin(), id.end(), p.begin() + 0x1a);
    std::copy(radio.begin(), radio.end(), p.begin() + 0x20);
    if (!owner.empty()) {
        p[0x60] = 1;
        std::copy(owner.begin(), owner.end(), p.begin() + 0x64);
        p[0x84] = 192; p[0x85] = 168; p[0x86] = 1; p[0x87] = 2;
    }
    return p;
}
std::size_t requests(const Packets& writes)
{
    return static_cast<std::size_t>(std::count_if(writes.begin(), writes.end(), [](const auto& p) {
        return p.size() == kLenConnInfo && p[0x14] == 1 && p[0x15] == 3;
    }));
}
}
namespace AetherSDR::icom {
struct IcomStreamTestAccess {
    static void attach(IcomStream& s, IcomStream::Role role, Packets& writes)
    {
        s.m_config.role = role; s.m_localSid = local;
        s.m_activityClock.start();
        s.m_testWriter = [&writes](auto p) { writes.emplace_back(p.begin(), p.end()); };
    }
    static void receive(IcomStream& s, const Packet& p)
    {
        s.handleDatagram(QByteArray(reinterpret_cast<const char*>(p.data()),
                                   static_cast<qsizetype>(p.size())));
    }
    static bool noSocket(const IcomStream& s) { return s.m_socket == nullptr; }
};
struct IcomCivBackendTestAccess {
    static void setup(IcomSession& s, Packets& writes)
    {
        s.m_control = s.makeStream(); s.m_serial = s.makeStream(); s.m_audio = s.makeStream();
        IcomStreamTestAccess::attach(*s.m_control, IcomStream::Role::Control, writes);
        IcomStreamTestAccess::attach(*s.m_serial, IcomStream::Role::Serial, writes);
        IcomStreamTestAccess::attach(*s.m_audio, IcomStream::Role::Audio, writes);
        s.m_tokenRequestId = 0x0201;
        s.m_params.civAddress = 0xB6;
        QObject::connect(s.m_control, &IcomStream::ready, &s, &IcomSession::onControlReady);
        QObject::connect(s.m_control, &IcomStream::payloadReady, &s, &IcomSession::onControlPayload);
        receive(s, envelope(kHeaderSize, static_cast<std::uint16_t>(PacketType::IAmHere)));
        receive(s, envelope(kHeaderSize, static_cast<std::uint16_t>(PacketType::AreYouReady)));
        // Only route discovery is injected: no socket is permitted in this test.
        s.m_auditOwnerIpv4 = 0xc0a80102;
    }
    static void receive(IcomSession& s, const Packet& p) { IcomStreamTestAccess::receive(*s.m_control, p); }
    static bool buffered(const IcomSession& s) { return !s.m_auditEarlyConnInfo.isEmpty(); }
    static bool available(const IcomSession& s) { return s.m_auditAvailable; }
    static bool granted(const IcomSession& s) { return s.m_streamGranted; }
    static bool failed(const IcomSession& s) { return s.m_failing; }
    static std::string owner(const IcomSession& s) { return s.m_auditOwnerName; }
    static void age(IcomSession& s, qint64 ms)
    {
        s.m_auditEarlyReceivedAt = std::chrono::steady_clock::now() - std::chrono::milliseconds(ms);
    }
    static auto receivedAt(const IcomSession& s) { return s.m_auditEarlyReceivedAt; }
    static bool socketFree(const IcomSession& s)
    {
        return IcomStreamTestAccess::noSocket(*s.m_control)
            && IcomStreamTestAccess::noSocket(*s.m_serial)
            && IcomStreamTestAccess::noSocket(*s.m_audio);
    }
};
}
using Access = IcomCivBackendTestAccess;
namespace {
struct Fixture {
    Packets writes;
    IcomSession session{nullptr, IcomOutboundPolicy::rxAudit()};
    Fixture() { Access::setup(session, writes); }
    void receive(const Packet& p) { Access::receive(session, p); }
    void authenticate() { receive(caps()); receive(token()); }
};
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    {
        Fixture f;
        check(f.writes.size() == 3 && f.writes.back().size() == kLenLogin,
              "real handshake dispatch emits two ready packets and one login");
        f.receive(login());
        check(f.writes.size() == 5 && f.writes.back()[0x15] == 5
              && f.writes.back()[0x16] == 0 && f.writes.back()[0x17] == 2,
              "login emits first auth then token request with sequence 2");
        f.receive(usage());
        check(Access::buffered(f.session) && !Access::available(f.session) && requests(f.writes) == 0,
              "early conninfo is retained as bytes, not permission");
        f.receive(caps());
        check(requests(f.writes) == 0, "capabilities alone cannot authorize pending availability");
        f.receive(token());
        check(requests(f.writes) == 1 && !Access::buffered(f.session)
              && f.writes.back()[0x71] == 0 && f.writes.back()[0x73] == 0,
              "token accepted revalidates early conninfo and sends exactly one RX-only request");
        f.receive(usage(auth, Access::owner(f.session)));
        check(Access::granted(f.session) && !Access::failed(f.session),
              "observed handshake/login/conninfo/capabilities/token/request/grant sequence completes");
        f.receive(usage(auth, Access::owner(f.session))); f.receive(token()); f.receive(caps());
        check(requests(f.writes) == 1 && !Access::failed(f.session),
              "repeated grant/token/capabilities never resend the stream request");
        check(Access::socketFree(f.session) && f.session.deniedDatagrams() == 0,
              "exact initialization sequence uses final writer with zero sockets and zero denied datagrams");
    }
    {
        Fixture f; f.receive(login()); f.receive(usage()); f.receive(token());
        check(requests(f.writes) == 0, "token before capabilities cannot authorize replay early");
        f.receive(caps());
        check(requests(f.writes) == 1, "capabilities arriving last also revalidates retained conninfo");
    }
    for (int offset : {0x08, 0x0c, 0x1a, 0x1f, 0x20, 0x2f}) {
        Fixture f; f.receive(login()); auto p = usage(); p[offset] ^= 1;
        f.receive(p); f.authenticate();
        check(requests(f.writes) == 0 && !Access::available(f.session),
              "stale SID or mismatched AuthId/RadioId in early notification never authorizes streams");
        f.receive(usage());
        check(requests(f.writes) == 1, "fresh fully correlated confirmation recovers after rejected early data");
    }
    {
        Fixture f; f.receive(login()); f.receive(usage());
        Access::age(f.session, 100); const auto first = Access::receivedAt(f.session);
        auto duplicate = usage(); duplicate[6] = 99; duplicate[0x17] = 55;
        f.receive(duplicate);
        check(Access::receivedAt(f.session) == first, "duplicates with changed sequence fields do not refresh age");
        Access::age(f.session, 5001); f.authenticate();
        check(requests(f.writes) == 0 && !Access::buffered(f.session), "expired early data is consumed without permission");
        f.receive(usage());
        check(requests(f.writes) == 1, "fresh post-auth notification can recover after early-data expiry");
    }
    for (bool earlyUsesNewToken : {false, true}) {
        Fixture f; f.receive(login()); f.receive(usage(earlyUsesNewToken ? rotated : auth));
        f.receive(caps()); f.receive(token(rotated, true));
        check(requests(f.writes) == (earlyUsesNewToken ? 1u : 0u),
              "token rotation rechecks cached bytes against final AuthId without rewriting them");
        if (!earlyUsesNewToken) {
            f.receive(usage(auth));
            check(requests(f.writes) == 0, "old-token notification after rotation remains untrusted");
            f.receive(usage(rotated));
        }
        f.receive(usage(rotated, Access::owner(f.session)));
        check(requests(f.writes) == 1 && Access::granted(f.session), "current rotated token can complete a correlated grant");
    }
    {
        Fixture f; f.receive(login()); f.receive(usage(auth, "another-client")); f.authenticate();
        check(Access::failed(f.session) && requests(f.writes) == 0,
              "early true Busy is fully validated then refuses without claiming streams");
        f.receive(usage());
        check(requests(f.writes) == 0, "late availability cannot bypass a Busy refusal");
    }
    for (bool busyFirst : {false, true}) {
        Fixture f; f.receive(login());
        f.receive(busyFirst ? usage(auth, "another-client") : usage());
        f.receive(busyFirst ? usage() : usage(auth, "another-client"));
        f.authenticate();
        check(Access::failed(f.session) && requests(f.writes) == 0,
              "conflicting early notifications fail closed in either order");
    }
    {
        Fixture f; f.receive(login()); auto p = usage(); p[0x61] = 1;
        f.receive(p); f.authenticate();
        check(Access::failed(f.session) && requests(f.writes) == 0, "malformed early notification is not buffered as availability");
    }
    {
        Fixture f; f.receive(usage()); f.receive(login()); f.authenticate();
        check(!Access::buffered(f.session) && requests(f.writes) == 0, "pre-login data cannot seed the new login's availability");
    }
    {
        Fixture f; f.receive(login()); f.authenticate();
        check(requests(f.writes) == 0 && !Access::granted(f.session), "no conninfo means no inferred availability or stream request");
    }
    {
        Fixture f; f.receive(login()); f.receive(usage());
        auto stale = caps(); stale[0x08] ^= 1; f.receive(stale); f.receive(token());
        check(requests(f.writes) == 0, "stale capabilities cannot complete deferred correlation");
        f.receive(caps()); check(requests(f.writes) == 1, "current capabilities finish pending correlation");
    }
    {
        Fixture f; auto stale = login(); stale[0x0c] ^= 1; f.receive(stale); f.receive(usage());
        check(!Access::buffered(f.session), "stale login cannot enable early buffering");
        stale = login(); stale[0x1a] ^= 1; f.receive(stale); f.receive(usage());
        check(!Access::buffered(f.session), "wrong login request ID cannot enable early buffering");
        f.receive(login()); f.receive(usage()); f.receive(login(rotated)); f.authenticate();
        check(requests(f.writes) == 1, "repeated login does not replace current token or retained evidence");
    }
    {
        Fixture f; f.receive(login()); f.receive(usage()); f.session.stop();
        check(!Access::buffered(f.session) && !Access::receivedAt(f.session), "stop discards buffered bytes and monotonic age");
        Access::setup(f.session, f.writes); f.receive(login()); f.authenticate();
        check(requests(f.writes) == 0, "reconnect cannot reuse previous availability even with reused fixture SIDs");
    }
    std::printf("init regression failures=%d; no sockets bound\n", failures);
    return failures ? 1 : 0;
}
