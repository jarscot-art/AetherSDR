#include "core/backends/icom/IcomCivBackend.h"
#include "core/backends/icom/IcomSession.h"
#include "core/backends/icom/IcomStream.h"
#include "core/backends/icom/CivCodec.h"
#include "TestSettingsProfile.h"

#include <QCoreApplication>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <set>
#include <vector>

using namespace AetherSDR::icom;
using Packets = std::vector<std::vector<std::uint8_t>>;
namespace {
int failures = 0;
void check(bool condition, const char* label)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition) { ++failures; }
}
constexpr std::uint32_t localSid = 0x12345678, remoteSid = 0x87654321;
}
namespace AetherSDR::icom {
struct IcomStreamTestAccess {
    static void prepare(IcomStream& s, Packets& writes, bool identified)
    {
        s.m_config.role = IcomStream::Role::Serial;
        s.m_localSid = localSid; s.m_remoteSid = remoteSid;
        s.m_gotRemoteSid = s.m_ready = true;
        s.m_auditIdentified = identified;
        s.m_activityClock.start();
        s.m_testWriter = [&writes](auto bytes) { writes.emplace_back(bytes.begin(), bytes.end()); };
    }
};
struct IcomCivBackendTestAccess {
    static void prepare(IcomCivBackend& b, Packets& writes, bool startup = false)
    {
        b.m_session = std::make_unique<IcomSession>(nullptr, b.m_outboundPolicy);
        b.m_session->m_params.civAddress = 0xB6;
        b.m_session->m_serial = b.m_session->makeStream();
        IcomStreamTestAccess::prepare(*b.m_session->m_serial, writes, !startup);
        b.m_civAddressPinned = true; b.m_civSeedAddress = 0xB6;
        QObject::connect(b.m_session.get(), &IcomSession::civFrameReady, &b,
                         [&b](const CivFrame& f) { b.onCivFrame(f, b.m_sessionGeneration); });
        if (startup) { b.onSessionConnected(QStringLiteral("IC-7300MK2")); }
        else {
            b.m_connected = true; b.m_model = modelForId(0xB6);
            b.m_civReported = b.m_civModelId = 0xB6;
            b.m_mode = CivMode::Usb; b.m_frequencyHz = 7'074'000;
        }
    }
    static void receive(IcomCivBackend& b, std::initializer_list<std::uint8_t> body)
    {
        std::vector<std::uint8_t> frame{0xFE,0xFE,0xE0,0xB6};
        frame.insert(frame.end(), body); frame.push_back(0xFD);
        const auto packet = buildSerialData(remoteSid, localSid, 1, 1, frame);
        b.m_session->onSerialPayload(QByteArray(reinterpret_cast<const char*>(packet.data()),
                                               static_cast<int>(packet.size())));
    }
    static void tick(IcomCivBackend& b)
    {
        b.m_splitTxPollAtMs = -1; // make the periodic reads due without sleeping
        b.onLinkTick(); b.onMeterTick();
    }
    static void drain(IcomCivBackend& b)
    {
        // Exercise timeout recovery and the real terminal writer, without sockets.
        auto now = std::max<qint64>(b.nowMs(), b.m_civScheduler.stats().lastDispatchMs);
        for (int i = 0; i < 50; ++i) { b.pumpCiv(now += 1000); }
    }
    static QVariantMap readback(const IcomCivBackend& b)
    { return b.splitTxReadbackDiagnostics(b.nowMs(), true); }
    static std::set<std::vector<std::uint8_t>> queued(const IcomCivBackend& b)
    {
        std::set<std::vector<std::uint8_t>> out;
        for (const auto& q : b.m_civScheduler.m_queue) { out.insert(q.request.frame); }
        for (const auto& entry : b.m_civTrace) {
            if (!entry.outbound) { continue; }
            std::vector<std::uint8_t> bytes;
            for (const auto& octet : entry.hex.split(QLatin1Char(' '))) {
                bytes.push_back(static_cast<std::uint8_t>(octet.toUInt(nullptr, 16)));
            }
            out.insert(bytes);
        }
        return out;
    }
    static void forbidden(IcomCivBackend& b)
    {
        b.queueRead(cmdReadFilterWidth(0xB6), "filter", IcomCivScheduler::Priority::Control);
        b.queueWrite(cmdSetPtt(0xB6, true), "ptt", IcomCivScheduler::Priority::Control);
        b.queueEmergencyWriteNoReply(cmdSetPtt(0xB6, false), "ptt");
        b.sendUserCommand(cmdScopeOnOff(0xB6, true));
        b.invokeExtension(QStringLiteral("icom"), QStringLiteral("power.wake"), 1, {});
    }
};
}
using Access = IcomCivBackendTestAccess;
static qulonglong denied(const IcomCivBackend& b)
{ return b.rxAuditDiagnostics().value(QStringLiteral("deniedRequests")).toULongLong(); }

