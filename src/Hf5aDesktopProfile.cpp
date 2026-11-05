#include "Hf5aDesktopProfile.h"
#include "core/AppSettings.h"
#include "core/AutomationBridgeSettings.h"
#include "core/SettingsPaths.h"
#include "core/UpdateSettings.h"
#include <QDir>
#include <QtGlobal>

#ifdef HAVE_KEYCHAIN
#error HF5A desktop qualification must not use the production credential store
#endif

namespace AetherSDR {
Hf5aDesktopProfile::Hf5aDesktopProfile()
    : m_directory(QDir::tempPath() + QStringLiteral("/hf5a-desktop-stage4-XXXXXX"))
{
    m_directory.setAutoRemove(false);
}

bool Hf5aDesktopProfile::activate()
{
    // Override even an inherited production-profile or automation environment.
    return m_directory.isValid()
        && qputenv("AETHER_SETTINGS_DIR", path().toUtf8())
        && qunsetenv("AETHER_AUTOMATION");
}

bool Hf5aDesktopProfile::initializeSettings()
{
    if (!m_directory.isValid() || SettingsPaths::configDir() != path()) {
        return false;
    }
    auto& settings = AppSettings::instance();
    settings.setValue("AutoConnectToLastRadio", "False");
    settings.setValue("QsoRecordingDir", path() + QStringLiteral("/Recordings"));
    AutomationBridgeSettings::setEnabled(false);
    UpdateSettings::setCheckOnStartup(false);
    settings.save();
    return settings.value("AutoConnectToLastRadio").toString() == QStringLiteral("False")
        && settings.value("LastConnectedRadioSerial").toString().isEmpty()
        && settings.value("LastRoutedRadioIp").toString().isEmpty()
        && !AutomationBridgeSettings::enabled()
        && !UpdateSettings::checkOnStartup();
}
} // namespace AetherSDR
