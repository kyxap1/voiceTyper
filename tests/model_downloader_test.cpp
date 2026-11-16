// Integration test: downloads the whisper fallback model from Hugging Face
// into $XDG_CONFIG_HOME and checks it lands verified. Needs network.
// Link ModelDownloader, Qt Core and Qt Network.
#include "asr/ModelDownloader.h"
#include "core/Logging.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <cstdio>

Q_LOGGING_CATEGORY(vtAsr, "voicetyper.asr")

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("voiceTyper"));
    QCoreApplication::setOrganizationName(QStringLiteral("voiceTyper"));

    vt::ModelDownloader dl(vt::ModelDownloader::whisperSmall());
    int rc = 1;
    QObject::connect(&dl, &vt::ModelDownloader::finished, [&](const QString& path) {
        const qint64 size = QFileInfo(path).size();
        std::printf("finished: %s (%lld bytes)\n", qPrintable(path), size);
        rc = size == 190085487 ? 0 : 1;
        app.quit();
    });
    QObject::connect(&dl, &vt::ModelDownloader::failed, [&](const QString& error) {
        std::printf("failed: %s\n", qPrintable(error));
        app.quit();
    });
    dl.start();
    app.exec();
    std::printf("%s\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}
