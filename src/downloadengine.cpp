#include "downloadengine.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QTimer>
#include <QDir>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

DownloadEngine::DownloadEngine(QObject *parent) : QObject(parent) {
#ifdef Q_OS_WIN
    connect(&process, &QProcess::started, this, [this] {
        if (jobHandle) CloseHandle(static_cast<HANDLE>(jobHandle));
        jobHandle = CreateJobObjectW(nullptr, nullptr);
        if (!jobHandle) return;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(static_cast<HANDLE>(jobHandle), JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        HANDLE child = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(process.processId()));
        const bool assigned = child && AssignProcessToJobObject(static_cast<HANDLE>(jobHandle), child);
        if (child) CloseHandle(child);
        if (!assigned) { CloseHandle(static_cast<HANDLE>(jobHandle)); jobHandle = nullptr; }
    });
#endif
    connect(&process, &QProcess::readyReadStandardOutput, this, &DownloadEngine::readOutput);
    connect(&process, &QProcess::readyReadStandardError, this, &DownloadEngine::readErrors);
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &DownloadEngine::complete);
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && !reported) {
            reported = true;
            emit finished(false, false, tr("下载引擎无法启动，请检查 yt-dlp.exe 路径。"));
        }
    });
}

DownloadEngine::~DownloadEngine() {
    process.disconnect(this);
#ifdef Q_OS_WIN
    if (jobHandle) { CloseHandle(static_cast<HANDLE>(jobHandle)); jobHandle = nullptr; }
#endif
    if (busy()) { process.kill(); process.waitForFinished(1000); }
}

bool DownloadEngine::busy() const { return process.state() != QProcess::NotRunning; }

QStringList DownloadEngine::commonArguments(const EngineOptions &o) const {
    QStringList args{QStringLiteral("--ignore-config"), QStringLiteral("--no-colors"),
        QStringLiteral("--encoding"), QStringLiteral("utf-8"),
        QStringLiteral("--socket-timeout"), QStringLiteral("25"),
        QStringLiteral("--retries"), QStringLiteral("3")};
    if (!o.proxy.isEmpty()) args << QStringLiteral("--proxy") << o.proxy;
    if (!o.cookies.isEmpty()) args << QStringLiteral("--cookies") << o.cookies;
    if (!o.ffmpegDirectory.isEmpty()) args << QStringLiteral("--ffmpeg-location") << o.ffmpegDirectory;
    return args;
}

void DownloadEngine::inspect(const QString &url, const EngineOptions &o) {
    if (busy()) return;
    mode = Mode::Inspect;
    auto args = commonArguments(o);
    args << QStringLiteral("--dump-single-json") << QStringLiteral("--skip-download")
         << QStringLiteral("--") << url;
    start(args, o);
}

void DownloadEngine::download(const QString &url, const QString &format, const EngineOptions &o, int playlistIndex) {
    if (busy()) return;
    mode = Mode::Download;
    auto args = commonArguments(o);
    if (playlistIndex > 0) args << QStringLiteral("--playlist-items") << QString::number(playlistIndex);
    args << QStringLiteral("--newline") << QStringLiteral("--progress")
         << QStringLiteral("--progress-delta") << QStringLiteral("0.3")
         << QStringLiteral("--progress-template")
         << QStringLiteral("download:MDOWN_PROGRESS:%(progress)j")
         << QStringLiteral("--print") << QStringLiteral("after_move:MDOWN_FILE:%(filepath)j")
         << QStringLiteral("--no-simulate")
         << QStringLiteral("--windows-filenames") << QStringLiteral("--continue")
         << QStringLiteral("--no-overwrites")
         << QStringLiteral("-P") << o.outputDirectory
         << QStringLiteral("-o") << QStringLiteral("%(uploader_id|unknown)s_%(id)s.%(ext)s")
         << QStringLiteral("-f") << format << QStringLiteral("--") << url;
    start(args, o);
}

void DownloadEngine::start(const QStringList &args, const EngineOptions &o) {
    output.clear(); pendingOut.clear(); pendingErr.clear(); errors.clear(); savedPath.clear();
    cancelled = false; reported = false;
    process.setProgram(o.executable);
    process.setArguments(args);
    process.start();
}

void DownloadEngine::readOutput() {
    const auto bytes = process.readAllStandardOutput();
    if (mode == Mode::Inspect) output += bytes;
    else {
        pendingOut += bytes;
        int i;
        while ((i = pendingOut.indexOf('\n')) >= 0) {
            handleLine(pendingOut.left(i).trimmed(), false); pendingOut.remove(0, i + 1);
        }
    }
}

