#include "core/backends/icom/IcomStream.h"
#include "core/backends/icom/IcomSession.h"
#include "core/backends/icom/IcomCivBackend.h"
#include "core/backends/icom/CivCodec.h"
#include "TestSettingsProfile.h"
#include "TxTestAuthority.h"

#include <QCoreApplication>
#include <QPointer>
#include <QTimer>
#include <cstdio>
#include <cstdint>
#include <memory>
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
constexpr std::uint32_t localSid = 0x12345678;
constexpr std::uint32_t remoteSid = 0x87654321;
}
namespace AetherSDR::icom {
struct IcomStreamTestAccess {
    static void prepare(IcomStream& stream, IcomStream::Role role, Packets& writes, bool identified = true)
    {
        stream.m_config.role = role;
        stream.m_localSid = localSid;
        stream.m_remoteSid = remoteSid;
        stream.m_gotRemoteSid = stream.m_ready = true;
        stream.m_auditIdentified = identified;
        stream.m_activityClock.start();
        stream.m_testWriter = [&writes](auto packet) { writes.emplace_back(packet.begin(), packet.end()); };
    }
    static void replay(IcomStream& stream, quint16 seq)
    {
        stream.handleRetransmitRequest(buildRetransmitRequest(remoteSid, localSid, seq));
    }
    static quint16 sequence(const IcomStream& stream) { return stream.m_txSeq; }
    static void remote(IcomStream& stream, quint32 sid) { stream.m_remoteSid = sid; }
};
struct IcomCivBackendTestAccess {
    static void session(IcomSession& session, Packets& writes)
    {
        session.m_control = session.makeStream();
        session.m_serial = session.makeStream();
        session.m_audio = session.makeStream();
        check(session.m_control->isRxAudit() && session.m_serial->isRxAudit() && session.m_audio->isRxAudit(),
              "real session factory propagates immutable audit policy to all streams");
        IcomStreamTestAccess::prepare(*session.m_control, IcomStream::Role::Control, writes);
        IcomStreamTestAccess::prepare(*session.m_serial, IcomStream::Role::Serial, writes, false);
        IcomStreamTestAccess::prepare(*session.m_audio, IcomStream::Role::Audio, writes);
        session.m_params.enableTx = true; // deliberately wrong input must not grant audit TX
        session.m_params.civAddress = 0xB6;
        session.m_authOk = session.m_haveRadioId = true;
        session.m_authId = {1,2,3,4,5,6};
    }
    static void negotiate(IcomSession& session) { session.requestStreamsIfReady(); }
    static void firstAuth(IcomSession& session)
    {
        session.sendControl(buildAuth(localSid, remoteSid, 1, session.m_authId, AuthKind::First));
        session.sendRenewal(QStringLiteral("offline"));
    }
    static void login(IcomSession& session) { session.onControlReady(); }
    static void txPump(IcomSession& session) { session.onAudioReady(); session.onTxPump(); }
    static bool noTxTimer(const IcomSession& session) { return session.m_txTimer == nullptr; }
    static void identify(IcomSession& session)
    {
        const std::vector<std::uint8_t> reply{0xFE,0xFE,0xE0,0xB6,0x19,0,0xB6,0xFD};
        const auto packet = buildSerialData(remoteSid, localSid, 1, 1, reply);
        session.onSerialPayload(QByteArray(reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size())));
    }
    static void busy(IcomSession& session)
    {
        std::vector<std::uint8_t> packet(kLenConnInfo);
        writeHeader(packet, {kLenConnInfo, 0, 0, remoteSid, localSid});
        packet[0x13] = 0x80; packet[0x14] = 3; packet[0x15] = 0; packet[0x60] = 1;
        session.onControlPayload(QByteArray(reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size())));
    }
    static bool failing(const IcomSession& session) { return session.m_failing; }
    static void backend(IcomCivBackend& backend, Packets& writes)
    {
        backend.m_session = std::make_unique<IcomSession>(nullptr, IcomOutboundPolicy::rxAudit());
        session(*backend.m_session, writes);
        identify(*backend.m_session);
        backend.m_connected = true;
        backend.m_model = modelForId(0xB6);
        backend.m_civReported = backend.m_civModelId = 0xB6;
    }
    static void forbidden(IcomCivBackend& backend)
    {
        backend.queueEmergencyWriteNoReply(cmdSetPtt(0xB6, false), "ptt");
        backend.sendUserCommand(cmdSetPtt(0xB6, true));
        backend.sendUserCommand(cmdAbortCwMessage(0xB6));
        backend.sendUserCommand(cmdScopeOnOff(0xB6, true));
    }
    static void startScope(IcomCivBackend& backend) { backend.applyScopeStartup(); }
    static void tick(IcomCivBackend& backend) { backend.onLinkTick(); backend.onMeterTick(); }
    static void burst(IcomCivBackend& backend) { backend.sendConnectReadBurst(); }
    static void scopeTraffic(IcomCivBackend& backend)
    {
        CivFrame frame; frame.cmd = cmd::kScope;
        backend.onCivFrame(frame, backend.m_sessionGeneration);
    }
    static IcomSession* current(IcomCivBackend& backend) { return backend.m_session.get(); }
    static void wireDisconnect(IcomCivBackend& backend)
    {
        QObject::connect(backend.m_session.get(), &IcomSession::disconnected,
                         &backend, &IcomCivBackend::onSessionDisconnected);
    }
};
}

