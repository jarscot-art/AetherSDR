#include "IcomRxReadbackGui.h"
#include "core/backends/icom/IcomRxAuditRunner.h"
#include "core/backends/IRadioBackend.h"
#include "models/TransmitModel.h"
#include "gui/RxReadbackWidget.h"
#include "gui/SpectrumWidget.h"
#include <QApplication>
#include <QLabel>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>
#include <cstdio>

namespace AetherSDR {
int runIcomRxReadbackGui(int argc, char** argv)
{
    // Before any widget/ThemeManager/AppSettings construction: never read the
    // production profile, bridge settings, credentials or autoconnect options.
    QTemporaryDir settings;
    if (!settings.isValid()) { return 2; }
    qputenv("AETHER_SETTINGS_DIR", settings.path().toUtf8());
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(true);
    app.setApplicationName(QStringLiteral("aether-icom-rx-readback-gui"));
    app.setOrganizationName(QStringLiteral("HF5A-RxAudit"));
    QWidget window;
    window.setWindowTitle(QStringLiteral("AetherSDR — RX-only readback audit"));
    auto* layout = new QVBoxLayout(&window);
    auto* notice = new QLabel(QStringLiteral("RX-only • one exclusive LAN session • no scope acquisition"), &window);
    notice->setAccessibleName(QStringLiteral("RX audit safety mode"));
    layout->addWidget(notice);
    auto* readback = new RxReadbackWidget(&window);
    layout->addWidget(readback);
    auto* spectrum = new SpectrumWidget(&window);
    spectrum->setAttribute(Qt::WA_TransparentForMouseEvents);
    spectrum->setFocusPolicy(Qt::NoFocus);
    spectrum->setVfoFrequency(0.0);
    spectrum->setAccessibleDescription(QStringLiteral("Read-only frequency scale. No measured spectrum in audit mode. TX readback is not VFO B."));
    layout->addWidget(spectrum, 1);
    window.resize(1100, 500);
    TransmitModel empty;
    readback->refresh(empty, false);
    if (app.arguments().contains(QStringLiteral("--self-check"))) {
        // Exercise the shipped GUI components without creating any session.
        TransmitDelta sample;
        sample.split = false; sample.splitValid = true;
        sample.splitDeadline = QDeadlineTimer(1000);
        sample.receiveDialFreq = 7.074; sample.receiveDialFreqValid = true;
        sample.receiveDialFreqDeadline = QDeadlineTimer(1000);
        sample.transmitFreq = 7.075; sample.transmitFreqValid = true;
        sample.transmitFreqDeadline = QDeadlineTimer(1000);
        empty.applyChanges(sample);
        readback->refresh(empty, true);
        spectrum->setConfirmedTxReadback(empty.confirmedTransmitFreq(), empty.transmitFreqDeadline());
        const bool valid = readback->text().contains(QStringLiteral("SPLIT OFF"))
            && readback->text().contains(QStringLiteral("+1000 Hz"))
            && spectrum->confirmedTxReadback() == std::optional<double>(7.075);
        empty.resetTransmitFrequencyObservation();
        readback->refresh(empty, false);
        spectrum->setConfirmedTxReadback({}, QDeadlineTimer{});
        const bool cleared = !spectrum->confirmedTxReadback()
            && !readback->text().contains(QStringLiteral("7.075000"));
        const int policy = runIcomRxAudit(app);
        std::printf("RX GUI self-check: %s; isolated entry; no session or sockets created\n",
                    valid && cleared && policy == 0 ? "PASS" : "FAIL");
        return valid && cleared && policy == 0 ? 0 : 1;
    }
    // The common runner refuses missing consent/credentials BEFORE attach.
    QTimer refresh;
    bool connected = false;
    return runIcomRxAudit(app, [&](IRadioBackend& backend, TransmitModel& transmit) {
        const auto render = [&] {
            readback->refresh(transmit, connected);
            const auto tx = connected ? transmit.confirmedTransmitFreq() : std::nullopt;
            spectrum->setConfirmedTxReadback(tx, transmit.transmitFreqDeadline());
            const auto rx = connected ? transmit.confirmedReceiveDialFreq() : std::nullopt;
            if (rx) {
                    spectrum->setFrequencyRangeImmediate(*rx, 0.020);
                    spectrum->setVfoFrequency(*rx);
            } else { spectrum->setVfoFrequency(0.0); }
        };
        QObject::connect(&backend, &IRadioBackend::connected, &transmit, [&] { connected = true; });
        QObject::connect(&backend, &IRadioBackend::disconnected, &transmit, [&] {
            connected = false;
            spectrum->setVfoFrequency(0.0);
            spectrum->setConfirmedTxReadback({}, QDeadlineTimer{});
            readback->refresh(transmit, false);
        });
        QObject::connect(&transmit, &TransmitModel::stateChanged, &transmit, render);
        QObject::connect(&refresh, &QTimer::timeout, &transmit, render);
        refresh.start(100);
        window.show();
    });
}
}
