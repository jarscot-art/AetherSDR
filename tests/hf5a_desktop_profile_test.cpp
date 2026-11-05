#include "Hf5aDesktopProfile.h"
#include "TestSettingsProfile.h"
#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/AutomationBridgeSettings.h"
#include "core/SettingsBootstrap.h"
#include "core/SettingsPaths.h"
#include "core/LocalMemoryStore.h"
#include "core/UpdateSettings.h"
#include "core/VoiceKeyerSettings.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <cstdio>

#ifdef HAVE_KEYCHAIN
#error HF5A desktop qualification must not link the production credential store
#endif

int main(int argc, char** argv)
{
    using namespace AetherSDR;
    TestSettingsProfile inherited(QStringLiteral("hf5a-inherited-profile"));
    const QString inheritedRoot = SettingsPaths::configDir();
    const QByteArray sentinel("<Settings><AutoConnectToLastRadio>True</AutoConnectToLastRadio>"
                              "<LastConnectedRadioSerial>production-sentinel</LastConnectedRadioSerial>"
                              "</Settings>");
    if (!QDir().mkpath(inheritedRoot)) { return 1; }
    QFile legacy(SettingsPaths::legacyXmlPath());
    if (!legacy.open(QIODevice::WriteOnly) || legacy.write(sentinel) != sentinel.size()) {
        return 1;
    }
    legacy.close();
    qputenv("AETHER_AUTOMATION", "1");
    Hf5aDesktopProfile profile;
    if (!profile.activate()) { return 1; }
    // Like the real entry point, bootstrap reads happen before QApplication.
    const bool bootstrapIsolated = SettingsPaths::configDir() == profile.path()
        && profile.path() != inheritedRoot
        && LocalMemoryStore::defaultFilePath() == profile.path() + "/memories.json"
        && SettingsBootstrap::readValue("LastConnectedRadioSerial").isEmpty()
        && !qEnvironmentVariableIsSet("AETHER_AUTOMATION");
    QCoreApplication app(argc, argv);
    AppSettings::instance().load();
    const bool initialized = profile.initializeSettings();
    AppSettings::instance().setValue("HF5AProfileSentinel", "test-only");
    AppSettings::instance().save();
    const bool isolatedWrite = QFileInfo::exists(profile.path() + "/AetherSDR.db")
        && !QFileInfo::exists(inheritedRoot + "/AetherSDR.db");
    const bool safeStartup = AppSettings::instance().value("AutoConnectToLastRadio").toString() == "False"
        && !AutomationBridgeSettings::enabled() && !UpdateSettings::checkOnStartup();
    const bool auxiliaryPaths = AudioEngine::wisdomFilePath().startsWith(profile.path() + "/fftw/")
        && VoiceKeyerSettings::recordingsDir() == profile.path() + "/VoiceKeyer"
        && AppSettings::instance().value("QsoRecordingDir").toString() == profile.path() + "/Recordings";
    if (!legacy.open(QIODevice::ReadOnly)) { return 1; }
    const bool untouched = legacy.readAll() == sentinel;
    legacy.close();
    Hf5aDesktopProfile nextLaunch;
    const bool fresh = nextLaunch.path() != profile.path()
        && !QFileInfo::exists(nextLaunch.path() + "/AetherSDR.db");
    // Reject a redirected profile before applying startup preferences.
    qputenv("AETHER_SETTINGS_DIR", inheritedRoot.toUtf8());
    const bool refusesRedirect = !profile.initializeSettings();
    qputenv("AETHER_SETTINGS_DIR", profile.path().toUtf8());
    const bool ok = bootstrapIsolated && initialized && isolatedWrite && safeStartup && auxiliaryPaths
        && untouched && fresh && refusesRedirect;
    std::printf("HF5A profile: bootstrap=%d initialized=%d isolatedWrite=%d safeStartup=%d "
                "auxiliaryPaths=%d untouched=%d fresh=%d refusesRedirect=%d\n", bootstrapIsolated, initialized,
                isolatedWrite, safeStartup, auxiliaryPaths, untouched, fresh, refusesRedirect);
    return ok ? 0 : 1;
}
