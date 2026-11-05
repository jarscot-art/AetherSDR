#include "core/backends/icom/IcomRxAuditRunner.h"
#include <QCoreApplication>
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    return AetherSDR::runIcomRxAudit(app);
}
