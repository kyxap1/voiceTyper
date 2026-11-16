#pragma once

#include <QObject>
#include <QString>

#include <vector>

class QNetworkAccessManager;

namespace vt {

struct ModelFile {
    const char* name;
    const char* sha256;
};

// A model published on Hugging Face. Files go to
// <AppConfigLocation>/models/<subdir>, a folder autodetect scans; the last
// file is the one the app loads, so it appears only once the set is complete.
struct ModelSpec {
    const char* subdir;
    const char* baseUrl;
    std::vector<ModelFile> files;
};

// Fetches a ModelSpec one file at a time, each verified against its pinned
// sha256. A file only gets its final name once verified, so present files are
// trusted and a re-run resumes with the missing ones.
class ModelDownloader final : public QObject {
    Q_OBJECT
public:
    static const ModelSpec& parakeet();
    static const ModelSpec& whisperSmall();

    explicit ModelDownloader(const ModelSpec& spec, QObject* parent = nullptr);

    // The file to load once finished() fires.
    static QString mainPath(const ModelSpec& spec);

    void start();

signals:
    void finished(const QString& mainPath);
    void failed(const QString& error);

private:
    void next();

    const ModelSpec& spec_;
    QNetworkAccessManager* nam_ = nullptr;
    size_t index_ = 0;
};

} // namespace vt
