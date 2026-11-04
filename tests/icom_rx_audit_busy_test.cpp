// Actual session dispatcher + final UDP writer seam; no sockets are bound.
#include "core/backends/icom/IcomSession.h"
#include <QCoreApplication>
#include <algorithm>
#include <cstdio>

using namespace AetherSDR::icom;
using Packets = std::vector<std::vector<std::uint8_t>>;
namespace {
int failures = 0;
void check(bool ok, const char* label)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) { ++failures; }
}
constexpr std::uint32_t local = 0x11223344, remote = 0x55667788;
const AuthId auth{1,2,3,4,5,6};
const RadioId radio{7,8,9};
const std::string owner = "HF5A-0123456789";
}

namespace AetherSDR::icom {
struct IcomStreamTestAccess {
    static void attach(IcomStream& s, IcomStream::Role role, Packets& writes)
    {
        s.m_config.role = role;
        s.m_localSid = local; s.m_remoteSid = remote;
        s.m_ready = s.m_gotRemoteSid = true;
        s.m_activityClock.start();
        s.m_testWriter = [&writes](auto p) { writes.emplace_back(p.begin(), p.end()); };
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
        s.m_authOk = s.m_haveRadioId = true;
        s.m_authId = auth; s.m_radioId = radio;
        s.m_auditOwnerName = owner; s.m_auditOwnerIpv4 = 0xc0a80102;
        s.m_params.enableTx = true; // immutable audit policy must still suppress TX
        s.m_params.civAddress = 0xB6;
    }
    static void receive(IcomSession& s, const std::vector<std::uint8_t>& p)
    {
        s.onControlPayload(QByteArray(reinterpret_cast<const char*>(p.data()),
                                      static_cast<qsizetype>(p.size())));
    }
    static void request(IcomSession& s) { s.requestStreamsIfReady(); }
    static void login(IcomSession& s) { s.onControlReady(); }
    static bool granted(const IcomSession& s) { return s.m_streamGranted; }
    static bool failed(const IcomSession& s) { return s.m_failing; }
    static bool cleared(const IcomSession& s)
    {
        return !s.m_auditAvailable && s.m_auditOwnerName.empty() && !s.m_auditOwnerIpv4;
    }
    static bool socketFree(const IcomSession& s)
    {
        return IcomStreamTestAccess::noSocket(*s.m_control)
            && IcomStreamTestAccess::noSocket(*s.m_serial)
            && IcomStreamTestAccess::noSocket(*s.m_audio);
    }
    static AuthId token(const IcomSession& s) { return s.m_authId; }
};
}
using Access = IcomCivBackendTestAccess;
namespace {
std::vector<std::uint8_t> notification(bool busy)
{
    std::vector<std::uint8_t> p(kLenConnInfo, 0);
    writeHeader(p, {kLenConnInfo, 0, 3, remote, local});
    p[0x13] = 0x80; p[0x14] = 3;
    std::copy(auth.begin(), auth.end(), p.begin() + 0x1a);
    std::copy(radio.begin(), radio.end(), p.begin() + 0x20);
    if (busy) {
        p[0x60] = 1;
        std::copy(owner.begin(), owner.end(), p.begin() + 0x64);
        p[0x84] = 192; p[0x85] = 168; p[0x86] = 1; p[0x87] = 2;
    }
    return p;
}
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    {
        Packets writes;
        IcomSession s(nullptr, IcomOutboundPolicy::rxAudit()); Access::setup(s, writes);
        Access::request(s);
        check(writes.empty(), "no stream claim before correlated owner-free notification");
        Access::receive(s, notification(false));
        check(writes.size() == 1 && writes[0].size() == kLenConnInfo
              && writes[0][0x71] == 0 && writes[0][0x73] == 0,
              "availability sends exactly one RX-only stream request at terminal writer");
        Access::receive(s, notification(true));
        check(Access::granted(s) && !Access::failed(s) && writes.size() == 1,
              "correlated own Busy is grant, not occupied-session failure");
        Access::receive(s, notification(true));
        check(writes.size() == 1 && !Access::failed(s), "duplicate own status does not reopen streams");
        check(Access::socketFree(s), "complete negotiation test never binds UDP");
        Access::receive(s, notification(false));
        check(Access::failed(s), "current owner-free notification after grant releases session");
        s.stop();
        check(Access::cleared(s), "stop clears ownership evidence before reconnect");
    }
    for (const int offset : {0x08, 0x0c, 0x1a, 0x1f, 0x20, 0x2f}) {
        Packets writes;
        IcomSession s(nullptr, IcomOutboundPolicy::rxAudit()); Access::setup(s, writes);
        auto p = notification(false); p[offset] ^= 1;
        Access::receive(s, p);
        check(writes.empty(), "foreign availability cannot trigger a stream request");
        Access::receive(s, notification(false));
        p = notification(true); p[offset] ^= 1; p[0x64] = 'x';
        Access::receive(s, p);
        check(!Access::failed(s) && !Access::granted(s) && writes.size() == 1
              && Access::token(s) == auth,
              "stale SID or mismatched AuthId/RadioId neither fails nor grants nor adopts token");
        Access::receive(s, notification(true));
        check(Access::granted(s), "valid reply after stale Busy can complete pending request");
    }
    for (bool pending : {false, true}) {
        for (int offset : {0x64, 0x87}) {
            Packets writes;
            IcomSession s(nullptr, IcomOutboundPolicy::rxAudit()); Access::setup(s, writes);
            if (pending) { Access::receive(s, notification(false)); }
            const auto before = writes.size();
            auto p = notification(true); p[offset] ^= 1;
            Access::receive(s, p);
            check(Access::failed(s) && !Access::granted(s) && writes.size() == before,
                  "real Busy before/after stream request blocks other owner on same IP or another IP");
            Access::receive(s, notification(true));
            check(!Access::granted(s), "terminal Busy cannot be bypassed by a following own-looking grant");
        }
    }
    for (int offset : {0x13, 0x14, 0x15, 0x61, 0x64, 0x84}) {
        Packets writes;
        IcomSession s(nullptr, IcomOutboundPolicy::rxAudit()); Access::setup(s, writes);
        Access::receive(s, notification(false));
        auto p = notification(true);
        if (offset == 0x84) { std::fill(p.begin() + 0x84, p.begin() + 0x88, 0); }
        else if (offset == 0x64) { p[offset] = 0; }
        else { p[offset] ^= 1; }
        Access::receive(s, p);
        check(Access::failed(s) && !Access::granted(s) && writes.size() == 1,
              "ambiguous current packet fails closed without opening media");
    }
    {
        Packets writes;
        IcomSession s(nullptr, IcomOutboundPolicy::rxAudit()); Access::setup(s, writes);
        Access::receive(s, notification(true));
        check(Access::failed(s) && !Access::granted(s) && writes.empty(),
              "own-looking unsolicited grant before availability cannot claim radio");
    }
    {
        Packets writes;
        IcomSession s(nullptr, IcomOutboundPolicy::rxAudit()); Access::setup(s, writes);
        Access::login(s);
        const auto first = writes.back();
        Access::login(s);
        const auto second = writes.back();
        check(first.size() == kLenLogin && first[0x6f] == 0 && second[0x6f] == 0
              && !std::equal(first.begin() + 0x60, first.begin() + 0x70, second.begin() + 0x60),
              "each login gets a fresh bounded owner label at actual final writer");
    }
    std::printf("failures=%d\n", failures);
    return failures ? 1 : 0;
}
