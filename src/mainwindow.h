#pragma once
#include <QMainWindow>
#include <QJsonObject>
#include "downloadengine.h"
class QPlainTextEdit;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QLabel;
class QCloseEvent;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    enum State { WaitingParse, Parsing, Ready, WaitingDownload, Downloading, Complete, Failed, Cancelled };
    struct Task {
        QString url, title, path, error;
        QJsonObject info;
        State state = WaitingParse;
        int index = 0;
    };
    DownloadEngine engine;
    QVector<Task> tasks;
    int active = -1;
    bool closing = false;
    QPlainTextEdit *links;
    QLineEdit *outputPath, *enginePath, *ffmpegPath, *proxy, *cookies;
    QTableWidget *table;
    QLabel *status;
    EngineOptions options() const;
    void addLinks();
    void schedule();
    void receiveInfo(const QJsonObject &info);
    void addFormats(int row);
    void updateRow(int row);
    void startDownloads();
    void startSelectedDownloads();
    void clearRecords();
    void cancelSelected();
    void retrySelected();
    void openSelected();
    void saveSettings();
    void loadHistory();
    void saveHistory();
    bool validateOptions(bool downloading);
};
