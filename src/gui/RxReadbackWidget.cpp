#include "RxReadbackWidget.h"
#include "models/TransmitModel.h"
#include "core/ThemeManager.h"
#include <QAccessible>
#include <QVBoxLayout>
#include <QLabel>
#include <cmath>

namespace AetherSDR {
RxReadbackWidget::RxReadbackWidget(QWidget* parent)
    : QWidget(parent), m_label(new QLabel(this)), m_unselectedLabel(new QLabel(this))
{
    setObjectName(QStringLiteral("RxReadback"));
    setAccessibleName(tr("Confirmed radio readback"));
    m_label->setAccessibleName(tr("SPLIT, TX and RX dial readback"));
    const auto description = tr("Read only. TX is radio-reported, not VFO B. TX minus RX dial excludes RIT. "
                               "Samples are independent, not an atomic snapshot. Unknown or expired values are omitted.");
    setToolTip(description); setAccessibleDescription(description);
    m_label->setToolTip(description); m_label->setAccessibleDescription(description);
    m_unselectedLabel->setObjectName(QStringLiteral("UnselectedVfoReadback"));
    m_unselectedLabel->setAccessibleName(tr("VFO NIEWYBRANE"));
    const auto vfoDescription = tr("Read only: the VFO not currently selected by the radio. "
        "Not a physical VFO B or a TX frequency. Frequency and mode are independent samples; "
        "DATA and FIL1/FIL2/FIL3 belong to the mode sample. Expired values are omitted.");
    m_unselectedLabel->setToolTip(vfoDescription);
    m_unselectedLabel->setAccessibleDescription(vfoDescription);
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->addWidget(m_label);
    layout->addWidget(m_unselectedLabel);
    ThemeManager::instance().applyStyleSheet(m_label,
        QStringLiteral("QLabel { color: {{color.text.secondary}}; }"));
    ThemeManager::instance().applyStyleSheet(m_unselectedLabel,
        QStringLiteral("QLabel { color: {{color.text.secondary}}; } "
                       "QLabel:disabled { color: {{color.control.unavailable}}; }"));
}
QString RxReadbackWidget::text() const { return m_label->text(); }
QString RxReadbackWidget::unselectedVfoText() const { return m_unselectedLabel->text(); }
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
    const auto updateLabel = [](QLabel* label, const QString& text) {
        if (text == label->text()) { return; }
        label->setText(text);
        QAccessibleValueChangeEvent event(label, text);
        QAccessible::updateAccessibility(&event);
    };
    updateLabel(m_label, value);
    const auto vfoFrequency = connected ? model.confirmedUnselectedVfoFrequency() : std::nullopt;
    const auto vfoMode = connected ? model.confirmedUnselectedVfoMode() : std::nullopt;
    const QString vfoUnknown = connected && !model.hasUnselectedVfoReadback()
        ? tr("unavailable / unconfirmed") : unknown;
    const QString modeText = vfoMode
        ? tr("%1  |  DATA %2  |  FIL%3").arg(vfoMode->emission,
              vfoMode->data ? tr("ON") : tr("OFF")).arg(vfoMode->filter)
        : tr("emission / DATA / filter: %1").arg(vfoUnknown);
    const QString vfoValue = !vfoFrequency && !vfoMode
        ? tr("VFO NIEWYBRANE  |  %1").arg(vfoUnknown)
        : tr("VFO NIEWYBRANE  |  %1  |  %2")
              .arg(vfoFrequency ? frequency(vfoFrequency) : vfoUnknown, modeText);
    m_unselectedLabel->setEnabled(vfoFrequency.has_value() || vfoMode.has_value());
    updateLabel(m_unselectedLabel, vfoValue);
}
}
