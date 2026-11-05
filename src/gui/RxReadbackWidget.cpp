#include "RxReadbackWidget.h"
#include "models/TransmitModel.h"
#include "core/ThemeManager.h"
#include <QAccessible>
#include <QHBoxLayout>
#include <QLabel>
#include <cmath>

namespace AetherSDR {
RxReadbackWidget::RxReadbackWidget(QWidget* parent) : QWidget(parent), m_label(new QLabel(this))
{
    setObjectName(QStringLiteral("RxReadback"));
    setAccessibleName(tr("Confirmed radio readback"));
    m_label->setAccessibleName(tr("SPLIT, TX and RX dial readback"));
    const auto description = tr("Read only. TX is radio-reported, not VFO B. TX minus RX dial excludes RIT. "
                               "Samples are independent, not an atomic snapshot. Unknown or expired values are omitted.");
    setToolTip(description); setAccessibleDescription(description);
    m_label->setToolTip(description); m_label->setAccessibleDescription(description);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->addWidget(m_label);
    ThemeManager::instance().applyStyleSheet(m_label,
        QStringLiteral("QLabel { color: {{color.text.secondary}}; }"));
}
QString RxReadbackWidget::text() const { return m_label->text(); }
void RxReadbackWidget::refresh(const TransmitModel& model, bool connected)
{
    const auto split = connected ? model.confirmedSplit() : std::nullopt;
    const auto tx = connected ? model.confirmedTransmitFreq() : std::nullopt;
    const auto rx = connected ? model.confirmedReceiveDialFreq() : std::nullopt;
    const QString unknown = connected ? tr("unconfirmed / expired") : tr("disconnected");
    const QString splitText = split ? (*split ? tr("ON") : tr("OFF")) : unknown;
    const auto frequency = [&](const auto& value) { return value ? QString::number(*value, 'f', 6) + tr(" MHz") : unknown; };
    const QString delta = tx && rx
        ? QStringLiteral("%1%2 Hz").arg(*tx >= *rx ? QStringLiteral("+") : QString())
              .arg(std::llround((*tx - *rx) * 1e6)) : unknown;
    const QString value = tr("SPLIT %1  |  TX %2  |  RX dial %3  |  TX − RX dial %4")
        .arg(splitText, frequency(tx), frequency(rx), delta);
    if (value == m_label->text()) { return; }
    m_label->setText(value);
    QAccessibleValueChangeEvent event(m_label, value);
    QAccessible::updateAccessibility(&event);
}
}