int main(int argc, char** argv)
{
    TestSettingsProfile profile(QStringLiteral("icom-rx-audit-egress"));
    if (!profile.isValid()) { return 1; }
    QCoreApplication app(argc, argv);
    {
        Packets writes;
        IcomStream serial(nullptr, IcomOutboundPolicy::rxAudit());
        IcomStreamTestAccess::prepare(serial, IcomStream::Role::Serial, writes);
        const auto query = buildSerialData(localSid, remoteSid, 0, 1, cmdReadSplit(0xB6));
        serial.sendTracked(query);
        check(writes.size() == 1, "approved query reaches last writer");
        const auto firstSeq = parseHeader(writes.front()).seq;
        IcomStreamTestAccess::replay(serial, firstSeq);
        check(writes.size() == 2 && writes[0] == writes[1], "approved query retransmits unchanged");
        const auto before = writes.size();
        const auto seq = IcomStreamTestAccess::sequence(serial);
        TxTestAuthority authority;
        for (const auto& frame : {cmdSetPtt(0xB6, true), cmdSetPtt(0xB6, false),
                                  cmdAbortCwMessage(0xB6), cmdScopeOnOff(0xB6, true),
                                  cmdSetTransmitFrequencyCheck(0xB6, false)}) {
            const auto packet = buildSerialData(localSid, remoteSid, 0, 0, frame);
            serial.sendRaw(packet);
            serial.sendRawTwice(packet);
            serial.sendTracked(packet);
            serial.sendTrackedTxCommand(packet, {authority.operation, true, {}});
            serial.retainForTest(600, packet); // force the bypass that normal enqueue refuses
            IcomStreamTestAccess::replay(serial, 600);
        }
        check(writes.size() == before, "raw/tracked/emergency/authorized-TX/replay cannot leak PTT, OFF, CW, scope or XFC");
        std::printf("EVIDENCE serial: packets_before=%zu packets_after_forbidden=%zu denied=%llu\n",
                    before, writes.size(), static_cast<unsigned long long>(serial.deniedDatagrams()));
        check(IcomStreamTestAccess::sequence(serial) == seq, "denied writes consume no sequence");
        check(serial.deniedDatagrams() >= 25, "denials counted at actual egress boundary");
        IcomStreamTestAccess::remote(serial, remoteSid + 1);
        IcomStreamTestAccess::replay(serial, firstSeq);
        check(writes.size() == before, "old-session retained packet cannot cross new SID");
        serial.stop();
        check(writes.size() == before + 2
              && parseHeader(writes.back()).type == static_cast<std::uint16_t>(PacketType::Disconnect),
              "stop uses guarded writer and closes only own stream");
    }
    {
        Packets writes;
        IcomStream audio(nullptr, IcomOutboundPolicy::rxAudit());
        IcomStreamTestAccess::prepare(audio, IcomStream::Role::Audio, writes);
        TxTestAuthority authority;
        const auto packet = buildAudio(localSid, remoteSid, 0, 1, std::vector<std::uint8_t>(1364));
        audio.sendTrackedTxAudio(packet, authority.context);
        audio.sendTracked(packet);
        audio.sendRaw(packet);
        audio.retainForTest(99, packet);
        IcomStreamTestAccess::replay(audio, 99);
        check(writes.empty(), "audio including silence is denied even with a valid TX grant and replay");
        std::printf("EVIDENCE audio: packets=%zu denied=%llu\n", writes.size(),
                    static_cast<unsigned long long>(audio.deniedDatagrams()));
        check(audio.retainedSequences().size() == 1, "only deliberately injected forbidden replay exists");
        audio.stop();
        check(writes.size() == 2, "audio transport disconnect allowed without PCM");
    }
    {
        Packets writes;
        IcomSession session(nullptr, IcomOutboundPolicy::rxAudit());
        IcomCivBackendTestAccess::session(session, writes);
        IcomCivBackendTestAccess::login(session);
        IcomCivBackendTestAccess::firstAuth(session);
        IcomCivBackendTestAccess::negotiate(session);
        check(writes.size() == 4 && writes.back().size() == kLenConnInfo
              && writes.back()[0x71] == 0 && writes.back()[0x73] == 0,
              "canonical login/auth/renew/RX negotiation permitted; caller cannot enable TX");
        session.sendCiv(cmdReadTransmitFrequency(0xB6));
        check(writes.size() == 4, "non-ID query cannot leave before radio identification");
        IcomCivBackendTestAccess::identify(session);
        session.sendCiv(cmdReadTransmitFrequency(0xB6));
        check(writes.size() == 5, "real decoder unlocks approved polling after directed MK2 reply");
        const auto before = writes.size();
        session.sendCiv(cmdSetPtt(0xB6, false));
        session.sendCiv(cmdScopeDataOutput(0xB6, true));
        check(!session.reopenCivPipe(), "audit cannot automatically restart CI-V");
        IcomCivBackendTestAccess::txPump(session);
        check(IcomCivBackendTestAccess::noTxTimer(session) && writes.size() == before,
              "audit never creates a TX timer or transmits audio, even with enableTx forced true");
        session.stop();
        bool ownOnly = true;
        for (const auto& packet : writes) {
            const auto h = parseHeader(packet);
            ownOnly &= h.sentId == localSid && h.rcvdId == remoteSid;
        }
        check(ownOnly && writes.size() == before + 8, "teardown sends own deauth and six own disconnects only");
    }
    {
        Packets writes;
        IcomSession session(nullptr, IcomOutboundPolicy::rxAudit());
        IcomCivBackendTestAccess::session(session, writes);
        IcomCivBackendTestAccess::busy(session);
        check(IcomCivBackendTestAccess::failing(session) && writes.empty(),
              "busy notification cannot become an audit stream grant");
    }
    {
        Packets writes;
        IcomCivBackend backend(nullptr, IcomOutboundPolicy::rxAudit());
        IcomCivBackendTestAccess::backend(backend, writes);
        IcomCivBackendTestAccess::startScope(backend);
        IcomCivBackendTestAccess::scopeTraffic(backend);
        IcomCivBackendTestAccess::forbidden(backend);
        check(writes.empty(), "scope startup and emergency/user writes suppressed before scheduling");
        IcomCivBackendTestAccess::tick(backend);
        bool queriesOnly = true;
        for (const auto& packet : writes) {
            queriesOnly &= isSerialData(packet)
                && IcomOutboundPolicy::rxAudit().allowsCiv(serialPayload(packet));
        }
        check(!writes.empty() && queriesOnly, "audit backend uses real scheduler for approved polling only");
        backend.disconnectRadio();
        bool noCleanupCiv = true;
        for (const auto& packet : writes) {
            if (isSerialData(packet)) {
                noCleanupCiv &= IcomOutboundPolicy::rxAudit().allowsCiv(serialPayload(packet));
            }
        }
        check(noCleanupCiv, "backend disconnect cannot issue CW abort or PTT OFF");
    }
    {
        Packets writes;
        IcomCivBackend backend(nullptr, IcomOutboundPolicy::rxAudit());
        IcomCivBackendTestAccess::backend(backend, writes);
        IcomCivBackendTestAccess::wireDisconnect(backend);
        QPointer<IcomSession> session(IcomCivBackendTestAccess::current(backend));
        bool retired = false;
        QObject::connect(&backend, &AetherSDR::IRadioBackend::disconnected, &backend, [&] {
            backend.disconnectRadio(); retired = true;
        }, Qt::QueuedConnection);
        IcomCivBackendTestAccess::busy(*session);
        check(session && !retired, "failure callback does not synchronously destroy its session");
        QCoreApplication::processEvents();
        check(!session && retired, "queued failure cleanup safely retires only the test session");
    }
    {
        Packets writes;
        auto backend = std::make_unique<IcomCivBackend>(nullptr, IcomOutboundPolicy::rxAudit());
        IcomCivBackendTestAccess::backend(*backend, writes);
        QObject::connect(backend.get(), &AetherSDR::IRadioBackend::transmitChanged, &app,
                         [&](const AetherSDR::TransmitDelta&) { backend.reset(); });
        IcomCivBackendTestAccess::burst(*backend);
        check(!backend, "readback-reset observer may retire backend without a subsequent dispatch");
    }
    {
        Packets writes;
        IcomStream denied(nullptr, IcomOutboundPolicy{});
        IcomStreamTestAccess::prepare(denied, IcomStream::Role::Serial, writes);
        denied.sendRaw(buildIdle(localSid, remoteSid, 0));
        denied.sendTracked(buildSerialData(localSid, remoteSid, 0, 0, cmdReadSplit(0xB6)));
        check(writes.empty(), "missing policy fails closed at final writer");
    }
    std::printf("RX audit egress failures=%d; no sockets bound\n", failures);
    return failures ? 1 : 0;
}
