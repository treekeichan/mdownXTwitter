#pragma once
#include <QObject>
#include <QProcess>
#include <QJsonObject>

struct EngineOptions {
    QString executable;
    QString ffmpegDirectory;
    QString outputDirectory;
    QString proxy;
    QString cookies;
};

class DownloadEngine final : public QObject {
    Q_OBJECT
public:
    explicit DownloadEngine(QObject *parent = nullptr);
    ~DownloadEngine() override;
    bool busy() const;
    void inspect(const QString &url, const EngineOptions &options);
    void download(const QString &url, const QString &format, const EngineOptions &options, int playlistIndex = 0);
    void cancel();
signals:
    void inspected(const QJsonObject &info);
    void progress(double percent, const QString &speed, const QString &eta);
    void stageChanged(const QString &stage);
    void saved(const QString &path);
    void finished(bool success, bool cancelled, const QString &message);
private:
    enum class Mode { Inspect, Download };
    QProcess process;
    Mode mode = Mode::Inspect;
    QByteArray output, pendingOut, pendingErr;
    QString errors, savedPath;
    bool cancelled = false;
    bool reported = false;
#ifdef Q_OS_WIN
    void *jobHandle = nullptr;
#endif
    QStringList commonArguments(const EngineOptions &options) const;
    void start(const QStringList &args, const EngineOptions &options);
    void readOutput();
    void readErrors();
    void handleLine(const QByteArray &line, bool error);
    void complete(int code, QProcess::ExitStatus status);
};