int main(int argc, char** argv)
{
    TestSettingsProfile profile(QStringLiteral("icom-rx-audit-requests"));
    if (!profile.isValid()) { return 1; }
    QCoreApplication app(argc, argv);
    {
        Packets writes;
        IcomCivBackend b(nullptr, IcomOutboundPolicy::rxAudit());
        Access::prepare(b, writes);
        for (int i = 0; i < 11; ++i) { Access::receive(b, {0x26,0,1,1,1}); }
        std::printf("EVIDENCE mode-response deniedRequests=%llu\n", denied(b));
        check(denied(b) == 0, "eleven USB-D reports generate no forbidden passband/PBT/TBW reads");
        Access::drain(b);
        check(b.rxAuditDiagnostics().value("deniedDatagrams").toULongLong() == 0,
              "automatic follow-ups do not reach or trip terminal guard");
    }
    {
        Packets writes;
        IcomCivBackend b(nullptr, IcomOutboundPolicy::production());
        Access::prepare(b, writes);
        Access::receive(b, {0x26,0,1,1,1});
        const auto queries = Access::queued(b);
        check(queries.contains(cmdReadFilterWidth(0xB6))
              && queries.contains(buildFrameSub(0xB6,0x14,0x07))
              && queries.contains(buildFrameSub(0xB6,0x14,0x08))
              && queries.contains(cmdReadSetting(0xB6,17)),
              "production still requests width, both PBTs and DATA TX bandwidth");
    }
    {
        Packets writes;
        IcomCivBackend b(nullptr, IcomOutboundPolicy::rxAudit());
        Access::prepare(b, writes, true);
        Access::receive(b, {0x19,0,0xB6});
        for (int i = 0; i < 20; ++i) {
            Access::tick(b);
            Access::receive(b, {0x03,0,0x40,0x07,0x07,0});
            Access::receive(b, {0x04,1,1});
            Access::receive(b, {0x26,0,1,static_cast<std::uint8_t>(i % 2),1});
            Access::receive(b, {0x21,0,0,0,0});
            Access::receive(b, {0x21,1,0});
            Access::receive(b, {0x21,2,0});
            Access::receive(b, {0x16,0x58,0}); // unsolicited TX bandwidth report
            Access::receive(b, {0x27,0x10,1}); // unsolicited scope report
            Access::receive(b, {0x0F,0});
            Access::receive(b, {0x1C,3,0,0x40,0x07,0x07,0});
        }
        const auto state = Access::readback(b);
        check(state.value("split").toMap().value("valid").toBool()
              && state.value("txFrequencyHz").toMap().value("valid").toBool()
              && state.value("txFrequencyHz").toMap().value("value").toULongLong() == 7'074'000,
              "normal decoded cycle retains valid SPLIT and 7074000 Hz TX readback");
        check(denied(b) == 0, "connect, polling, mode changes and unsolicited reports cause zero request denials");
        Access::tick(b); // stable context: request the two samples cancelled by the last DATA change
        Access::drain(b);
        std::set<std::vector<std::uint8_t>> emitted;
        for (const auto& packet : writes) {
            if (isSerialData(packet)) {
                const auto bytes = serialPayload(packet);
                emitted.emplace(bytes.begin(), bytes.end());
                check(IcomOutboundPolicy::rxAudit().allowsCiv(bytes), "actual final-writer bytes are approved queries");
            }
        }
        check(emitted.size() == 11, "nine baseline and two unselected VFO queries reach the writer");
        check(emitted.contains(cmdReadUnselectedVfoFrequency(0xB6))
              && emitted.contains(cmdReadUnselectedVfoMode(0xB6)),
              "unselected VFO polling uses the two exact approved queries");
        const auto before = writes.size();
        Access::forbidden(b);
        check(denied(b) == 5 && writes.size() == before,
              "all five denial entry points count and block explicit forbidden operations");
        Access::receive(b, {0x26,0,1,1,1});
        Access::tick(b);
        check(denied(b) == 5, "later normal polling neither adds spurious denials nor resets genuine ones");
        b.disconnectRadio();
        check(denied(b) == 5, "disconnect neither generates forbidden cleanup nor clears counter");
        for (const auto& packet : writes) {
            check(!isSerialData(packet) || IcomOutboundPolicy::rxAudit().allowsCiv(serialPayload(packet)),
                  "teardown never leaks a CI-V write");
        }
        std::printf("EVIDENCE normal-cycle deniedRequests=0 explicit-denials=%llu final-writer-packets=%zu\n",
                    denied(b), writes.size());
    }
    std::printf("RX audit request failures=%d; no sockets bound\n", failures);
    return failures ? 1 : 0;
}
