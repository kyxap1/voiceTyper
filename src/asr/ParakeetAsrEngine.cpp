#include "asr/ParakeetAsrEngine.h"

#include "core/Logging.h"

#include <QDir>
#include <QFileInfo>

#include <sherpa-onnx/c-api/c-api.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

namespace vt {

namespace {

// First file in `dir` matching `pattern`, or empty.
QByteArray findFile(const QDir& dir, const QString& pattern) {
    const QStringList hits = dir.entryList({pattern}, QDir::Files, QDir::Name);
    return hits.isEmpty() ? QByteArray() : dir.filePath(hits.first()).toUtf8();
}

} // namespace

bool ParakeetAsrEngine::isParakeetModel(const std::string& path) {
    return QString::fromStdString(path).endsWith(QLatin1String(".onnx"),
                                                 Qt::CaseInsensitive);
}

bool ParakeetAsrEngine::supportsLanguage(const std::string& code) {
    static const QStringList kLanguages{
        "bg", "cs", "da", "de", "el", "en", "es", "et", "fi", "fr", "hr", "hu", "it",
        "lt", "lv", "mt", "nl", "pl", "pt", "ro", "ru", "sk", "sl", "sv", "uk"};
    return code.empty() || code == "auto" ||
           kLanguages.contains(QString::fromStdString(code));
}

ParakeetAsrEngine::ParakeetAsrEngine(const std::string& encoderPath) {
    const QFileInfo encoder(QString::fromStdString(encoderPath));
    const QDir dir = encoder.absoluteDir();
    modelDir_ = dir.absolutePath().toStdString();

    const QByteArray enc = encoder.absoluteFilePath().toUtf8();
    const QByteArray dec = findFile(dir, QStringLiteral("decoder*.onnx"));
    const QByteArray join = findFile(dir, QStringLiteral("joiner*.onnx"));
    const QByteArray tokens = findFile(dir, QStringLiteral("tokens.txt"));
    if (!encoder.exists() || dec.isEmpty() || join.isEmpty() || tokens.isEmpty()) {
        qCWarning(vtAsr) << "Parakeet: incomplete model in" << dir.absolutePath()
                         << "- need encoder*.onnx, decoder*.onnx, joiner*.onnx, tokens.txt";
        return;
    }

    SherpaOnnxOfflineRecognizerConfig config;
    std::memset(&config, 0, sizeof(config));
    config.decoding_method = "greedy_search";
    config.model_config.transducer.encoder = enc.constData();
    config.model_config.transducer.decoder = dec.constData();
    config.model_config.transducer.joiner = join.constData();
    config.model_config.tokens = tokens.constData();
    // ponytail: CPU only; try "coreml" / "cuda" / "directml" if CPU is too slow.
    config.model_config.provider = "cpu";
    config.model_config.num_threads =
        static_cast<int32_t>(std::clamp(std::thread::hardware_concurrency() / 2, 1u, 8u));

    recognizer_ = SherpaOnnxCreateOfflineRecognizer(&config);
    if (recognizer_)
        qCInfo(vtAsr) << "Parakeet: loaded" << dir.absolutePath();
    else
        qCWarning(vtAsr) << "Parakeet: failed to load" << dir.absolutePath();
}

ParakeetAsrEngine::~ParakeetAsrEngine() {
    if (recognizer_)
        SherpaOnnxDestroyOfflineRecognizer(recognizer_);
}

TranscriptionResult ParakeetAsrEngine::transcribe(const AudioBuffer& audio,
                                                  const TranscriptionOptions& options) {
    TranscriptionResult r;
    r.ok = false;
    if (!recognizer_)
        return r;

    std::lock_guard<std::mutex> lock(mutex_);
    // ponytail: abort is only checked before decoding; one pass can't be
    // interrupted midway.
    if (options.abortFlag && options.abortFlag->load())
        return r;

    const auto t0 = std::chrono::steady_clock::now();
    const SherpaOnnxOfflineStream* stream = SherpaOnnxCreateOfflineStream(recognizer_);
    if (!stream)
        return r;
    SherpaOnnxAcceptWaveformOffline(stream, audio.sampleRate, audio.samples.data(),
                                    static_cast<int32_t>(audio.samples.size()));
    SherpaOnnxDecodeOfflineStream(recognizer_, stream);
    const SherpaOnnxOfflineRecognizerResult* result =
        SherpaOnnxGetOfflineStreamResult(stream);
    if (result) {
        r.text = result->text ? result->text : "";
        r.ok = true;
        SherpaOnnxDestroyOfflineRecognizerResult(result);
    }
    SherpaOnnxDestroyOfflineStream(stream);

    r.durationSeconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

std::string ParakeetAsrEngine::backendName() const {
    return "Parakeet (sherpa-onnx, CPU) — " + modelDir_;
}

} // namespace vt
