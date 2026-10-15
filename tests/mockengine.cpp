#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    const auto url = args.last();
    QTextStream out(stdout), err(stderr);
    if (url.endsWith(QStringLiteral("slow"))) { QTimer::singleShot(20000, &app, &QCoreApplication::quit); return app.exec(); }
    if (url.endsWith(QStringLiteral("failure"))) { err << "ERROR: 429 rate limited\n"; err.flush(); return 1; }
    if (url.endsWith(QStringLiteral("invalid"))) { out << "not-json"; out.flush(); return 0; }
    if (args.contains(QStringLiteral("--dump-single-json"))) {
        auto info = QJsonDocument::fromJson(R"({"id":"123","title":"test video","duration":42,"formats":[{"format_id":"mp4","ext":"mp4","height":720,"vcodec":"h264","acodec":"aac"}]})").object();
        if (url.endsWith(QStringLiteral("222"))) {
            auto second = info; second.insert(QStringLiteral("id"), QStringLiteral("124")); second.insert(QStringLiteral("title"), QStringLiteral("second video"));
            info = QJsonObject{{QStringLiteral("entries"), QJsonArray{info, second}}};
        }
        out << QJsonDocument(info).toJson(QJsonDocument::Compact);
    } else {
        // Exercise progress arriving on stderr and the final line lacking a newline.
        err << "MDOW_PROGRESS:{\"downloaded_bytes\":500,\"total_bytes\":1000,\"speed\":1048576,\"eta\":2}\n"; err.flush();
        out << "MDOW_FILE:" << QJsonDocument(QJsonArray{QStringLiteral("E:/测试/video.mp4")}).toJson(QJsonDocument::Compact).mid(1).chopped(1);
    }
    out.flush(); return 0;
}
