#include <qt6keychain/keychain.h>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QUuid>
#include <cstdio>

// Opt-in qualification only: never use the application's credential namespace.
static bool finish(QKeychain::Job& job, QKeychain::Error expected = QKeychain::NoError)
{
    QEventLoop loop;
    bool completed = false;
    job.setAutoDelete(false);
    job.setInsecureFallback(false);
    QObject::connect(&job, &QKeychain::Job::finished, &loop, [&] {
        completed = true;
        loop.quit();
    });
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    job.start();
    if (!completed) { loop.exec(); }
    if (!completed || job.error() != expected) {
        std::fprintf(stderr, "Keychain operation: completed=%d error=%d\n", completed, int(job.error()));
    }
    return completed && job.error() == expected;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QString service = QStringLiteral("HF5A-CI-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString key = QStringLiteral("disposable-probe");
    const QString value = QUuid::createUuid().toString();
    QKeychain::WritePasswordJob write(service);
    write.setKey(key);
    write.setTextData(value);
    const bool written = finish(write);
    QKeychain::ReadPasswordJob read(service);
    read.setKey(key);
    const bool recovered = written && finish(read) && read.textData() == value;
    // Cleanup is attempted even if write/read failed; cleanup failure is a FAIL.
    QKeychain::DeletePasswordJob remove(service);
    remove.setKey(key);
    const bool removed = finish(remove);
    QKeychain::ReadPasswordJob missing(service);
    missing.setKey(key);
    const bool absent = finish(missing, QKeychain::EntryNotFound);
    std::printf("QtKeychain real backend: write=%d read=%d delete=%d absent=%d; unique CI service\n",
                written, recovered, removed, absent);
    return written && recovered && removed && absent ? 0 : 1;
}
