#pragma once

#include "asr/IAsrEngine.h"

#include <mutex>
#include <string>

struct SherpaOnnxOfflineRecognizer; // fwd-decl from sherpa-onnx c-api.h

namespace vt {

// NVIDIA Parakeet (sherpa-onnx export, e.g.
// sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8) via sherpa-onnx. encoderPath is
// the model's encoder*.onnx; decoder*.onnx, joiner*.onnx and tokens.txt are
// taken from the same folder. Parakeet detects the language itself, so
// language, translate and the initial prompt are ignored.
class ParakeetAsrEngine final : public IAsrEngine {
public:
    explicit ParakeetAsrEngine(const std::string& encoderPath);
    ~ParakeetAsrEngine() override;

    ParakeetAsrEngine(const ParakeetAsrEngine&) = delete;
    ParakeetAsrEngine& operator=(const ParakeetAsrEngine&) = delete;

    TranscriptionResult transcribe(const AudioBuffer& audio,
                                   const TranscriptionOptions& options) override;
    [[nodiscard]] bool isReady() const override { return recognizer_ != nullptr; }
    [[nodiscard]] std::string backendName() const override;

    // True when `path` names a Parakeet encoder (*.onnx) rather than a whisper
    // model.
    static bool isParakeetModel(const std::string& path);
    // True for "auto"/"" and the 25 languages Parakeet v3 was trained on.
    static bool supportsLanguage(const std::string& code);

private:
    const SherpaOnnxOfflineRecognizer* recognizer_ = nullptr;
    std::string modelDir_;
    std::mutex mutex_;
};

} // namespace vt
