#pragma once
#include <QDeadlineTimer>
#include <QPainter>
#include <QRect>
#include <optional>
#include <cmath>
#include <algorithm>
namespace AetherSDR {
struct TxReadbackMarker {
    std::optional<double> frequencyMhz;
    QDeadlineTimer deadline;
    std::optional<double> confirmed() const {
        return frequencyMhz && std::isfinite(*frequencyMhz) && *frequencyMhz > 0.0
            && !deadline.hasExpired() ? frequencyMhz : std::nullopt;
    }
    // x comes from the spectrum's existing frequency mapping (including crop).
    // This confirmed marker never moves optimistically; SpectrumWidget owns a separate preview.
    bool paint(QPainter& painter, const QRect& rect, int x, const QColor& color) const {
        if (!confirmed() || !rect.isValid() || x < rect.left() || x > rect.right()) { return false; }
        painter.save();
        painter.setClipRect(rect, Qt::IntersectClip);
        painter.setPen(QPen(color, 1, Qt::DashLine));
        painter.drawLine(x, rect.top(), x, rect.bottom());
        painter.drawText(QRect(std::max(rect.left(), x - 45), rect.top() + 2, 90, 20),
                         Qt::AlignCenter, QStringLiteral("TX readback"));
        painter.restore();
        return true;
    }
};
}
