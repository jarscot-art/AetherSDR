#pragma once
#include <functional>
class QCoreApplication;
namespace AetherSDR {
class IRadioBackend;
class TransmitModel;
using RxAuditAttach = std::function<void(IRadioBackend&, TransmitModel&)>;
// One immutable audit session, bounded duration, explicit exclusive-LAN consent.
int runIcomRxAudit(QCoreApplication& app, const RxAuditAttach& attach = {});
}
