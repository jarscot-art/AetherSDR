// Real SpectrumWidget events, injected final CI-V writer; no socket or radio.
#include "TestSettingsProfile.h"
#include "gui/SpectrumWidget.h"
#include "core/backends/icom/IcomCivBackend.h"
#include <QApplication>
#include <QImage>
#include <QFocusEvent>
#include <QMetaMethod>
#include <QThread>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

using namespace AetherSDR;
using namespace AetherSDR::icom;
using Packets = std::vector<std::vector<std::uint8_t>>;
namespace AetherSDR::icom {
struct IcomStreamTestAccess {
    static void prepare(IcomStream& s, Packets& packets) {
        s.m_config.role = IcomStream::Role::Serial;
        s.m_localSid = 0x12345678; s.m_remoteSid = 0x87654321;
        s.m_gotRemoteSid = s.m_ready = true;
        s.m_activityClock.start();
        s.m_testWriter = [&packets](auto bytes) { packets.emplace_back(bytes.begin(), bytes.end()); };
    }
};
struct IcomCivBackendTestAccess {
    static void prepare(IcomCivBackend& b, Packets& packets) {
        b.m_session = std::make_unique<IcomSession>(nullptr, b.m_outboundPolicy);
        b.m_session->m_params.civAddress = 0xB6;
        b.m_session->m_serial = b.m_session->makeStream();
        IcomStreamTestAccess::prepare(*b.m_session->m_serial, packets);
        b.m_model = modelForId(0xB6); b.m_connected = true;
        b.m_civReported = b.m_civModelId = 0xB6; b.m_civAmbiguous = false;
        b.m_sessionGeneration = 7;
    }
    static void pump(IcomCivBackend& b) {
        b.pumpCiv(std::max<qint64>(b.nowMs(), b.m_civScheduler.stats().lastDispatchMs + 50));
    }
};
}
namespace AetherSDR {
struct SpectrumOffscreenTestAccess {
    static QPoint point(SpectrumWidget& w, double mhz, bool waterfall) {
        return {w.mhzToX(mhz), waterfall ? w.height() - 60 : 60};
    }
    static double frequency(SpectrumWidget& w, int x) { return w.xToMhz(x); }
    static void mouse(SpectrumWidget& w, QEvent::Type type, QPoint p,
                      Qt::KeyboardModifiers mods = Qt::AltModifier) {
        const bool release = type == QEvent::MouseButtonRelease;
        QMouseEvent e(type, QPointF(p), QPointF(p),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                      release ? Qt::NoButton : Qt::LeftButton, mods);
        QApplication::sendEvent(&w, &e);
    }
    static void clearOverlayDirty(SpectrumWidget& w) {
#ifdef AETHER_GPU_SPECTRUM
        w.m_overlayStaticDirty = false;
#endif
    }
    static bool overlayDirty(SpectrumWidget& w) {
#ifdef AETHER_GPU_SPECTRUM
        return w.m_overlayStaticDirty;
#else
        return false;
#endif
    }
    static QImage paint(SpectrumWidget& w) {
        QImage image(w.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        w.drawConfirmedTxReadback(painter, image.rect());
        return image;
    }
};
}
using Access = SpectrumOffscreenTestAccess;
int main(int argc, char** argv) {
    std::fprintf(stderr, "GUI_MAIN sizeof(SpectrumWidget)=%zu sizeof(DssRenderer)=%zu\n", sizeof(SpectrumWidget), sizeof(DssRenderer));
    TestSettingsProfile profile(QStringLiteral("tx-marker-preview"));
    if (!profile.isValid()) { return 1; }
    QApplication app(argc, argv);
    int failures = 0;
    const auto check = [&](bool ok, const char* message) {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", message);
        if (!ok) { ++failures; }
    };
    Packets packets;
    IcomCivBackend backend;
    IcomCivBackendTestAccess::prepare(backend, packets);
    const auto wOwner = std::make_unique<SpectrumWidget>();
    SpectrumWidget& w = *wOwner;
    w.resize(1000, 500);
    w.setFrequencyRangeImmediate(7.075, 0.040);
    w.setSingleClickTune(true);
    // Real legacy tune route: the positive control below proves this writer is live.
    QObject::connect(&w, &SpectrumWidget::frequencyClicked, &backend,
                     [&](double mhz) { backend.setSliceFrequency(0, mhz * 1e6); });
    QObject::connect(&w, &SpectrumWidget::incrementalTuneRequested, &backend,
                     [&](double mhz) { backend.setSliceFrequency(0, mhz * 1e6); });
    std::vector<std::unique_ptr<QSignalSpy>> spies;
    for (int i = SpectrumWidget::staticMetaObject.methodOffset();
         i < SpectrumWidget::staticMetaObject.methodCount(); ++i) {
        const QMetaMethod method = SpectrumWidget::staticMetaObject.method(i);
        if (method.methodType() == QMetaMethod::Signal) {
            spies.push_back(std::make_unique<QSignalSpy>(&w, method));
            check(spies.back()->isValid(), "widget signal observer installed");
        }
    }
    const auto ready = [&] {
        w.setConfirmedTxReadback(7.080, QDeadlineTimer(5000));
        w.setTxPreviewContext(true, true, QDeadlineTimer(5000));
    };
    const auto press = [&](bool waterfall = true) {
        Access::mouse(w, QEvent::MouseButtonPress, Access::point(w, 7.080, waterfall));
    };
    const auto move = [&] { Access::mouse(w, QEvent::MouseMove, Access::point(w, 7.085, true)); };
    const auto release = [&] { Access::mouse(w, QEvent::MouseButtonRelease, Access::point(w, 7.085, true)); };
    ready();
    const QImage confirmed = Access::paint(w);
    for (bool waterfall : {false, true}) {
        press(waterfall); move(); release();
        check(w.txMarkerPreview().has_value(), "preview remains after release on spectrum/waterfall");
        check(w.txMarkerPreview() && std::abs(*w.txMarkerPreview()
              - Access::frequency(w, Access::point(w, 7.085, true).x())) < 1e-9,
              "preview uses the existing frequency/pixel mapping");
        check(w.confirmedTxReadback() == std::optional<double>{7.080}, "confirmed TX never moves");
        check(w.accessibleDescription().contains(QStringLiteral("PODGLĄD — BEZ ZAPISU")),
              "preview label is explicit and accessible");
        const QImage preview = Access::paint(w);
        check(preview != confirmed, "separate preview renders");
        const int txX = Access::point(w, 7.080, true).x();
        check(preview.copy(txX, 100, 1, 300) == confirmed.copy(txX, 100, 1, 300),
              "red confirmed line remains unchanged in spectrum and waterfall");
    }
    // Re-grab the current yellow position, not the original confirmed TX position.
    for (int repeat = 0; repeat != 16; ++repeat) {
        const auto current = w.txMarkerPreview();
        check(current.has_value(), "yellow selection exists before repeated drag");
        if (!current) { break; }
        const bool waterfall = repeat % 2 == 0;
        const double target = repeat % 2 == 0 ? 7.090 : 7.085;
        Access::mouse(w, QEvent::MouseButtonPress, Access::point(w, *current, waterfall));
        Access::mouse(w, QEvent::MouseMove, Access::point(w, target, waterfall));
        Access::mouse(w, QEvent::MouseButtonRelease, Access::point(w, target, waterfall));
        check(w.txMarkerPreview() && std::abs(*w.txMarkerPreview()
              - Access::frequency(w, Access::point(w, target, waterfall).x())) < 1e-9,
              "repeated yellow drag tracks its current position on spectrum/waterfall");
        check(w.confirmedTxReadback() == std::optional<double>{7.080},
              "yellow drag never moves confirmed red TX");
    }
    for (int separation : {0, 3, -3, 6, -6}) {
        ready(); press();
        const QPoint close = Access::point(w, 7.080, true) + QPoint(separation, 0);
        Access::mouse(w, QEvent::MouseMove, close);
        Access::mouse(w, QEvent::MouseButtonRelease, close);
        Access::mouse(w, QEvent::MouseButtonPress, close);
        move(); release();
        check(w.txMarkerPreview() && std::abs(*w.txMarkerPreview()
              - Access::frequency(w, Access::point(w, 7.085, true).x())) < 1e-9,
              "overlapping red/yellow hits remain exclusively local preview gestures");
    }
    ready(); press(); move(); release();
    check(w.txMarkerPreview().has_value(), "preview prepared for expiry refusal");
    const QPoint expiredYellow = Access::point(w, w.txMarkerPreview().value_or(7.085), true);
    w.setConfirmedTxReadback(7.080, QDeadlineTimer(1));
    QThread::msleep(3);
    Access::mouse(w, QEvent::MouseButtonPress, expiredYellow);
    move(); release();
    check(!w.txMarkerPreview(), "expired yellow hit is refused without falling through to RX");
    for (int invalidation = 0; invalidation != 6; ++invalidation) {
        ready(); press(); move();
        switch (invalidation) {
        case 0: w.setTxPreviewContext(false, true, QDeadlineTimer(5000)); break;
        case 1: w.setTxPreviewContext(true, false, QDeadlineTimer(5000)); break;
        case 2: w.setConfirmedTxReadback(std::nullopt, QDeadlineTimer(0)); break;
        case 3: w.setConfirmedTxReadback(7.080, QDeadlineTimer(0)); break;
        case 4: w.setTxPreviewContext(true, true, QDeadlineTimer(0)); break;
        case 5: w.setConfirmedTxReadback(7.081, QDeadlineTimer(5000)); break;
        }
        check(!w.txMarkerPreview(), "invalidated preview disappears");
        ready(); move(); release();
        check(!w.txMarkerPreview(), "cancelled gesture cannot resume or fall through after recovery");
    }
    for (bool transmitting : {false, true}) {
        ready(); w.setTxPreviewContext(!transmitting, !transmitting, QDeadlineTimer(0));
        press(); move(); release();
        check(!w.txMarkerPreview(), "unavailable Alt gesture never creates preview or tunes RX");
    }
    for (bool splitExpires : {false, true}) {
        ready(); press(); move(); release();
        if (splitExpires) {
            w.setTxPreviewContext(true, true, QDeadlineTimer(1));
        } else {
            w.setConfirmedTxReadback(7.080, QDeadlineTimer(1));
        }
        QThread::msleep(3); // No refresh event: getter and paint must enforce the deadline themselves.
        check(!w.txMarkerPreview(), "clock expiry hides preview before refresh timer runs");
        ready();
        check(!w.txMarkerPreview(), "fresh same-value readback cannot resurrect an expired preview");
    }
    // Focus/key events must preserve both rendered markers, before and after release.
    for (bool dragging : {false, true}) {
        ready(); press(); move();
        if (!dragging) { release(); }
        const auto selection = w.txMarkerPreview();
        const QImage before = Access::paint(w);
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(&w, &escape);
        for (auto type : {QEvent::WindowDeactivate, QEvent::FocusOut,
                          QEvent::UngrabMouse, QEvent::WindowActivate, QEvent::FocusIn}) {
            if (type == QEvent::FocusIn || type == QEvent::FocusOut) {
                QFocusEvent event(type);
                QApplication::sendEvent(&w, &event);
            } else {
                QEvent event(type);
                QApplication::sendEvent(&w, &event);
            }
            check(w.txMarkerPreview() == selection, "focus/Escape preserve selected preview");
            check(Access::paint(w) == before, "focus leaves both marker pixels unchanged");
        }
        Access::clearOverlayDirty(w);
        ready(); // The 100ms model refresh changes deadlines, not visible pixels.
        check(!Access::overlayDirty(w), "unchanged readback does not rebuild the GPU overlay");
        check(w.confirmedTxReadback() == std::optional<double>{7.080}, "focus never invalidates confirmed TX");
        if (dragging) {
            Access::mouse(w, QEvent::MouseMove, Access::point(w, 7.090, true));
            release();
        }
        check(w.txMarkerPreview() == selection, "interrupted drag tail cannot move selection or tune RX");
        press();
        Access::mouse(w, QEvent::MouseMove, Access::point(w, 7.090, true));
        release();
        check(w.txMarkerPreview() != selection, "new Alt drag updates persistent selection");
    }
    ready(); press(); move();
    const auto releasedOutside = w.txMarkerPreview();
    QMouseEvent noButton(QEvent::MouseMove, QPointF(400,400), QPointF(400,400),
                         Qt::NoButton, Qt::NoButton, Qt::AltModifier);
    QApplication::sendEvent(&w, &noButton);
    release();
    check(w.txMarkerPreview() == releasedOutside, "release outside preserves selection and consumes tail");
    ready(); press(); move(); release();
    w.setTxPreviewContext(false, true, QDeadlineTimer(5000));
    check(!w.txMarkerPreview(), "SPLIT OFF clears retained selection");
    ready();
    check(!w.txMarkerPreview(), "SPLIT ON cannot restore a cleared selection");
    Access::clearOverlayDirty(w);
    w.setConfirmedTxReadback(7.080, QDeadlineTimer(0));
#ifdef AETHER_GPU_SPECTRUM
    check(Access::overlayDirty(w), "expired TX invalidates the cached red marker");
#endif
    check(!w.confirmedTxReadback(), "red marker never retains stale TX as confirmed");
    ready(); press(); move();
    QWheelEvent wheel(QPointF(400,400), QPointF(400,400), QPoint(), QPoint(0,120),
                      Qt::LeftButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&w, &wheel);
    release();
    ready();
    Access::mouse(w, QEvent::MouseButtonDblClick, Access::point(w, 7.080, true));
    release();
    for (const auto& spy : spies) { check(spy->isEmpty(), "preview emits no widget control signal"); }
    IcomCivBackendTestAccess::pump(backend);
    check(packets.empty(), "ZERO outbound CI-V datagrams after all preview/cancel/expiry gestures");
    // No modifiers: preserve normal RX click, and prove the injected egress observes writes.
    const QPoint rx = Access::point(w, 7.070, false);
    Access::mouse(w, QEvent::MouseButtonPress, rx, Qt::NoModifier);
    Access::mouse(w, QEvent::MouseButtonRelease, rx, Qt::NoModifier);
    IcomCivBackendTestAccess::pump(backend);
    check(!packets.empty(), "ordinary RX click still reaches injected CI-V writer (positive control)");
    check(w.confirmedTxReadback() == std::optional<double>{7.080},
          "RX tuning does not optimistically overwrite independent confirmed TX");
    return failures ? 1 : 0;
}
