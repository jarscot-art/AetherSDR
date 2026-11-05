#pragma once
#include <QWidget>
class QLabel;
namespace AetherSDR {
class TransmitModel;
// Presentation only. No radio/model setters or command-producing signals.
class RxReadbackWidget : public QWidget {
public:
    explicit RxReadbackWidget(QWidget* parent = nullptr);
    void refresh(const TransmitModel& model, bool connected);
    QString text() const;
private:
    QLabel* m_label;
};
}
