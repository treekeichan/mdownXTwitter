#include "mainwindow.h"
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFrame>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QCloseEvent>
#include <QTimer>
#include <QSet>
#include <algorithm>

namespace {
QString findTool(const QString &name) {
    const QString base = QCoreApplication::applicationDirPath();
    for (const auto &dir : {base + QStringLiteral("/tools"), base + QStringLiteral("/../tools"), base}) {
        const QString path = QDir(dir).absoluteFilePath(name);
        if (QFileInfo::exists(path)) return QDir::cleanPath(path);
    }
    return QStandardPaths::findExecutable(name);
}
QString historyPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/history.json");
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), engine(this) {
    setWindowTitle(tr("mdownXTwitter 视频下载器")); resize(1120, 780); setMinimumSize(960, 680);
    setObjectName(QStringLiteral("downloadWindow"));
    setStyleSheet(QStringLiteral(R"(
        QMainWindow#downloadWindow { background: #f3f6fa; }
        QLabel { color: #1e293b; }
        QLabel#subtitle, QLabel#queueHint { color: #64748b; }
        QFrame#inputPanel, QFrame#queuePanel {
            background: #ffffff; border: 1px solid #e0e6ee; border-radius: 10px;
        }
        QPushButton {
            background: #ffffff; color: #334155; border: 1px solid #cdd6e1;
            border-radius: 6px; padding: 8px 13px; min-height: 20px;
        }
        QPushButton:hover { background: #f0f5fb; border-color: #94a9c2; }
        QPushButton:pressed, QPushButton:checked { background: #e4edf8; }
        QPushButton:focus { border: 1px solid #1968b5; }
        QPushButton#parse, QPushButton#startDownload {
            background: #1968b5; color: #ffffff; border: 1px solid #1968b5;
            padding: 10px 24px; font-weight: 600;
        }
        QPushButton#parse:hover, QPushButton#startDownload:hover { background: #155a9d; border-color: #155a9d; }
        QPushButton#parse:pressed, QPushButton#startDownload:pressed { background: #104a83; }
        QPushButton#parse:focus, QPushButton#startDownload:focus { border: 1px solid #0c3764; }
        QPushButton:disabled { background: #eef2f6; color: #94a3b8; border-color: #e2e8f0; }
        QPlainTextEdit, QLineEdit, QComboBox {
            background: #ffffff; color: #1e293b; border: 1px solid #d2dce8;
            border-radius: 6px; padding: 7px 10px; selection-background-color: #dbeafe; selection-color: #153c68;
        }
        QPlainTextEdit:focus, QLineEdit:focus, QComboBox:focus { border: 1px solid #1968b5; }
        QComboBox { padding-right: 22px; }
        QComboBox::drop-down { border: none; width: 22px; }
        QComboBox QAbstractItemView { background: #ffffff; color: #1e293b; selection-background-color: #dbeafe; }
        QGroupBox { color: #475569; border: 1px solid #e0e6ee; border-radius: 6px; margin-top: 10px; padding-top: 12px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }
        QTableWidget {
            background: #ffffff; alternate-background-color: #f8fafc; color: #1e293b;
            border: none; gridline-color: #edf1f6; selection-background-color: #e5f0fd; selection-color: #153c68;
        }
        QTableWidget::item { padding: 6px 10px; border-bottom: 1px solid #edf1f6; }
        QTableWidget QComboBox { margin: 5px 6px; padding: 4px 8px; padding-right: 22px; }
        QHeaderView::section { background: #f1f5f9; color: #475569; border: none; border-bottom: 1px solid #e0e6ee; padding: 10px; font-weight: 600; }
        QProgressBar { border: none; border-radius: 4px; background: #e8eef5; color: #234769; text-align: center; margin: 10px; }
        QProgressBar::chunk { background: #83b7ee; border-radius: 4px; }
        QLabel#status { color: #64748b; padding: 2px 0; }
    )"));
    auto *central = new QWidget(this); setCentralWidget(central);
    auto *layout = new QVBoxLayout(central); layout->setContentsMargins(24, 22, 24, 16); layout->setSpacing(14);
    auto *title = new QLabel(tr("mdownXTwitter 视频下载器"));
    QFont font = title->font(); font.setPointSize(20); font.setBold(true); title->setFont(font);
    layout->addWidget(title);
    auto *subtitle = new QLabel(tr("粘贴链接，选择清晰度，将视频保存到本地。"));
    subtitle->setObjectName(QStringLiteral("subtitle")); layout->addWidget(subtitle);
    auto *inputPanel = new QFrame; inputPanel->setObjectName(QStringLiteral("inputPanel"));
    auto *inputLayout = new QVBoxLayout(inputPanel); inputLayout->setContentsMargins(18, 16, 18, 16); inputLayout->setSpacing(12);
    inputLayout->addWidget(new QLabel(tr("帖子链接 · 每行一个")));
    links = new QPlainTextEdit; links->setPlaceholderText(QStringLiteral("https://x.com/username/status/1234567890"));
    links->setObjectName(QStringLiteral("links"));
    auto *parse = new QPushButton(tr("解析并添加"));
    parse->setObjectName(QStringLiteral("parse"));
    const int linkRowHeight = 44;
    parse->setFixedHeight(linkRowHeight);
    links->setFixedHeight(linkRowHeight);
    auto *inputRow = new QHBoxLayout; inputRow->setSpacing(14);
    inputRow->addWidget(links, 1); inputRow->addWidget(parse); inputLayout->addLayout(inputRow);
    connect(parse, &QPushButton::clicked, this, &MainWindow::addLinks);

    QSettings settings;
    outputPath = new QLineEdit(settings.value(QStringLiteral("output"),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + QStringLiteral("/X")).toString());
    outputPath->setObjectName(QStringLiteral("outputPath"));
    auto *outputRow = new QHBoxLayout; outputRow->addWidget(new QLabel(tr("保存目录"))); outputRow->addWidget(outputPath);
    auto *browse = new QPushButton(tr("选择目录")); outputRow->addWidget(browse); inputLayout->addLayout(outputRow);
    connect(browse, &QPushButton::clicked, this, [this] {
        auto path = QFileDialog::getExistingDirectory(this, tr("保存目录"), outputPath->text());
        if (!path.isEmpty()) outputPath->setText(path);
    });
    auto *settingsButton = new QPushButton(tr("引擎、代理和登录设置")); settingsButton->setCheckable(true);
    auto *settingsBox = new QGroupBox(tr("本机配置")); auto *form = new QFormLayout(settingsBox);
    enginePath = new QLineEdit(settings.value(QStringLiteral("engine"), findTool(QStringLiteral("yt-dlp.exe"))).toString());
    enginePath->setObjectName(QStringLiteral("enginePath"));
    const QString ffmpeg = findTool(QStringLiteral("ffmpeg.exe"));
    ffmpegPath = new QLineEdit(settings.value(QStringLiteral("ffmpeg"), ffmpeg.isEmpty() ? QString() : QFileInfo(ffmpeg).absolutePath()).toString());
    proxy = new QLineEdit(settings.value(QStringLiteral("proxy")).toString());
    cookies = new QLineEdit(settings.value(QStringLiteral("cookies")).toString());
    proxy->setPlaceholderText(QStringLiteral("http://127.0.0.1:7890"));
    cookies->setPlaceholderText(tr("可选，Netscape 格式 Cookie 文件"));
    const auto addPath = [this, form](const QString &label, QLineEdit *edit, bool directory) {
        auto *row = new QHBoxLayout; row->addWidget(edit); auto *button = new QPushButton(tr("浏览")); row->addWidget(button);
        form->addRow(label, row);
        connect(button, &QPushButton::clicked, this, [this, edit, directory] {
            const auto path = directory ? QFileDialog::getExistingDirectory(this, tr("选择目录"), edit->text())
                : QFileDialog::getOpenFileName(this, tr("选择文件"), edit->text());
            if (!path.isEmpty()) edit->setText(path);
        });
    };
    addPath(tr("yt-dlp.exe"), enginePath, false); addPath(tr("FFmpeg 所在目录"), ffmpegPath, true);
    form->addRow(tr("代理地址"), proxy); addPath(tr("Cookie 文件"), cookies, false);
    settingsBox->hide();
    connect(settingsButton, &QPushButton::toggled, settingsBox, &QWidget::setVisible);
    layout->addWidget(inputPanel);

    auto *queuePanel = new QFrame; queuePanel->setObjectName(QStringLiteral("queuePanel"));
    auto *queueLayout = new QVBoxLayout(queuePanel); queueLayout->setContentsMargins(18, 16, 18, 14); queueLayout->setSpacing(12);
    auto *queueHeader = new QHBoxLayout;
    auto *queueTitle = new QLabel(tr("下载队列"));
    auto queueFont = queueTitle->font(); queueFont.setBold(true); queueTitle->setFont(queueFont);
    queueHeader->addWidget(queueTitle);
    auto *queueHint = new QLabel(tr("选中任务后开始下载，支持 Ctrl / Shift 多选"));
    queueHint->setObjectName(QStringLiteral("queueHint")); queueHeader->addStretch(); queueHeader->addWidget(queueHint);
    queueLayout->addLayout(queueHeader);
    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    auto *startSelected = new QPushButton(tr("开始下载"));
    startSelected->setObjectName(QStringLiteral("startDownload"));
    startSelected->setToolTip(tr("下载选中的就绪任务，可按 Ctrl 或 Shift 多选。"));
    connect(startSelected, &QPushButton::clicked, this, &MainWindow::startSelectedDownloads);
    for (const auto &action : QList<QPair<QString, void (MainWindow::*)()>>{
        {tr("下载所有就绪任务"), &MainWindow::startDownloads}, {tr("取消选中"), &MainWindow::cancelSelected},
        {tr("重试选中"), &MainWindow::retrySelected}, {tr("打开所在目录"), &MainWindow::openSelected}}) {
        auto *button = new QPushButton(action.first); actions->addWidget(button);
        connect(button, &QPushButton::clicked, this, action.second);
    }
    auto *clear = new QPushButton(tr("清空记录"));
    clear->setObjectName(QStringLiteral("clearRecords"));
    clear->setToolTip(tr("清除已完成、失败和已取消的记录，保留待处理任务和本地文件。"));
    actions->addWidget(clear);
    actions->addWidget(settingsButton);
    connect(clear, &QPushButton::clicked, this, &MainWindow::clearRecords);
    actions->addStretch(); actions->addWidget(startSelected); queueLayout->addLayout(actions);
    queueLayout->addWidget(settingsBox);
    table = new QTableWidget(0, 5); table->setHorizontalHeaderLabels({tr("视频"), tr("时长"), tr("清晰度"), tr("状态"), tr("进度")});
    table->setObjectName(QStringLiteral("tasks"));
    table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->verticalHeader()->hide();
    table->setShowGrid(false); table->setAlternatingRowColors(true);
    table->verticalHeader()->setDefaultSectionSize(44);
    table->horizontalHeader()->setHighlightSections(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->setColumnWidth(1, 65); table->setColumnWidth(2, 170); table->setColumnWidth(3, 125); table->setColumnWidth(4, 160);
    queueLayout->addWidget(table, 1); layout->addWidget(queuePanel, 1);
    status = new QLabel(tr("就绪 · 下载队列按顺序执行")); status->setObjectName(QStringLiteral("status"));
    status->setWordWrap(true); layout->addWidget(status);

    connect(&engine, &DownloadEngine::inspected, this, &MainWindow::receiveInfo);
    connect(&engine, &DownloadEngine::saved, this, [this](const QString &path) { if (active >= 0) tasks[active].path = path; });
    connect(&engine, &DownloadEngine::stageChanged, this, [this](const QString &stage) {
        if (active >= 0) table->item(active, 3)->setText(stage);
    });
    connect(&engine, &DownloadEngine::progress, this, [this](double percent, const QString &speed, const QString &eta) {
        if (active < 0) return;
        auto *bar = qobject_cast<QProgressBar *>(table->cellWidget(active, 4));
        bar->setRange(0, percent < 0 ? 0 : 100); bar->setValue(qRound(percent));
        status->setText(tr("下载中 · %1 %2").arg(speed, eta));
    });
    connect(&engine, &DownloadEngine::finished, this, [this](bool success, bool cancelled, const QString &message) {
        if (active >= 0) {
            auto &task = tasks[active];
            if (!success) { task.state = cancelled ? Cancelled : Failed; task.error = message; }
            else if (task.state == Downloading) task.state = Complete;
            else if (task.state == Parsing) { task.state = Failed; task.error = tr("未找到视频。"); }
            updateRow(active); saveHistory();
            status->setText(message.isEmpty() ? tr("任务完成") : message);
        }
        active = -1;
        if (closing) { QTimer::singleShot(0, this, &QWidget::close); return; }
        QTimer::singleShot(0, this, &MainWindow::schedule);
    });
    loadHistory();
}

EngineOptions MainWindow::options() const {
    return {enginePath->text().trimmed(), ffmpegPath->text().trimmed(), outputPath->text().trimmed(),
        proxy->text().trimmed(), cookies->text().trimmed()};
}

bool MainWindow::validateOptions(bool downloading) {
    const auto o = options();
    QString error;
    if (!QFileInfo(o.executable).isFile()) error = tr("请在设置中指定有效的 yt-dlp.exe。可先运行 scripts/setup-tools.ps1。");
    else if (!o.cookies.isEmpty() && !QFileInfo(o.cookies).isReadable()) error = tr("Cookie 文件不存在或不可读取。");
    else if (!o.ffmpegDirectory.isEmpty() && !QFileInfo(QDir(o.ffmpegDirectory).filePath(QStringLiteral("ffmpeg.exe"))).isFile())
        error = tr("FFmpeg 目录中没有 ffmpeg.exe。");
    else if (!o.proxy.isEmpty()) {
        const QUrl url(o.proxy);
        if (!url.isValid() || url.host().isEmpty() || !QStringList{QStringLiteral("http"), QStringLiteral("https"), QStringLiteral("socks5"), QStringLiteral("socks5h")}.contains(url.scheme()))
            error = tr("代理地址需包含有效的协议和主机，例如 http://127.0.0.1:7890。");
    }
    if (error.isEmpty() && downloading && (o.outputDirectory.isEmpty() || !QDir().mkpath(o.outputDirectory)))
        error = tr("无法创建保存目录。");
    if (!error.isEmpty()) { QMessageBox::warning(this, tr("检查配置"), error); return false; }
    saveSettings(); return true;
}

void MainWindow::addLinks() {
    if (!validateOptions(false)) return;
    const auto input = links->toPlainText().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    const QRegularExpression pattern(QStringLiteral("^https?://(?:www\\.|mobile\\.|m\\.)?(?:x|twitter)\\.com/(?:[A-Za-z0-9_]+/status|i/web/status)/(\\d+)(?:[/?#].*)?$"), QRegularExpression::CaseInsensitiveOption);
    QStringList urls;
    for (const auto &value : input) {
        const auto match = pattern.match(value);
        if (!match.hasMatch()) { QMessageBox::warning(this, tr("链接格式"), tr("请输入完整的 X/Twitter 帖子链接，包含 status/帖子编号。")); return; }
        const QString canonical = QStringLiteral("https://x.com/i/web/status/") + match.captured(1);
        if (!urls.contains(canonical)) urls << canonical;
    }
    if (urls.isEmpty()) { status->setText(tr("请先输入帖子链接。")); return; }
    int added = 0;
    for (const auto &url : urls) {
        bool exists = false;
        for (const auto &task : tasks) if (task.url == url && task.state != Cancelled && task.state != Failed &&
            (task.state != Complete || QFileInfo::exists(task.path))) { exists = true; break; }
        if (exists) continue;
        Task task; task.url = url; task.title = url; tasks.append(task); updateRow(tasks.size() - 1); ++added;
    }
    links->clear(); status->setText(tr("添加 %1 个帖子，重复链接已跳过。").arg(added)); schedule();
}

void MainWindow::schedule() {
    if (active >= 0 || engine.busy() || closing) return;
    for (int i = 0; i < tasks.size(); ++i) {
        if (tasks[i].state != WaitingParse && tasks[i].state != WaitingDownload) continue;
        const bool parse = tasks[i].state == WaitingParse;
        if (!validateOptions(!parse)) { status->setText(tr("队列等待配置修正，修正后请重试。")); return; }
        active = i; tasks[i].state = parse ? Parsing : Downloading; tasks[i].error.clear(); updateRow(i);
        if (parse) engine.inspect(tasks[i].url, options());
        else {
            auto *quality = qobject_cast<QComboBox *>(table->cellWidget(i, 2));
            engine.download(tasks[i].url, quality->currentData().toString(), options(), tasks[i].index);
        }
        return;
    }
}

void MainWindow::receiveInfo(const QJsonObject &info) {
    if (active < 0) return;
    QJsonArray entries = info.value(QStringLiteral("entries")).toArray();
    const bool playlist = !entries.isEmpty();
    if (!playlist) entries.append(info);
    bool first = true;
    for (int i = 0; i < entries.size(); ++i) {
        const auto entry = entries[i].toObject();
        if (entry.value(QStringLiteral("formats")).toArray().isEmpty()) continue;
        Task task; task.url = tasks[active].url; task.info = entry; task.state = Ready;
        task.index = playlist ? i + 1 : 0;
        task.title = entry.value(QStringLiteral("title")).toString(tr("未命名视频"));
        if (first) { tasks[active] = task; updateRow(active); first = false; }
        else { tasks.append(task); updateRow(tasks.size() - 1); }
    }
}

void MainWindow::addFormats(int row) {
    auto *combo = new QComboBox;
    combo->addItem(tr("最佳可用（优先 MP4）"), QStringLiteral("b[ext=mp4]/b/bv*+ba/b"));
    combo->addItem(tr("最高 1080p"), QStringLiteral("b[height<=1080][ext=mp4]/b[height<=1080]/bv*[height<=1080]+ba/b[height<=1080]"));
    combo->addItem(tr("最高 720p"), QStringLiteral("b[height<=720][ext=mp4]/b[height<=720]/bv*[height<=720]+ba/b[height<=720]"));
    auto formats = tasks[row].info.value(QStringLiteral("formats")).toArray();
    for (const auto &value : formats) {
        const auto f = value.toObject();
        if (f.value(QStringLiteral("vcodec")).toString() == QStringLiteral("none")) continue;
        if (f.value(QStringLiteral("acodec")).toString() == QStringLiteral("none")) continue;
        const QString id = f.value(QStringLiteral("format_id")).toString();
        if (id.isEmpty()) continue;
        combo->addItem(tr("%1p · %2 · %3").arg(f.value(QStringLiteral("height")).toInt()).arg(f.value(QStringLiteral("ext")).toString(), id), id);
    }
    table->setCellWidget(row, 2, combo);
}

void MainWindow::updateRow(int row) {
    while (table->rowCount() <= row) table->insertRow(table->rowCount());
    const auto &task = tasks[row];
    const QStringList labels{tr("等待解析"), tr("解析中"), tr("就绪"), tr("等待下载"), tr("下载中"), tr("已完成"), tr("失败"), tr("已取消")};
    const int seconds = task.info.value(QStringLiteral("duration")).toInt();
    const QStringList values{task.title, seconds > 0 ? QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0')) : QStringLiteral("—"), QString(), labels[task.state]};
    for (int col : {0, 1, 3}) {
        if (!table->item(row, col)) table->setItem(row, col, new QTableWidgetItem);
        table->item(row, col)->setText(values[col]);
    }
    table->item(row, 0)->setToolTip(task.url);
    table->item(row, 3)->setToolTip(task.error);
    if (!table->cellWidget(row, 2) || task.state == Ready) addFormats(row);
    table->cellWidget(row, 2)->setEnabled(task.state == Ready);
    auto *bar = qobject_cast<QProgressBar *>(table->cellWidget(row, 4));
    if (!bar) { bar = new QProgressBar; table->setCellWidget(row, 4, bar); }
    bar->setRange(0, task.state == Parsing ? 0 : 100); bar->setValue(task.state == Complete ? 100 : 0);
}

void MainWindow::startDownloads() {
    if (!validateOptions(true)) return;
    for (int i = 0; i < tasks.size(); ++i) if (tasks[i].state == Ready) { tasks[i].state = WaitingDownload; updateRow(i); }
    schedule();
}

void MainWindow::startSelectedDownloads() {
    QList<int> rows;
    for (const auto &index : table->selectionModel()->selectedRows())
        if (tasks[index.row()].state == Ready) rows.append(index.row());
    if (rows.isEmpty()) {
        status->setText(tr("请先选中需要下载的就绪任务。"));
        return;
    }
    if (!validateOptions(true)) return;
    for (int row : rows) { tasks[row].state = WaitingDownload; updateRow(row); }
    schedule();
}

void MainWindow::clearRecords() {
    int removed = 0;
    // Remove backwards to preserve remaining rows' format selections and progress.
    // Remap the active index so asynchronous engine callbacks still reach its row.
    for (int row = tasks.size() - 1; row >= 0; --row) {
        if (row == active) continue;
        const auto state = tasks[row].state;
        if (state != Complete && state != Failed && state != Cancelled) continue;
        tasks.removeAt(row);
        table->removeRow(row);
        if (active > row) --active;
        ++removed;
    }
    saveHistory();
    status->setText(removed > 0 ? tr("已清除 %1 条记录，本地视频文件已保留。").arg(removed)
                               : tr("没有可清除的记录，待处理任务已保留。"));
}

void MainWindow::cancelSelected() {
    const auto rows = table->selectionModel()->selectedRows();
    for (const auto &index : rows) {
        const int row = index.row();
        if (row == active) engine.cancel();
        else if (tasks[row].state != Complete) { tasks[row].state = Cancelled; updateRow(row); }
    }
    saveHistory();
}

void MainWindow::retrySelected() {
    for (const auto &index : table->selectionModel()->selectedRows()) {
        auto &task = tasks[index.row()];
        if (task.state != Failed && task.state != Cancelled) continue;
        task.state = task.info.isEmpty() ? WaitingParse : Ready; task.error.clear(); updateRow(index.row());
    }
    schedule();
}

void MainWindow::openSelected() {
    const auto rows = table->selectionModel()->selectedRows();
    QString path = outputPath->text();
    if (!rows.isEmpty() && !tasks[rows.first().row()].path.isEmpty()) path = QFileInfo(tasks[rows.first().row()].path).absolutePath();
    if (QDir(path).exists()) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::saveSettings() {
    QSettings settings;
    settings.setValue(QStringLiteral("output"), outputPath->text()); settings.setValue(QStringLiteral("engine"), enginePath->text());
    settings.setValue(QStringLiteral("ffmpeg"), ffmpegPath->text()); settings.setValue(QStringLiteral("cookies"), cookies->text());
    // Do not persist proxy credentials.
    QUrl proxyUrl(proxy->text()); proxyUrl.setUserInfo(QString());
    settings.setValue(QStringLiteral("proxy"), proxyUrl.toString());
}

void MainWindow::saveHistory() {
    QJsonArray history;
    for (const auto &task : tasks) if (task.state == Complete) history.append(QJsonObject{
        {QStringLiteral("url"), task.url}, {QStringLiteral("title"), task.title}, {QStringLiteral("path"), task.path}});
    while (history.size() > 200) history.removeFirst();
    QSaveFile file(historyPath());
    if (file.open(QIODevice::WriteOnly)) { file.write(QJsonDocument(history).toJson()); file.commit(); }
}

void MainWindow::loadHistory() {
    QFile file(historyPath()); if (!file.open(QIODevice::ReadOnly)) return;
    for (const auto &value : QJsonDocument::fromJson(file.readAll()).array()) {
        const auto item = value.toObject(); Task task;
        task.url = item.value(QStringLiteral("url")).toString(); task.title = item.value(QStringLiteral("title")).toString();
        task.path = item.value(QStringLiteral("path")).toString(); task.state = Complete;
        tasks.append(task); updateRow(tasks.size() - 1);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveSettings(); saveHistory();
    if (engine.busy()) { closing = true; event->ignore(); engine.cancel(); }
    else event->accept();
}
