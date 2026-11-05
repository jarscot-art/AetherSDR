#pragma once

#include <QString>
#include <QTemporaryDir>

namespace AetherSDR {
// A fresh profile for each qualification launch. Kept on disk for diagnostics;
// never imports a saved radio, credential or peripheral from another launch.
class Hf5aDesktopProfile {
public:
    Hf5aDesktopProfile();
    bool activate(); // before QApplication and SettingsBootstrap
    bool initializeSettings(); // after AppSettings::load, before MainWindow
    QString path() const { return m_directory.path(); }

private:
    QTemporaryDir m_directory;
};
} // namespace AetherSDR
