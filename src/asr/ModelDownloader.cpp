#include "asr/ModelDownloader.h"

#include "core/Logging.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

#include <memory>

namespace vt {

namespace {

QString modelDir(const ModelSpec& spec) {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(QStringLiteral("models/") + QString::fromLatin1(spec.subdir));
}

} // namespace

// Base URLs pin a revision: a moved `main` must not break the hashes.
const ModelSpec& ModelDownloader::parakeet() {
    static const ModelSpec spec{
        "parakeet-tdt-0.6b-v3-int8",
        "https://huggingface.co/csukuangfj/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8/"
        "resolve/ed5d745623472f97cfc2e6c2916ee248798fc5a8/",
        {
            {"tokens.txt", "d58544679ea4bc6ac563d1f545eb7d474bd6cfa467f0a6e2c1dc1c7d37e3c35d"},
            {"decoder.int8.onnx", "0222a86147299d4dee90edf95b6d13827edb42ad8b7c9cf0b2582f030937c0b5"},
            {"joiner.int8.onnx", "cbd02f3696e9944970111fb439d9c3fd365a80374565f13f2b9c7007d253fc98"},
            {"encoder.int8.onnx", "1afbe69c1ffdabee94755f42e4daf1d93549131a3da11356bda84fcef0f2c479"},
        }};
    return spec;
}

const ModelSpec& ModelDownloader::whisperSmall() {
    static const ModelSpec spec{
        "",
        "https://huggingface.co/ggerganov/whisper.cpp/"
        "resolve/5359861c739e955e79d9a303bcbc70fb988958b1/",
        {
            {"ggml-small-q5_1.bin", "ae85e4a935d7a567bd102fe55afc16bb595bdb618e11b2fc7591bc08120411bb"},
        }};
    return spec;
}

ModelDownloader::ModelDownloader(const ModelSpec& spec, QObject* parent)
    : QObject(parent), spec_(spec), nam_(new QNetworkAccessManager(this)) {}

QString ModelDownloader::mainPath(const ModelSpec& spec) {
    return QDir(modelDir(spec)).filePath(QString::fromLatin1(spec.files.back().name));
}

void ModelDownloader::start() {
    if (!QDir().mkpath(modelDir(spec_))) {
        emit failed(tr("Cannot create %1").arg(modelDir(spec_)));
        return;
    }
    index_ = 0;
    next();
}

void ModelDownloader::next() {
    const QDir d(modelDir(spec_));
    while (index_ < spec_.files.size() &&
           QFile::exists(d.filePath(QString::fromLatin1(spec_.files[index_].name))))
        ++index_;
    if (index_ == spec_.files.size()) {
        emit finished(mainPath(spec_));
        return;
    }

    const ModelFile f = spec_.files[index_];
    const QString finalPath = d.filePath(QString::fromLatin1(f.name));
    auto part = std::make_shared<QFile>(finalPath + QStringLiteral(".part"));
    if (!part->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit failed(tr("Cannot write %1").arg(part->fileName()));
        return;
    }
    auto hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256);

    QNetworkRequest req(QUrl(QString::fromLatin1(spec_.baseUrl) + QString::fromLatin1(f.name)));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    // Aborts after a minute without data, not after a minute in total.
    req.setTransferTimeout(60000);
    qCInfo(vtAsr) << "Downloading" << req.url().toString();

    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::readyRead, this, [reply, part, hash]() {
        const QByteArray chunk = reply->readAll();
        hash->addData(chunk);
        if (part->write(chunk) != chunk.size())
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, part, hash, finalPath, f]() {
        reply->deleteLater();
        const QByteArray tail = reply->readAll();
        hash->addData(tail);
        part->write(tail);
        part->close();

        QString error;
        if (reply->error() != QNetworkReply::NoError)
            error = reply->errorString();
        else if (part->error() != QFileDevice::NoError)
            error = part->errorString();
        else if (hash->result().toHex() != QByteArray(f.sha256))
            error = tr("checksum mismatch for %1").arg(QString::fromLatin1(f.name));
        else if (!part->rename(finalPath))
            error = part->errorString();

        if (!error.isEmpty()) {
            part->remove();
            qCWarning(vtAsr) << "Download failed:" << error;
            emit failed(error);
            return;
        }
        ++index_;
        next();
    });
}

} // namespace vt
