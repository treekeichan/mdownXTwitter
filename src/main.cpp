#include "mainwindow.h"
#include <QApplication>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QFont>
#include <QSettings>
#include <QStandardPaths>
#include <QFileInfo>
#include <QSaveFile>
#include <QDir>

static void migrateLegacyData() {
    QSettings current;
    current.sync();
    if (current.allKeys().isEmpty()) {
        QSettings legacy(QStringLiteral("Mdow"), QStringLiteral("XDownloader"));
        for (const auto &key : legacy.allKeys()) current.setValue(key, legacy.value(key));
        current.sync();
    }

    const QString currentDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString currentHistory = QDir(currentDirectory).filePath(QStringLiteral("history.json"));
    if (QFileInfo::exists(currentHistory)) return;

    const QString organization = QCoreApplication::organizationName();
    const QString application = QCoreApplication::applicationName();
    QCoreApplication::setOrganizationName(QStringLiteral("Mdow"));
    QCoreApplication::setApplicationName(QStringLiteral("XDownloader"));
    const QString legacyHistory = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                                      .filePath(QStringLiteral("history.json"));
    QCoreApplication::setOrganizationName(organization);
    QCoreApplication::setApplicationName(application);

    QFile source(legacyHistory);
    if (!source.open(QIODevice::ReadOnly)) return;
    QDir().mkpath(currentDirectory);
    QSaveFile destination(currentHistory);
    if (destination.open(QIODevice::WriteOnly)) {
        destination.write(source.readAll());
        destination.commit();
    }
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    QCoreApplication::setOrganizationName(QStringLiteral("mdownXTwitter"));
    QCoreApplication::setApplicationName(QStringLiteral("mdownXTwitter"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    if (app.arguments().contains(QStringLiteral("--smoke-test")))
        QCoreApplication::setApplicationName(QStringLiteral("mdownXTwitterSmokeTest"));
    else
        migrateLegacyData();
    MainWindow window;
    window.show();
    if (app.arguments().contains(QStringLiteral("--smoke-test"))) {
        QTimer::singleShot(200, &app, [&] {
            window.grab().save(QCoreApplication::applicationDirPath() + QStringLiteral("/smoke-test.png"));
            QFile result(QCoreApplication::applicationDirPath() + QStringLiteral("/smoke-test.txt"));
            if (result.open(QIODevice::WriteOnly)) {
                QTextStream stream(&result);
                stream << "Qt Widgets startup OK; Qt " << qVersion() << '\n';
            }
            app.quit();
        });
    }
    return app.exec();
}
