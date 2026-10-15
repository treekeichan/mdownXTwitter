#include <QtTest>
#include "downloadengine.h"
class EngineTests : public QObject {
    Q_OBJECT
    EngineOptions options() const {
        EngineOptions o; o.executable = qEnvironmentVariable("MDOW_TEST_ENGINE"); o.outputDirectory = QDir::tempPath(); return o;
    }
private slots:
    void realDownload() {
        const QString url = qEnvironmentVariable("MDOW_INTEGRATION_URL");
        if (url.isEmpty()) QSKIP("Set MDOW_INTEGRATION_URL to opt into a real download");
        EngineOptions o;
        const QString tools = qEnvironmentVariable("MDOW_REAL_TOOLS");
        o.executable = tools + QStringLiteral("/yt-dlp.exe"); o.ffmpegDirectory = tools;
        o.outputDirectory = qEnvironmentVariable("MDOW_REAL_OUTPUT");
        QVERIFY(!o.outputDirectory.isEmpty()); QVERIFY(QDir().mkpath(o.outputDirectory));
        DownloadEngine engine; QSignalSpy info(&engine, &DownloadEngine::inspected), done(&engine, &DownloadEngine::finished), saved(&engine, &DownloadEngine::saved);
        engine.inspect(url, o); QVERIFY(done.wait(45000));
        QVERIFY2(done.first()[0].toBool(), qPrintable(done.first()[2].toString())); QCOMPARE(info.size(), 1);
        done.clear(); engine.download(url, QStringLiteral("b[height<=360]/b"), o);
        QVERIFY(done.wait(60000)); QVERIFY2(done.first()[0].toBool(), qPrintable(done.first()[2].toString()));
        QCOMPARE(saved.size(), 1); const QFileInfo file(saved.first()[0].toString());
        QVERIFY(file.exists()); QVERIFY(file.size() > 0); qInfo() << "Downloaded bytes:" << file.size();
    }
    void metadata() {
        DownloadEngine engine; QSignalSpy data(&engine, &DownloadEngine::inspected), done(&engine, &DownloadEngine::finished);
        engine.inspect(QStringLiteral("https://x.com/ok"), options());
        QVERIFY(done.wait(5000)); QCOMPARE(data.size(), 1); QVERIFY(done.first()[0].toBool());
        QCOMPARE(qvariant_cast<QJsonObject>(data.first()[0]).value(QStringLiteral("duration")).toInt(), 42);
    }
    void downloadProgressAndUnicodePath() {
        DownloadEngine engine; QSignalSpy progress(&engine, &DownloadEngine::progress), saved(&engine, &DownloadEngine::saved), done(&engine, &DownloadEngine::finished);
        engine.download(QStringLiteral("https://x.com/ok"), QStringLiteral("b"), options());
        QVERIFY(done.wait(5000)); QVERIFY(done.first()[0].toBool()); QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first()[0].toString(), QStringLiteral("E:/测试/video.mp4"));
        QCOMPARE(progress.size(), 1); QCOMPARE(progress.first()[0].toDouble(), 50.0);
    }
    void invalidJson() {
        DownloadEngine engine; QSignalSpy done(&engine, &DownloadEngine::finished);
        engine.inspect(QStringLiteral("https://x.com/invalid"), options());
        QVERIFY(done.wait(5000)); QVERIFY(!done.first()[0].toBool());
    }
    void rateLimited() {
        DownloadEngine engine; QSignalSpy done(&engine, &DownloadEngine::finished);
        engine.inspect(QStringLiteral("https://x.com/failure"), options());
        QVERIFY(done.wait(5000)); QVERIFY(!done.first()[0].toBool()); QVERIFY(done.first()[2].toString().contains(QStringLiteral("频繁")));
    }
    void failedStart() {
        DownloadEngine engine; QSignalSpy done(&engine, &DownloadEngine::finished);
        auto o = options(); o.executable = QStringLiteral("Z:/nonexistent-mdow.exe");
        engine.inspect(QStringLiteral("https://x.com/ok"), o);
        if (done.isEmpty()) QVERIFY(done.wait(5000));
        QCOMPARE(done.size(), 1); QVERIFY(!done.first()[0].toBool());
    }
    void cancelAndReuse() {
        DownloadEngine engine; QSignalSpy done(&engine, &DownloadEngine::finished);
        engine.inspect(QStringLiteral("https://x.com/slow"), options());
        QTest::qWait(200); engine.cancel();
        if (done.isEmpty()) QVERIFY(done.wait(5000));
        QVERIFY(done.first()[1].toBool()); QVERIFY(!engine.busy());
        done.clear(); engine.inspect(QStringLiteral("https://x.com/ok"), options());
        QVERIFY(done.wait(5000)); QVERIFY(done.first()[0].toBool());
    }
};
QTEST_GUILESS_MAIN(EngineTests)
#include "enginetests.moc"
