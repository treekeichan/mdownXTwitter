#include "mainwindow.h"
#include <QApplication>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QFont>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    QCoreApplication::setOrganizationName(QStringLiteral("Mdow"));
    QCoreApplication::setApplicationName(QStringLiteral("XDownloader"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    if (app.arguments().contains(QStringLiteral("--smoke-test")))
        QCoreApplication::setApplicationName(QStringLiteral("XDownloaderSmokeTest"));
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