void DownloadEngine::readErrors() {
    pendingErr += process.readAllStandardError();
    int i;
    while ((i = pendingErr.indexOf('\n')) >= 0) {
        handleLine(pendingErr.left(i).trimmed(), true); pendingErr.remove(0, i + 1);
    }
}

void DownloadEngine::handleLine(const QByteArray &line, bool error) {
    if (line.startsWith("MDOWN_PROGRESS:")) {
        const auto p = QJsonDocument::fromJson(line.mid(14)).object();
        const double total = p.value(QStringLiteral("total_bytes")).toDouble(
            p.value(QStringLiteral("total_bytes_estimate")).toDouble());
        const double received = p.value(QStringLiteral("downloaded_bytes")).toDouble();
        const double speed = p.value(QStringLiteral("speed")).toDouble();
        const int eta = p.value(QStringLiteral("eta")).toInt(-1);
        emit progress(total > 0 ? qBound(0.0, received * 100.0 / total, 100.0) : -1,
            speed > 0 ? tr("%1 MiB/s").arg(speed / 1048576.0, 0, 'f', 2) : QString(),
            eta >= 0 ? tr("剩余 %1 秒").arg(eta) : QString());
    } else if (line.startsWith("MDOWN_FILE:")) {
        const auto a = QJsonDocument::fromJson(QByteArray("[") + line.mid(10) + QByteArray("]")).array();
        if (!a.isEmpty()) savedPath = a.first().toString();
    } else if (error && !line.isEmpty()) {
        // Do not expose engine diagnostics: they can contain cookie paths, proxy credentials or signed URLs.
        const QString text = QString::fromUtf8(line).toLower();
        if (text.contains(QStringLiteral("429"))) errors = tr("请求过于频繁，请稍后重试。");
        else if (text.contains(QStringLiteral("login")) || text.contains(QStringLiteral("cookie")) || text.contains(QStringLiteral("403")))
            errors = tr("访问受限或登录状态无效，请检查 Cookie 与网络设置。");
        else if (text.contains(QStringLiteral("ffmpeg"))) errors = tr("需要 FFmpeg，请在设置中指定其所在目录。");
        else if (text.contains(QStringLiteral("requested format"))) errors = tr("所选格式不可用，请重新解析并选择清晰度。");
        else if (text.contains(QStringLiteral("no video"))) errors = tr("该帖子没有可下载的视频。");
        else if (text.contains(QStringLiteral("error")) && errors.isEmpty()) errors = tr("解析或下载失败，请检查链接、网络及引擎版本后重试。");
    } else if (line.startsWith("[Merger]") || line.startsWith("[Fixup")) {
        emit stageChanged(tr("处理视频中"));
    }
}

void DownloadEngine::complete(int code, QProcess::ExitStatus status) {
#ifdef Q_OS_WIN
    if (jobHandle) { CloseHandle(static_cast<HANDLE>(jobHandle)); jobHandle = nullptr; }
#endif
    readOutput(); readErrors();
    if (!pendingOut.isEmpty()) handleLine(pendingOut.trimmed(), false);
    if (!pendingErr.isEmpty()) handleLine(pendingErr.trimmed(), true);
    if (reported) return;
    reported = true;
    if (cancelled) { emit finished(false, true, tr("已取消")); return; }
    if (code != 0 || status != QProcess::NormalExit) {
        emit finished(false, false, errors.isEmpty() ? tr("引擎执行失败，请检查网络及配置。") : errors); return;
    }
    if (mode == Mode::Inspect) {
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(output, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            emit finished(false, false, tr("引擎返回的数据无法解析。")); return;
        }
        emit inspected(doc.object());
    } else {
        if (savedPath.isEmpty()) { emit finished(false, false, tr("引擎未返回保存路径，请检查输出目录。")); return; }
        emit saved(savedPath);
    }
    emit finished(true, false, QString());
}

void DownloadEngine::cancel() {
    if (!busy()) return;
    cancelled = true;
#ifdef Q_OS_WIN
    // Job objects stop FFmpeg descendants without blocking the GUI on taskkill.
    if (jobHandle) TerminateJobObject(static_cast<HANDLE>(jobHandle), 1);
    else process.kill();
#else
    process.terminate();
#endif
    const auto pid = process.processId();
    QTimer::singleShot(1500, this, [this, pid] { if (cancelled && busy() && process.processId() == pid) process.kill(); });
}
