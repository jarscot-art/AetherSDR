#include "core/backends/icom/IcomCivBackend.h"
#include "core/backends/icom/IcomSession.h"
#include "core/backends/icom/IcomStream.h"
#include "core/ThemeManager.h"
#include "models/TransmitModel.h"
#include "gui/RxReadbackWidget.h"
#include "gui/TxReadbackMarker.h"
#include "TestSettingsProfile.h"
#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QTest>
#include <cstdio>
#include <algorithm>
#include <memory>
#include <vector>

using namespace AetherSDR;
using namespace AetherSDR::icom;
namespace {
int failures = 0;
void check(bool passed, const char* message) {
    std::printf("%s %s\n", passed ? "PASS" : "FAIL", message);
    if (!passed) { ++failures; }
}
using Packets = std::vector<std::vector<std::uint8_t>>;
}
namespace AetherSDR::icom {
struct IcomStreamTestAccess {
    static void prepare(IcomStream& stream, Packets& packets) {
        stream.m_config.role = IcomStream::Role::Serial;
        stream.m_localSid = 1; stream.m_remoteSid = 2;
        stream.m_ready = stream.m_gotRemoteSid = stream.m_auditIdentified = true;
        stream.m_activityClock.start();
        stream.m_testWriter = [&packets](auto bytes) { packets.emplace_back(bytes.begin(), bytes.end()); };
    }
};
struct IcomCivBackendTestAccess {
    static void prepare(IcomCivBackend& b, Packets& packets) {
        b.m_session = std::make_unique<IcomSession>(nullptr, IcomOutboundPolicy::rxAudit());
        b.m_session->m_params.civAddress = 0xB6;
        b.m_session->m_serial = b.m_session->makeStream();
        IcomStreamTestAccess::prepare(*b.m_session->m_serial, packets);
        b.m_connected = true; b.m_sessionGeneration = 1;
        b.m_model = modelForId(0xB6); b.m_civReported = b.m_civModelId = 0xB6;
    }
    static void deliver(IcomCivBackend& b, std::initializer_list<std::uint8_t> body,
                        std::uint64_t generation = 1) {
        std::vector<std::uint8_t> raw{0xFE,0xFE,0xE0,0xB6};
        raw.insert(raw.end(), body); raw.push_back(0xFD);
        const auto frame = parseFrame(raw);
        check(frame.has_value(), "regression input frame parses");
        if (frame) { b.onCivFrame(*frame, generation); }
    }
    static void nextSlot(IcomCivBackend& b) {
        b.pumpCiv(std::max<qint64>(b.nowMs(), b.m_civScheduler.stats().lastDispatchMs)
                  + IcomCivScheduler::kSlotMs);
    }
    static void queueUnselected(IcomCivBackend& b) {
        b.queueUnselectedVfoReadback(IcomCivScheduler::Priority::Control);
    }
    static void timeoutRx(IcomCivBackend& b) {
        (void)b.m_civScheduler.reset();
        const auto read = cmdReadFrequency(0xB6);
        b.queueRead(read, "frequency", IcomCivScheduler::Priority::Control);
        const auto now = b.nowMs() + 1000;
        b.pumpCiv(now); b.pumpCiv(now + 1000);
    }
    static void expire(IcomCivBackend& b) {
        b.m_receiveDialReadback.atMs = b.nowMs() - 10'000;
        b.m_splitReadback.atMs = b.nowMs() - 10'000;
        b.serviceSplitTxReadback(b.nowMs());
    }
};
}
using Access = IcomCivBackendTestAccess;
int main(int argc, char** argv)
{
    TestSettingsProfile settings(QStringLiteral("rx-readback-gui"));
    if (!settings.isValid()) { return 1; }
    QApplication app(argc, argv);
    TransmitModel model;
    RxReadbackWidget widget;
    int intents = 0;
    QObject::connect(&model, &TransmitModel::moxChanged, &app, [&](bool) { ++intents; });
    QObject::connect(&model, &TransmitModel::tuneCommandIssued, &app, [&](bool) { ++intents; });
    widget.refresh(model, true);
    check(widget.text().contains("unconfirmed / expired") && !model.confirmedSplit(),
          "no unconfirmed SPLIT OFF or TX frequency is invented");
    TransmitDelta legacy; legacy.legacyTransmitFreq = 14.1; model.applyChanges(legacy);
    TransmitDelta confirmed;
    confirmed.split = false; confirmed.splitValid = true; confirmed.splitDeadline = QDeadlineTimer(5000);
    confirmed.receiveDialFreq = 7.074; confirmed.receiveDialFreqValid = true;
    confirmed.receiveDialFreqDeadline = QDeadlineTimer(5000);
    confirmed.transmitFreq = 7.075; confirmed.transmitFreqValid = true;
    confirmed.transmitFreqDeadline = QDeadlineTimer(5000);
    model.applyChanges(confirmed); widget.refresh(model, true);
    check(model.confirmedSplit() == std::optional<bool>(false)
          && widget.text().contains("SPLIT OFF") && widget.text().contains("TX 7.075000 MHz")
          && widget.text().contains("+1000 Hz"), "confirmed OFF/TX and positive TX-minus-RX-dial rendered");
    check(model.transmitFreq() == 14.1, "readback leaves legacy ATU/KAT500 reference unchanged");
    confirmed.split = true; confirmed.transmitFreq = 7.073;
    model.applyChanges(confirmed); widget.refresh(model, true);
    check(widget.text().contains("SPLIT ON") && widget.text().contains("-1000 Hz")
          && !widget.text().contains("VFO B"), "negative delta and SPLIT ON; TX is never labelled VFO B");
    TxReadbackMarker marker{model.confirmedTransmitFreq(), model.transmitFreqDeadline()};
    QImage canvas(200,100,QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent); QPainter painter(&canvas);
    const QColor color = ThemeManager::instance().color(QStringLiteral("color.slice.tx"));
    check(marker.paint(painter, QRect(0,0,200,100), 100, color), "confirmed marker uses existing scale position");
    check(!marker.paint(painter, QRect(0,0,200,100), -1, color)
          && !marker.paint(painter, QRect(0,0,200,100), 200, color), "out-of-range TX is not pinned to a false edge");
    painter.end();
    bool pixels = false;
    for (int y=0;y<canvas.height();++y) { pixels |= qAlpha(canvas.pixel(100,y)) != 0; }
    check(pixels, "marker painter produces actual pixels at the confirmed position");
    confirmed.transmitFreqDeadline = QDeadlineTimer(1);
    confirmed.splitDeadline = QDeadlineTimer(1); confirmed.receiveDialFreqDeadline = QDeadlineTimer(1);
    model.applyChanges(confirmed);
    marker = {model.confirmedTransmitFreq(), model.transmitFreqDeadline()};
    QTest::qWait(10); widget.refresh(model, true);
    check(!model.confirmedSplit() && !model.confirmedReceiveDialFreq() && !model.confirmedTransmitFreq()
          && !marker.confirmed() && !widget.text().contains("7.073000")
          && !widget.text().contains("-1000 Hz"), "deadlines expire without any further radio response");
    model.applyChanges(confirmed); // expired queued delivery must not restart its lifetime
    check(!model.confirmedSplit() && !model.confirmedReceiveDialFreq(), "late queued samples remain expired");
    TransmitDelta missing;
    missing.splitValid = true; missing.receiveDialFreqValid = true;
    model.applyChanges(missing);
    check(!model.confirmedSplit() && !model.confirmedReceiveDialFreq(), "validity without value/deadline fails closed");
    model.resetTransmitFrequencyObservation();
    check(!model.hasFrequencyReadback(), "backend reset withdraws capability and all observations");
    widget.refresh(model, false);
    check(widget.text().contains("disconnected") && !widget.text().contains("SPLIT OFF"), "disconnect removes stale values");
    {
        TransmitDelta update;
        UnselectedVfoObservation observation;
        observation.supported = true; observation.frequencyMHz = 14.195;
        observation.frequencyDeadline = QDeadlineTimer(5000);
        observation.mode = UnselectedVfoMode{QStringLiteral("RTTY-R"), true, 3};
        observation.modeDeadline = QDeadlineTimer(5000);
        const auto refresh = [&] {
            update.unselectedVfo = observation; model.applyChanges(update); widget.refresh(model, true);
        };
        refresh();
        check(widget.unselectedVfoText().contains("VFO NIEWYBRANE")
              && !widget.unselectedVfoText().contains("VFO B")
              && widget.unselectedVfoText().contains("14.195000 MHz")
              && widget.unselectedVfoText().contains("RTTY-R")
              && widget.unselectedVfoText().contains("DATA ON")
              && widget.unselectedVfoText().contains("FIL3"), "separate relative VFO frequency, emission, DATA and filter rendered");
        const auto unchangedSplitTx = widget.text();
        for (int filter : {1, 2, 3}) {
            observation.mode->filter = filter; observation.mode->data = false; refresh();
            check(widget.unselectedVfoText().contains(QStringLiteral("FIL%1").arg(filter))
                  && widget.unselectedVfoText().contains("DATA OFF") && widget.text() == unchangedSplitTx,
                  "VFO-only updates repaint even when SPLIT/TX line is unchanged");
        }
        auto* label = widget.findChild<QLabel*>(QStringLiteral("UnselectedVfoReadback"));
        check(label && label->isEnabled() && label->accessibleName() == "VFO NIEWYBRANE"
              && !label->toolTip().isEmpty(), "readback has accessible label and explanation");
        if (label) {
            auto& theme = ThemeManager::instance();
            check(theme.tokensForWidget(label).contains("color.text.secondary")
                  && theme.tokensForWidget(label).contains("color.control.unavailable")
                  && !label->styleSheet().contains("{{"), "both readable and unavailable colors use resolved theme tokens");
            const auto saved = theme.color(QStringLiteral("color.text.secondary"));
            const auto replacement = theme.color(QStringLiteral("color.text.primary"));
            theme.setColor(QStringLiteral("color.text.secondary"), replacement);
            QCoreApplication::processEvents();
            check(label->styleSheet().contains(replacement.name()), "readback follows live theme changes");
            theme.setColor(QStringLiteral("color.text.secondary"), saved);
        }
        observation.frequencyDeadline = QDeadlineTimer(0); refresh();
        check(!widget.unselectedVfoText().contains("14.195000") && widget.unselectedVfoText().contains("FIL3"),
              "expired frequency is omitted while mode remains current");
        observation.frequencyDeadline = QDeadlineTimer(5000);
        observation.modeDeadline = QDeadlineTimer(0); refresh();
        check(widget.unselectedVfoText().contains("14.195000") && !widget.unselectedVfoText().contains("RTTY-R")
              && !widget.unselectedVfoText().contains("DATA OFF") && !widget.unselectedVfoText().contains("FIL3"),
              "mode expiry withdraws emission, DATA and filter together");
        observation.frequencyDeadline = QDeadlineTimer(1); refresh();
        QTest::qWait(10); widget.refresh(model, true);
        check(!widget.unselectedVfoText().contains("14.195000") && label && !label->isEnabled(),
              "time alone dims stale row without requiring another model signal");
        observation.frequencyDeadline = QDeadlineTimer(5000);
        observation.modeDeadline = QDeadlineTimer(5000); refresh();
        widget.refresh(model, false);
        check(!widget.unselectedVfoText().contains("14.195000") && !widget.unselectedVfoText().contains("FIL3")
              && widget.unselectedVfoText().contains("disconnected"), "disconnect hides even not-yet-expired samples");
        model.resetTransmitFrequencyObservation(); widget.refresh(model, true);
        check(widget.unselectedVfoText().contains("unavailable / unconfirmed")
              && !widget.unselectedVfoText().contains("DATA OFF"), "new session does not inherit old observations");
    }
    check(intents == 0, "presentation never emits PTT or tune intents");
    {
        Packets packets;
        IcomCivBackend backend(nullptr, IcomOutboundPolicy::rxAudit());
        Access::prepare(backend, packets);
        QObject::connect(&backend, &IRadioBackend::transmitChanged, &model, &TransmitModel::applyChanges);
        Access::deliver(backend,{0x03,0,0x40,0x07,0x07,0});
        Access::deliver(backend,{0x0F,0});
        // The RX report queued a TX read; it went out before SPLIT was known.
        // Its first reply belongs to that superseded context and MUST fail.
        Access::deliver(backend,{0x1C,3,0,0x50,0x07,0x07,0});
        check(!model.confirmedTransmitFreq(), "pre-SPLIT in-flight TX answer cannot populate the GUI");
        Access::nextSlot(backend);
        Access::deliver(backend,{0x1C,3,0,0x50,0x07,0x07,0});
        widget.refresh(model,true);
        std::printf("EVIDENCE decoded GUI: %s\n", widget.text().toUtf8().constData());
        check(model.confirmedReceiveDialFreq() == std::optional<double>(7.074)
              && model.confirmedTransmitFreq() == std::optional<double>(7.075)
              && widget.text().contains("+1000 Hz"), "real CI-V decoder feeds model and GUI without diagnostic-map parsing");
        Access::queueUnselected(backend); Access::nextSlot(backend);
        Access::deliver(backend,{0x25,1,0,0x50,0x19,0x14,0}); Access::nextSlot(backend);
        Access::deliver(backend,{0x26,1,1,1,2}); QCoreApplication::processEvents();
        const auto packetsBeforeRefresh = packets.size();
        for (int i = 0; i < 10; ++i) { widget.refresh(model, true); }
        check(widget.unselectedVfoText().contains("14.195000 MHz")
              && widget.unselectedVfoText().contains("USB") && widget.unselectedVfoText().contains("DATA ON")
              && widget.unselectedVfoText().contains("FIL2") && model.confirmedTransmitFreq() == 7.075
              && model.confirmedReceiveDialFreq() == 7.074 && packets.size() == packetsBeforeRefresh,
              "real decoder -> typed model -> GUI; no RX/TX overwrite or GUI-generated packet");
        std::printf("EVIDENCE unselected GUI: %s\n", widget.unselectedVfoText().toUtf8().constData());
        Access::deliver(backend,{0x0F,0x10}); // DUP is not SPLIT
        Access::deliver(backend,{0x03,0xFA,0x40,0x07,0x07,0});
        check(model.confirmedSplit() == std::optional<bool>(false)
              && model.confirmedReceiveDialFreq() == std::optional<double>(7.074), "DUP and malformed RX data cannot fabricate state");
        Access::deliver(backend,{0x03,0,0x60,0x07,0x07,0},0);
        check(model.confirmedReceiveDialFreq() == std::optional<double>(7.074), "old session generation cannot replace RX");
        Access::deliver(backend,{0x03,0,0x60,0x07,0x07,0});
        widget.refresh(model,true);
        check(!model.confirmedTransmitFreq() && !widget.text().contains("+1000 Hz"), "RX context change withdraws old TX and delta");
        QCoreApplication::processEvents(); widget.refresh(model, true);
        check(!widget.unselectedVfoText().contains("14.195000") && !widget.unselectedVfoText().contains("FIL2"),
              "selected RX context change withdraws unselected VFO GUI evidence");
        Access::timeoutRx(backend);
        check(!model.confirmedReceiveDialFreq(), "RX read timeout invalidates independently");
        Access::deliver(backend,{0x03,0,0x60,0x07,0x07,0});
        Access::expire(backend);
        check(!model.confirmedReceiveDialFreq() && !model.confirmedSplit(), "scheduler expiry reaches readback model");
        backend.disconnectRadio(); widget.refresh(model,false);
        check(!model.confirmedSplit() && !model.confirmedReceiveDialFreq() && !model.confirmedTransmitFreq(),
              "backend disconnect invalidates all readbacks");
        check(backend.rxAuditDiagnostics().value("deniedRequests").toULongLong() == 0,
              "readback presentation generates no forbidden requests");
        for (const auto& packet : packets) {
            check(!isSerialData(packet) || IcomOutboundPolicy::rxAudit().allowsCiv(serialPayload(packet)),
                  "actual terminal UDP writer contains only existing approved CI-V queries");
        }
    }
    std::printf("RX readback GUI failures=%d; no sockets bound\n", failures);
    return failures ? 1 : 0;
}
