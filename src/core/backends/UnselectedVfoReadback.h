#pragma once
#include <QDeadlineTimer>
#include <QString>
#include <optional>

namespace AetherSDR {
// Relative to the selected VFO, never a physical A/B identity or TX frequency.
// Frequency and mode are independently sampled; DATA and filter belong to mode.
struct UnselectedVfoMode {
    QString emission;
    bool data = false;
    int filter = 0;
};
struct UnselectedVfoObservation {
    bool supported = false;
    std::optional<double> frequencyMHz;
    std::optional<UnselectedVfoMode> mode;
    // Absolute monotonic deadlines survive queued delivery without renewal.
    QDeadlineTimer frequencyDeadline;
    QDeadlineTimer modeDeadline;
};
}
