// Integration test: requires the sherpa-onnx Parakeet model and recordings
// decoded to mono 16 kHz float32 PCM. Link ParakeetAsrEngine, Qt Core and
// sherpa-onnx-c-api.
// Usage: test ENCODER_ONNX AUDIO.f32 EXPECTED_SUBSTRING [AUDIO.f32 EXPECTED ...]
// An empty EXPECTED only checks that some text came back.
#include "asr/ParakeetAsrEngine.h"
#include "core/Logging.h"

#include <QString>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

Q_LOGGING_CATEGORY(vtAsr, "voicetyper.asr")

int main(int argc, char** argv) {
    if (argc < 4 || (argc - 2) % 2)
        return 1;
    vt::ParakeetAsrEngine engine(argv[1]);
    if (!engine.isReady())
        return 2;

    int failures = 0;
    for (int i = 2; i + 1 < argc; i += 2) {
        std::ifstream file(argv[i], std::ios::binary);
        const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), {});
        vt::AudioBuffer audio;
        audio.samples.resize(bytes.size() / sizeof(float));
        std::memcpy(audio.samples.data(), bytes.data(),
                    audio.samples.size() * sizeof(float));

        const vt::TranscriptionResult r = engine.transcribe(audio, {});
        const QString text = QString::fromStdString(r.text);
        const bool pass = r.ok && !text.trimmed().isEmpty() &&
                          text.contains(QString::fromUtf8(argv[i + 1]),
                                        Qt::CaseInsensitive);
        std::printf("%s %s (%.1f s audio, %.2f s): %s\n", pass ? "PASS" : "FAIL",
                    argv[i], audio.durationSeconds(), r.durationSeconds,
                    r.text.c_str());
        failures += pass ? 0 : 1;
    }

    // Silence must not crash or hallucinate a long text.
    vt::AudioBuffer silence;
    silence.samples.assign(16000, 0.0f);
    const vt::TranscriptionResult s = engine.transcribe(silence, {});
    std::printf("%s silence: '%s'\n", s.ok && s.text.size() < 20 ? "PASS" : "FAIL",
                s.text.c_str());
    failures += s.ok && s.text.size() < 20 ? 0 : 1;

    return failures;
}
