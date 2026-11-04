#include "core/backends/icom/IcomCivBackend.h"
#include "core/backends/icom/IcomOutboundPolicy.h"
#include "models/TransmitModel.h"

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QAbstractSocket>
#include <QCoreApplication>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <cstdio>

using namespace AetherSDR;
using namespace AetherSDR::icom;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("aether-icom-rx-audit"));
    QCoreApplication::setOrganizationName(QStringLiteral("HF5A-RxAudit"));
    QCoreApplication::setApplicationVersion(QStringLiteral("stage2"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Offline by default. RX-only IC-7300MK2 diagnostic. A second LAN login may disrupt another client."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(QStringLiteral("self-check"), QStringLiteral("Check the fixed policy without creating any network session.")));
    parser.addOption(QCommandLineOption(QStringLiteral("connect"), QStringLiteral("Explicitly permit one real RS-BA1 connection (separate operator approval required).")));
    parser.addOption(QCommandLineOption(QStringLiteral("exclusive-lan-confirmed"), QStringLiteral("Operator confirms no other LAN client owns the radio.")));
    parser.addOption(QCommandLineOption(QStringLiteral("host"), QStringLiteral("Existing radio IPv4 address; no discovery or LAN changes."), QStringLiteral("IPv4")));
    parser.addOption(QCommandLineOption(QStringLiteral("credentials"), QStringLiteral("Private JSON file with username/password; never logged or persisted."), QStringLiteral("file")));
    parser.addOption(QCommandLineOption(QStringLiteral("seconds"), QStringLiteral("Observation duration, 5..120 seconds."), QStringLiteral("seconds"), QStringLiteral("30")));
    parser.addOption(QCommandLineOption(QStringLiteral("control-port"), QStringLiteral("Existing RS-BA1 control port."), QStringLiteral("port"), QStringLiteral("50001")));
    parser.addOption(QCommandLineOption(QStringLiteral("civ-port"), QStringLiteral("Existing RS-BA1 CI-V UDP port (not a COM port)."), QStringLiteral("port"), QStringLiteral("50002")));
    parser.addOption(QCommandLineOption(QStringLiteral("audio-port"), QStringLiteral("Existing RS-BA1 RX audio UDP port."), QStringLiteral("port"), QStringLiteral("50003")));
    parser.process(app);
    const auto policy = IcomOutboundPolicy::rxAudit();
    if (parser.isSet(QStringLiteral("self-check"))) {
        if (parser.isSet(QStringLiteral("connect"))) { return 2; }
        const bool safe = policy.allowsCiv(cmdReadSplit(0xB6))
            && policy.allowsCiv(cmdReadTransmitFrequency(0xB6))
            && !policy.allowsCiv(cmdSetPtt(0xB6, true))
            && !policy.allowsCiv(cmdSetPtt(0xB6, false))
            && !policy.allowsCiv(cmdScopeOnOff(0xB6, true));
        std::printf("RX policy self-check: %s; no session or sockets created\n", safe ? "PASS" : "FAIL");
        return safe ? 0 : 1;
    }
    if (!parser.isSet(QStringLiteral("connect"))
        || !parser.isSet(QStringLiteral("exclusive-lan-confirmed"))) {
        std::fprintf(stderr, "No connection attempted. Hardware use needs --connect and --exclusive-lan-confirmed.\n");
        return 2;
    }
    QHostAddress host(parser.value(QStringLiteral("host")));
    bool durationOk = false;
    const int seconds = parser.value(QStringLiteral("seconds")).toInt(&durationOk);
    if (host.protocol() != QAbstractSocket::IPv4Protocol || host.isNull() || host.isMulticast()
        || host == QHostAddress(QHostAddress::Broadcast) || !durationOk || seconds < 5 || seconds > 120) {
        std::fprintf(stderr, "Invalid explicit IPv4 address or duration; no connection attempted.\n");
        return 2;
    }
    const auto port = [&](const char* option) -> quint16 {
        bool ok = false;
        const uint value = parser.value(QString::fromLatin1(option)).toUInt(&ok);
        return ok && value > 0 && value <= 65535 ? static_cast<quint16>(value) : 0;
    };
    const quint16 controlPort = port("control-port"), civPort = port("civ-port"), audioPort = port("audio-port");
    if (!controlPort || !civPort || !audioPort) { return 2; }
    QFile credentials(parser.value(QStringLiteral("credentials")));
    if (!credentials.open(QIODevice::ReadOnly) || credentials.size() > 4096) {
        std::fprintf(stderr, "Cannot read bounded credentials file; no connection attempted.\n");
        return 2;
    }
    const auto object = QJsonDocument::fromJson(credentials.readAll()).object();
    const QString username = object.value(QStringLiteral("username")).toString();
    const QString password = object.value(QStringLiteral("password")).toString();
    credentials.close();
    if (username.isEmpty() || password.isEmpty() || username.toUtf8().size() > 16
        || password.toUtf8().size() > 16 || username.contains(QChar(0)) || password.contains(QChar(0))) {
        std::fprintf(stderr, "Invalid credentials format; no connection attempted.\n");
        return 2;
    }
    QTemporaryDir settings;
    if (!settings.isValid()) { return 2; }
    qputenv("AETHER_SETTINGS_DIR", settings.path().toUtf8());
    QStandardPaths::setTestModeEnabled(true);

    IcomCivBackend backend(nullptr, policy);
    TransmitModel transmit;
    QObject::connect(&backend, &IRadioBackend::transmitChanged, &transmit, &TransmitModel::applyChanges);
    bool closing = false;
    const auto report = [&] {
        QJsonObject result = QJsonObject::fromVariantMap(backend.rxAuditDiagnostics());
        const auto confirmed = transmit.confirmedTransmitFreq();
        result.insert(QStringLiteral("confirmedTransmitMHz"), confirmed ? QJsonValue(*confirmed) : QJsonValue());
        QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << Qt::endl;
    };
    const auto finish = [&](int code) {
        if (closing) { return; }
        closing = true;
        report();
        backend.disconnectRadio();
        report();
        app.exit(code);
    };
    QObject::connect(&backend, &IRadioBackend::connectionError, &app, [&](const QString&) { finish(3); });
    QObject::connect(&backend, &IRadioBackend::disconnected, &app, [&] { if (!closing) { finish(3); } });
    QTimer status;
    QObject::connect(&status, &QTimer::timeout, &app, report);
    status.start(1000);
    QTimer::singleShot(seconds * 1000, &app, [&] {
        const QVariantMap diagnostics = backend.rxAuditDiagnostics();
        const auto split = diagnostics.value(QStringLiteral("splitTxReadback")).toMap()
            .value(QStringLiteral("split")).toMap();
        const bool passed = split.value(QStringLiteral("valid")).toBool()
            && transmit.confirmedTransmitFreq().has_value()
            && diagnostics.value(QStringLiteral("deniedDatagrams")).toULongLong() == 0
            && diagnostics.value(QStringLiteral("deniedRequests")).toULongLong() == 0;
        finish(passed ? 0 : 1);
    });
    RadioConnectRequest request;
    request.host = host.toString(); request.port = controlPort;
    request.params = {{QStringLiteral("icom.username"), username}, {QStringLiteral("icom.password"), password},
                      {QStringLiteral("icom.serialPort"), civPort}, {QStringLiteral("icom.audioPort"), audioPort},
                      {QStringLiteral("icom.civAddress"), 0xB6}, {QStringLiteral("icom.civAddressPinned"), true}};
    // Defer so synchronous connection failure also has a running event loop to exit.
    QTimer::singleShot(0, &app, [&] { backend.connectRadio(request); });
    return app.exec();
}
