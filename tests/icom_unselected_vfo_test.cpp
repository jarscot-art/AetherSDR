// Socket-free backend readback: real scheduler, codec and final UDP writer guard.
#include "core/backends/icom/IcomCivBackend.h"
#include "TestSettingsProfile.h"
#include <QCoreApplication>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

using namespace AetherSDR;
using namespace AetherSDR::icom;
using Packets = std::vector<std::vector<std::uint8_t>>;
namespace {
int failures = 0;
void check(bool ok, const char* text) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", text);
    if (!ok) { ++failures; }
}
}
namespace AetherSDR::icom {
struct IcomStreamTestAccess {
    static void prepare(IcomStream& s, Packets& writes) {
        s.m_config.role = IcomStream::Role::Serial;
        s.m_localSid = 0x12345678; s.m_remoteSid = 0x87654321;
        s.m_gotRemoteSid = s.m_ready = s.m_auditIdentified = true;
        s.m_activityClock.start();
        s.m_testWriter = [&writes](auto bytes) { writes.emplace_back(bytes.begin(), bytes.end()); };
    }
};
struct IcomCivBackendTestAccess {
    static void prepare(IcomCivBackend& b, Packets& writes, std::uint64_t generation = 7) {
        b.m_session = std::make_unique<IcomSession>(nullptr, b.m_outboundPolicy);
        b.m_session->m_params.civAddress = 0xB6;
        b.m_session->m_serial = b.m_session->makeStream();
        IcomStreamTestAccess::prepare(*b.m_session->m_serial, writes);
        b.m_model = modelForId(0xB6); b.m_connected = true;
        b.m_civReported = b.m_civModelId = 0xB6; b.m_civAmbiguous = false;
        b.m_sessionGeneration = generation;
        b.m_frequencyHz = 7'074'000; b.m_mode = CivMode::Usb;
        b.m_dataMode = false; b.m_filter = 1;
    }
    static auto rx(const IcomCivBackend& b) {
        return std::tuple{b.m_frequencyHz, b.m_mode, b.m_dataMode, b.m_filter,
                          b.m_scopeCentreHz, b.m_scopeSpanHz, b.m_scopeLowerHz, b.m_scopeUpperHz};
    }
    static void pump(IcomCivBackend& b) {
        b.pumpCiv(std::max<qint64>(b.nowMs(), b.m_civScheduler.stats().lastDispatchMs + 50));
    }
    static void queue(IcomCivBackend& b) {
        b.queueUnselectedVfoReadback(IcomCivScheduler::Priority::Control);
    }
    static void poll(IcomCivBackend& b) { queue(b); pump(b); }
    static void receive(IcomCivBackend& b, std::initializer_list<std::uint8_t> body,
                        std::uint64_t generation = 7, std::uint8_t from = 0xB6,
                        std::uint8_t to = 0xE0, std::uint8_t terminator = 0xFD) {
        std::vector<std::uint8_t> wire{0xFE,0xFE,to,from};
        wire.insert(wire.end(), body); wire.push_back(terminator);
        const auto frame = parseFrame(wire);
        if (frame) { b.onCivFrame(*frame, generation); }
    }
    static void confirm(IcomCivBackend& b, std::uint64_t generation = 7) {
        poll(b);
        receive(b, {0x25,1,0,0x50,0x19,0x14,0}, generation);
        pump(b);
        receive(b, {0x26,1,0,1,3}, generation); // LSB-D, FIL3; deliberately different from RX
    }
    static auto key(IcomCivBackend& b) { return b.m_civScheduler.stats().inFlightKey; }
    static void timeout(IcomCivBackend& b) {
        b.pumpCiv(std::max<qint64>(b.nowMs(), b.m_civScheduler.stats().lastDispatchMs)
                  + IcomCivScheduler::kReadTimeoutMs + 1);
    }
    static void ageFrequency(IcomCivBackend& b) { b.m_unselectedFrequency.atMs -= 5001; }
    static void ageMode(IcomCivBackend& b) { b.m_unselectedMode.atMs -= 5001; }
    static void service(IcomCivBackend& b) { b.serviceUnselectedVfo(b.nowMs()); }
    static void end(IcomCivBackend& b) { b.onSessionDisconnected(QString()); }
    static void context(IcomCivBackend& b) { b.invalidateTxReadback(QStringLiteral("previous-context")); }
    static void noIdentity(IcomCivBackend& b) { b.m_civReported = 0; }
    static void otherModel(IcomCivBackend& b) { b.m_model = modelForId(0xA4); }
    static std::size_t pendingVfo(const IcomCivBackend& b) {
        std::size_t count = b.m_civScheduler.stats().inFlightKey.starts_with("vfo.unselected.") ? 1 : 0;
        for (const auto& queued : b.m_civScheduler.m_queue) {
            if (queued.request.key.starts_with("vfo.unselected.")) { ++count; }
        }
        return count;
    }
    static auto tx(const IcomCivBackend& b) { return b.splitTxReadbackDiagnostics(b.nowMs(), true); }
    static auto keys(const IcomCivBackend& b) {
        return std::tuple{b.semanticKey(cmdReadFrequency(0xB6)), b.semanticKey(cmdReadVfoMode(0xB6)),
                         b.semanticKey(cmdReadUnselectedVfoFrequency(0xB6)),
                         b.semanticKey(cmdReadUnselectedVfoMode(0xB6))};
    }
};
}
using Access = IcomCivBackendTestAccess;
struct Fixture {
    Packets writes;
    IcomCivBackend backend{nullptr, IcomOutboundPolicy::rxAudit()};
    Fixture() { Access::prepare(backend, writes); }
};
int main(int argc, char** argv) {
    TestSettingsProfile profile(QStringLiteral("icom-unselected-vfo"));
    if (!profile.isValid()) { return 1; }
    QCoreApplication app(argc, argv);
    {
        Fixture f; auto& b = f.backend;
        const auto before = Access::rx(b);
        int slices = 0, tx = 0;
        QObject::connect(&b, &IRadioBackend::sliceChanged, &b, [&](int, const SliceDelta&) { ++slices; });
        QObject::connect(&b, &IRadioBackend::transmitChanged, &b, [&](const TransmitDelta&) { ++tx; });
        check(!b.unselectedVfoReadback().frequencyHz && !b.unselectedVfoReadback().mode, "initial values unknown");
        Access::receive(b, {0x25,1,0,0x50,0x19,0x14,0});
        check(!b.unselectedVfoReadback().frequencyHz, "unsolicited reply cannot create confirmed state");
        Access::confirm(b);
        const auto state = b.unselectedVfoReadback();
        check(state.frequencyHz == 14'195'000 && state.mode && state.mode->mode == CivMode::Lsb
              && state.mode->dataMode && state.mode->filter == 3, "independent frequency/emission/DATA/filter confirmed");
        check(state.frequencyAtMs >= 0 && state.modeAtMs >= 0 && state.sessionGeneration == 7,
              "samples have individual monotonic timestamps and session provenance");
        check(Access::rx(b) == before && slices == 0 && tx == 0,
              "unselected replies do not publish or overwrite RX, TX, filter or scope");
        check(Access::keys(b) == std::tuple{std::string("frequency"), std::string("mode"),
              std::string("vfo.unselected.frequency"), std::string("vfo.unselected.mode")},
              "selected and unselected scheduler keys are independent");
        std::set<std::vector<std::uint8_t>> emitted;
        for (const auto& packet : f.writes) {
            check(isSerialData(packet), "only own serial query datagrams at final writer");
            const auto payload = serialPayload(packet);
            emitted.emplace(payload.begin(), payload.end());
            check(IcomOutboundPolicy::rxAudit().allowsCiv(payload), "final writer bytes satisfy unchanged RX-only policy");
        }
        check(emitted == std::set{cmdReadUnselectedVfoFrequency(0xB6), cmdReadUnselectedVfoMode(0xB6)},
              "only 25 01 and 26 01 emitted; no select, tune, PTT, XFC or scope writes");
        check(b.rxAuditDiagnostics().value("deniedRequests").toULongLong() == 0
              && b.rxAuditDiagnostics().value("deniedDatagrams").toULongLong() == 0,
              "valid read cycle produces no denied requests or datagrams");
        Access::receive(b, {0x1C,3,0,0x40,7,7,0});
        check(Access::tx(b).value("txFrequencyHz").toMap().value("value").toULongLong() == 7'074'000
              && b.unselectedVfoReadback().frequencyHz == 14'195'000,
              "TX 1C 03 remains independent of unselected VFO");
        Access::ageFrequency(b);
        check(!b.unselectedVfoReadback().frequencyHz && b.unselectedVfoReadback().mode.has_value(),
              "frequency expires at getter even without timer service; mode remains valid");
        Access::ageMode(b); Access::service(b);
        check(!b.unselectedVfoReadback().mode && b.unselectedVfoReadback().modeStatus == "stale",
              "mode expiry cannot preserve a filter or DATA claim");
    }
    {
        Fixture f; auto& b = f.backend; Access::poll(b);
        for (const auto& args : {std::tuple{7ULL,0xA4,0xE0,0xFD}, std::tuple{6ULL,0xB6,0xE0,0xFD},
                                std::tuple{7ULL,0xB6,0x00,0xFD}, std::tuple{7ULL,0xB6,0xE0,0xFC}}) {
            Access::receive(b, {0x25,1,0,0x50,0x19,0x14,0}, std::get<0>(args),
                            std::get<1>(args), std::get<2>(args), std::get<3>(args));
        }
        Access::receive(b, {0x25,0,0,0x50,0x19,0x14,0});
        Access::receive(b, {0x25,2,0,0x50,0x19,0x14,0});
        Access::receive(b, {0x25,1,0,0xFA,0x19,0x14,0});
        Access::receive(b, {0x25,1,0});
        check(!b.unselectedVfoReadback().frequencyHz && Access::key(b) == "vfo.unselected.frequency",
              "foreign, stale-session, broadcast, FC, selected, invalid-selector, BCD and short replies cannot retire read");
        Access::receive(b, {0x25,1,0,0x50,0x19,0x14,0}); Access::pump(b);
        Access::receive(b, {0x26,0,1,0,1}); // same selected context, no switch
        Access::receive(b, {0x26,1,6,0,1});
        Access::receive(b, {0x26,1,1,2,1});
        Access::receive(b, {0x26,1,1,0,0});
        Access::receive(b, {0x26,1,1,0});
        check(!b.unselectedVfoReadback().mode && Access::key(b) == "vfo.unselected.mode",
              "26 00 and malformed 26 01 never confirm or complete the unselected mode read");
        Access::receive(b, {0x26,1,1,1,2});
        check(b.unselectedVfoReadback().mode && b.unselectedVfoReadback().mode->filter == 2,
              "complete strict mode reply restores only the unselected sample");
    }
    {
        Fixture f; auto& b = f.backend; Access::confirm(b); Access::poll(b);
        Access::timeout(b);
        check(!b.unselectedVfoReadback().frequencyHz && b.unselectedVfoReadback().frequencyStatus == "timeout",
              "missing refresh immediately invalidates old frequency on timeout");
        Access::receive(b, {0x25,1,0,0x50,0x19,0x14,0});
        check(!b.unselectedVfoReadback().frequencyHz, "late frequency reply cannot undo timeout");
        Access::receive(b, {0xFA}, 7, 0xA4);
        check(Access::key(b) == "vfo.unselected.mode", "foreign NG cannot retire mode request");
        Access::receive(b, {0xFA});
        check(!b.unselectedVfoReadback().mode && b.unselectedVfoReadback().modeStatus == "ng",
              "NG invalidates previously confirmed mode");
        Access::poll(b); Access::receive(b, {0xFB});
        check(!b.unselectedVfoReadback().frequencyHz && b.unselectedVfoReadback().frequencyStatus == "ack-without-data",
              "ACK alone is not a frequency observation");
        Access::pump(b); Access::timeout(b);
        check(!b.unselectedVfoReadback().mode && b.unselectedVfoReadback().modeStatus == "timeout",
              "missing mode response has its own timeout and never keeps DATA/filter valid");
        Access::confirm(b);
        check(b.unselectedVfoReadback().frequencyHz == 14'195'000 && b.unselectedVfoReadback().mode.has_value(),
              "a new correlated read cycle restores both samples after failure");
    }
    for (int event = 0; event < 6; ++event) {
        Fixture f; auto& b = f.backend; Access::confirm(b);
        const auto context = b.unselectedVfoReadback().contextGeneration;
        if (event == 0) { Access::receive(b, {0x03,0,0x50,7,7,0}); }
        if (event == 1) { Access::receive(b, {0x26,0,1,1,1}); }
        if (event == 2) { Access::receive(b, {0x26,0,1,0,2}); }
        if (event == 3) { Access::receive(b, {0x0F,1}); }
        if (event == 4) { Access::receive(b, {0x0F,0}); }
        if (event == 5) { Access::context(b); }
        check(!b.unselectedVfoReadback().frequencyHz && !b.unselectedVfoReadback().mode
              && b.unselectedVfoReadback().contextGeneration > context,
              "RX frequency, DATA, filter, SPLIT or existing context invalidation withdraws both samples");
    }
    {
        Fixture f; auto& b = f.backend; Access::poll(b); Access::context(b);
        Access::receive(b, {0x25,1,0,0x50,0x19,0x14,0});
        check(!b.unselectedVfoReadback().frequencyHz, "in-flight answer from previous context cannot confirm");
        Access::end(b);
        check(!b.unselectedVfoReadback().frequencyHz && !b.unselectedVfoReadback().mode
              && b.unselectedVfoReadback().frequencyAtMs == -1, "disconnect clears values and timestamps");
        Access::prepare(b, f.writes, 8); Access::poll(b);
        Access::receive(b, {0x25,1,0,0x50,0x19,0x14,0}, 7);
        check(!b.unselectedVfoReadback().frequencyHz, "reconnect rejects previous session even with a live new request");
        Access::receive(b, {0x25,1,0,0x50,0x19,0x14,0}, 8); Access::pump(b);
        Access::receive(b, {0x26,1,1,0,1}, 8);
        check(b.unselectedVfoReadback().frequencyHz == 14'195'000 && b.unselectedVfoReadback().mode.has_value(),
              "reconnect requires and accepts fresh correlated replies");
    }
    {
        Fixture f; auto& b = f.backend;
        Access::poll(b);
        for (int i = 0; i < 20; ++i) { Access::queue(b); }
        check(Access::pendingVfo(b) == 2 && f.writes.size() == 1,
              "repeated polling coalesces to one outstanding frequency and one queued mode request");
    }
    for (bool unsupported : {false, true}) {
        Fixture f; auto& b = f.backend;
        if (unsupported) { Access::otherModel(b); } else { Access::noIdentity(b); }
        Access::poll(b); Access::receive(b, {0x25,1,0,0x50,0x19,0x14,0});
        check(f.writes.empty() && !b.unselectedVfoReadback().frequencyHz,
              "no new requests or state for unsupported models or missing identity");
    }
    return failures ? 1 : 0;
}
