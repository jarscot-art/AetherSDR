#pragma once

#include <cmath>
#include <optional>

// HF5A: one-click TX frequency selection via XIT. Pure logic deliberately
// independent of Qt, CI-V and VFO tuning. Never clamp an invalid click: a
// click outside the supported XIT span must not silently choose another QRG.
namespace Hf5aXitClick {

constexpr int kMaxOffsetHz = 9999;
constexpr int kClickTolerancePx = 4;

inline bool isStationaryClick(int pressX, int pressY, int releaseX, int releaseY)
{
    return std::abs(releaseX - pressX) + std::abs(releaseY - pressY)
        <= kClickTolerancePx;
}

inline std::optional<int> offsetHz(double selectedTxMhz, double vfoMhz)
{
    if (!std::isfinite(selectedTxMhz) || !std::isfinite(vfoMhz))
        return std::nullopt;

    const double rawHz = (selectedTxMhz - vfoMhz) * 1.0e6;
    if (!std::isfinite(rawHz) || rawHz < -kMaxOffsetHz - 0.5
        || rawHz > kMaxOffsetHz + 0.5)
        return std::nullopt;

    const long long roundedHz = std::llround(rawHz);
    if (roundedHz < -kMaxOffsetHz || roundedHz > kMaxOffsetHz)
        return std::nullopt;
    return static_cast<int>(roundedHz);
}

} // namespace Hf5aXitClick
