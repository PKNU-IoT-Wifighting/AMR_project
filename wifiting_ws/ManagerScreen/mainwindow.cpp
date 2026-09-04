#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QImage>
#include <QHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QKeyEvent>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QPushButton>
#include <QResizeEvent>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kVideoWidth = 640;
constexpr int kVideoHeight = 360;
constexpr int kBytesPerPixel = 3;
constexpr qsizetype kFrameSize =
    kVideoWidth * kVideoHeight * kBytesPerPixel;
const QString kServerBaseUrl = QStringLiteral("http://127.0.0.1:8080");
const QString kVelocityTopic = QStringLiteral("/cmd_vel_watchdog_input");
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , cameraProcess(new QProcess(this))
    , velocityPublisher(new QProcess(this))
    , robotStatusMonitor(new QProcess(this))
    , networkManager(new QNetworkAccessManager(this))
{
    ui->setupUi(this);

    createHistoryPage();
    connect(ui->logViewButton, &QPushButton::clicked,
            this, &MainWindow::showHistoryPage);

    // Keep the most important state first: robot, speed, then destination.
    ui->robotStatusPanelLayout->removeWidget(ui->robotConnectionStatusCard);
    ui->robotStatusPanelLayout->insertWidget(2, ui->robotConnectionStatusCard);

    // Use the whole dark viewport for the incoming 16:9 video.
    ui->cameraHintLabel->hide();
    ui->cameraSourceLabel->hide();
    ui->cameraViewportLayout->setContentsMargins(2, 2, 2, 2);
    ui->cameraViewportLayout->setSpacing(0);
    ui->cameraTopSpacer->changeSize(0, 0, QSizePolicy::Minimum,
                                    QSizePolicy::Fixed);
    ui->cameraBottomSpacer->changeSize(0, 0, QSizePolicy::Minimum,
                                       QSizePolicy::Fixed);
    ui->cameraViewLabel->setSizePolicy(QSizePolicy::Expanding,
                                       QSizePolicy::Expanding);
    ui->cameraViewLabel->setText(tr("UDP 카메라 스트림 연결 중..."));

    connect(ui->manualMoveButton, &QPushButton::toggled,
            this, &MainWindow::sendManualMode);

    startRobotStatusMonitor();

    setServerConnected(false);
    checkServerConnection();
    auto *serverCheckTimer = new QTimer(this);
    serverCheckTimer->setInterval(5000);
    connect(serverCheckTimer, &QTimer::timeout,
            this, &MainWindow::checkServerConnection);
    serverCheckTimer->start();

    cameraProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(cameraProcess, &QProcess::readyReadStandardOutput,
            this, &MainWindow::readCameraFrames);
    connect(cameraProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError) {
                setCameraStatus(tr("GStreamer를 실행할 수 없습니다"), false);
            });
    connect(cameraProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                const QString error =
                    QString::fromLocal8Bit(cameraProcess->readAllStandardError());
                if (error.contains(QStringLiteral("bind"), Qt::CaseInsensitive)
                    || error.contains(QStringLiteral("Address already in use"),
                                      Qt::CaseInsensitive)) {
                    setCameraStatus(
                        tr("UDP 5000 포트를 다른 프로그램이 사용 중입니다"), false);
                } else if (cameraProcess->state() == QProcess::NotRunning) {
                    setCameraStatus(
                        tr("카메라 스트림 연결이 종료되었습니다"), false);
                }
            });

    QTimer::singleShot(0, this, &MainWindow::startCameraStream);
}

MainWindow::~MainWindow()
{
    robotStatusMonitor->terminate();
    if (!robotStatusMonitor->waitForFinished(500))
        robotStatusMonitor->kill();

    velocityPublisher->terminate();
    if (!velocityPublisher->waitForFinished(500))
        velocityPublisher->kill();

    cameraProcess->terminate();
    if (!cameraProcess->waitForFinished(1000)) {
        cameraProcess->kill();
        cameraProcess->waitForFinished(1000);
    }
    delete ui;
}

void MainWindow::startRobotStatusMonitor()
{
    ui->robotConnectionValueLabel->setText(tr("연결 안 됨"));
    ui->robotConnectionValueLabel->setStyleSheet(
        QStringLiteral("color: #A56816;"));
    ui->destinationValueLabel->setText(tr("선택 안 됨"));
    ui->speedValueLabel->setText(tr("0.00 m/s"));

    robotStatusMonitor->setProcessChannelMode(QProcess::SeparateChannels);
    connect(robotStatusMonitor, &QProcess::readyReadStandardOutput,
            this, &MainWindow::readRobotStatusEvents);
    connect(robotStatusMonitor,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                ui->robotConnectionValueLabel->setText(tr("연결 안 됨"));
                ui->robotConnectionValueLabel->setStyleSheet(
                    QStringLiteral("color: #A56816;"));
                ui->speedValueLabel->setText(tr("0.00 m/s"));
            });

    const QString helper = QCoreApplication::applicationDirPath()
                           + QStringLiteral("/ros_status_monitor.py");
    robotStatusMonitor->start(QStringLiteral("/usr/bin/python3"), {helper});
}

void MainWindow::readRobotStatusEvents()
{
    while (robotStatusMonitor->canReadLine()) {
        const QByteArray line = robotStatusMonitor->readLine().trimmed();
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
            continue;

        const QJsonObject event = document.object();
        if (event.contains(QStringLiteral("robot_connected"))) {
            const bool connected =
                event.value(QStringLiteral("robot_connected")).toBool();
            ui->robotConnectionValueLabel->setText(
                connected ? tr("연결됨") : tr("연결 안 됨"));
            ui->robotConnectionValueLabel->setStyleSheet(
                connected ? QStringLiteral("color: #247A5B;")
                          : QStringLiteral("color: #A56816;"));
        }

        if (event.contains(QStringLiteral("destination"))) {
            setDestination(
                event.value(QStringLiteral("destination")).toString());
        }

        const QJsonValue speed = event.value(QStringLiteral("speed_mps"));
        if (speed.isDouble()) {
            ui->speedValueLabel->setText(
                tr("%1 m/s").arg(speed.toDouble(), 0, 'f', 2));
        }
    }
}

void MainWindow::setDestination(const QString &destinationId)
{
    static const QHash<QString, QString> destinationNames{
        {QStringLiteral("restroom"), tr("화장실 앞")},
        {QStringLiteral("room_301"), tr("강의실 301호")},
        {QStringLiteral("room_302"), tr("강의실 302호")},
        {QStringLiteral("elevator"), tr("엘리베이터 앞")}};
    ui->destinationValueLabel->setText(
        destinationNames.value(
            destinationId,
            destinationId.isEmpty() ? tr("선택 안 됨") : destinationId));
}

void MainWindow::createHistoryPage()
{
    historyPage = new QWidget(ui->centralwidget);
    historyPage->setObjectName(QStringLiteral("historyPage"));
    historyPage->setGeometry(ui->centralwidget->rect());
    historyPage->setStyleSheet(QStringLiteral(R"(
QWidget#historyPage { background-color: #F3F6FA; color: #172033; }
QFrame#historyHero { border: none; border-radius: 20px;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #17396F, stop:0.58 #245EC7, stop:1 #3989E8); }
QLabel#historyEyebrow { font-size: 12px; font-weight: 700; color: #BFD7FF; letter-spacing: 1px; }
QLabel#historyTitle { font-size: 28px; font-weight: 700; color: #FFFFFF; }
QLabel#historySubtitle { font-size: 13px; color: #D9E7FF; }
QLabel#historySummary { font-size: 13px; font-weight: 600; color: #778198; }
QLabel#historyEmpty { font-size: 16px; font-weight: 600; color: #778198; }
QPushButton#historyBackButton, QPushButton#historyRefreshButton {
    min-height: 40px; max-height: 40px; padding: 0 18px;
    border-radius: 10px; font-size: 14px; font-weight: 700;
}
QPushButton#historyBackButton { background: rgba(255,255,255,0.14); color: #FFFFFF; border: 1px solid rgba(255,255,255,0.32); }
QPushButton#historyBackButton:hover { background: rgba(255,255,255,0.24); }
QPushButton#historyRefreshButton { background: #FFFFFF; color: #245EC7; border: 1px solid #FFFFFF; }
QPushButton#historyRefreshButton:hover { background: #EAF2FF; }
QFrame.historyStatCard { background: #FFFFFF; border: 1px solid #E0E7F1; border-radius: 15px; }
QLabel.historyStatIcon { border-radius: 18px; min-width: 36px; max-width: 36px;
    min-height: 36px; max-height: 36px; font-size: 17px; qproperty-alignment: AlignCenter; }
QLabel.historyStatLabel { color: #8590A4; font-size: 12px; font-weight: 600; }
QLabel.historyStatValue { color: #1D2940; font-size: 21px; font-weight: 700; }
QFrame#historyTableCard { background: #FFFFFF; border: 1px solid #E0E7F1; border-radius: 17px; }
QLabel#historyListTitle { color: #1D2940; font-size: 17px; font-weight: 700; }
QTableWidget { background: #FFFFFF; alternate-background-color: #F8FAFD;
    border: none; gridline-color: #E8EDF4;
    selection-background-color: #E8F1FF; selection-color: #172033; font-size: 14px; }
QHeaderView::section { background: #EFF4FA; color: #59657A; border: none;
    border-bottom: 1px solid #DCE5F1; padding: 12px 10px; font-size: 13px; font-weight: 700; }
QTableWidget::item { padding: 9px; border-bottom: 1px solid #EEF2F7; }
)"));

    auto *root = new QVBoxLayout(historyPage);
    root->setContentsMargins(34, 28, 34, 30);
    root->setSpacing(18);

    auto *hero = new QFrame;
    hero->setObjectName(QStringLiteral("historyHero"));
    hero->setMinimumHeight(132);
    auto *header = new QHBoxLayout(hero);
    header->setContentsMargins(26, 20, 22, 20);
    auto *titles = new QVBoxLayout;
    auto *eyebrow = new QLabel(tr("GUIDEROBOT  ·  ACTIVITY"));
    eyebrow->setObjectName(QStringLiteral("historyEyebrow"));
    auto *title = new QLabel(tr("운행 기록"));
    title->setObjectName(QStringLiteral("historyTitle"));
    auto *subtitle = new QLabel(tr("안내 로봇의 최근 목적지 운행 내역을 확인합니다."));
    subtitle->setObjectName(QStringLiteral("historySubtitle"));
    titles->addWidget(eyebrow);
    titles->addWidget(title);
    titles->addWidget(subtitle);
    header->addLayout(titles);
    header->addStretch();

    auto *refreshButton = new QPushButton(tr("↻  새로고침"));
    refreshButton->setObjectName(QStringLiteral("historyRefreshButton"));
    refreshButton->setCursor(Qt::PointingHandCursor);
    auto *backButton = new QPushButton(tr("←  관제 화면"));
    backButton->setObjectName(QStringLiteral("historyBackButton"));
    backButton->setCursor(Qt::PointingHandCursor);
    header->addWidget(refreshButton);
    header->addWidget(backButton);
    root->addWidget(hero);

    auto createStatCard = [this](const QString &icon, const QString &caption,
                                 const QString &iconStyle, QLabel **valueLabel) {
        auto *card = new QFrame;
        card->setProperty("class", "historyStatCard");
        card->setMinimumHeight(82);
        auto *layout = new QHBoxLayout(card);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);
        auto *iconLabel = new QLabel(icon);
        iconLabel->setProperty("class", "historyStatIcon");
        iconLabel->setStyleSheet(iconStyle);
        auto *text = new QVBoxLayout;
        text->setSpacing(1);
        auto *captionLabel = new QLabel(caption);
        captionLabel->setProperty("class", "historyStatLabel");
        *valueLabel = new QLabel(tr("—"));
        (*valueLabel)->setProperty("class", "historyStatValue");
        text->addWidget(captionLabel);
        text->addWidget(*valueLabel);
        layout->addWidget(iconLabel);
        layout->addLayout(text, 1);
        return card;
    };

    auto *stats = new QHBoxLayout;
    stats->setSpacing(12);
    stats->addWidget(createStatCard(tr("▤"), tr("전체 기록"),
        QStringLiteral("background:#EAF2FF;color:#245EC7;"), &historyTotalValue));
    stats->addWidget(createStatCard(tr("✓"), tr("목적지 안내 완료"),
        QStringLiteral("background:#E9F8F1;color:#247A5B;"), &historyArrivedValue));
    stats->addWidget(createStatCard(tr("■"), tr("안내 취소"),
        QStringLiteral("background:#FFF0E8;color:#B45B25;"), &historyCanceledValue));
    stats->addWidget(createStatCard(tr("!"), tr("안내 실패"),
        QStringLiteral("background:#FDEBEC;color:#B43D48;"), &historyFailedValue));
    stats->addWidget(createStatCard(tr("⌖"), tr("자주 찾은 목적지"),
        QStringLiteral("background:#F2EDFF;color:#7252B8;"), &historyPopularValue));
    root->addLayout(stats);

    historySummaryLabel = new QLabel(tr("기록을 불러오는 중입니다."));
    historySummaryLabel->setObjectName(QStringLiteral("historySummary"));

    auto *tableCard = new QFrame;
    tableCard->setObjectName(QStringLiteral("historyTableCard"));
    auto *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(18, 15, 18, 16);
    tableLayout->setSpacing(10);
    auto *listHeader = new QHBoxLayout;
    auto *listTitle = new QLabel(tr("최근 안내 내역"));
    listTitle->setObjectName(QStringLiteral("historyListTitle"));
    listHeader->addWidget(listTitle);
    listHeader->addStretch();
    listHeader->addWidget(historySummaryLabel);
    tableLayout->addLayout(listHeader);

    historyTable = new QTableWidget;
    historyTable->setColumnCount(5);
    historyTable->setHorizontalHeaderLabels(
        {tr("목적지"), tr("출발 시각"), tr("종료 시각"), tr("소요 시간"), tr("상태")});
    historyTable->setAlternatingRowColors(true);
    historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    historyTable->setShowGrid(false);
    historyTable->verticalHeader()->hide();
    historyTable->verticalHeader()->setDefaultSectionSize(58);
    historyTable->verticalHeader()->setMinimumSectionSize(58);
    historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    historyTable->horizontalHeader()->resizeSection(4, 180);
    tableLayout->addWidget(historyTable, 1);

    historyEmptyLabel = new QLabel;
    historyEmptyLabel->setObjectName(QStringLiteral("historyEmpty"));
    historyEmptyLabel->setAlignment(Qt::AlignCenter);
    historyEmptyLabel->hide();
    tableLayout->addWidget(historyEmptyLabel, 1);
    root->addWidget(tableCard, 1);

    connect(backButton, &QPushButton::clicked, this, &MainWindow::hideHistoryPage);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::loadHistory);
    historyPage->hide();
}

void MainWindow::showHistoryPage()
{
    historyPage->setGeometry(ui->centralwidget->rect());
    historyPage->show();
    historyPage->raise();
    loadHistory();
}

void MainWindow::hideHistoryPage()
{
    historyPage->hide();
}

void MainWindow::loadHistory()
{
    historyTable->hide();
    historyEmptyLabel->setText(tr("기록을 불러오는 중입니다..."));
    historyEmptyLabel->show();
    historySummaryLabel->setText(tr("서버의 운행 기록을 확인하고 있습니다."));

    QNetworkRequest request(QUrl(kServerBaseUrl + QStringLiteral("/api/history")));
    request.setTransferTimeout(5000);
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &error);
        const bool succeeded = reply->error() == QNetworkReply::NoError
                               && error.error == QJsonParseError::NoError
                               && document.isObject()
                               && document.object().value(QStringLiteral("records")).isArray();
        if (succeeded) {
            populateHistory(document.object().value(QStringLiteral("records")).toArray());
        } else {
            historyTable->hide();
            historyEmptyLabel->setText(tr("운행 기록을 불러오지 못했습니다.\n서버 연결을 확인한 뒤 새로고침해 주세요."));
            historyEmptyLabel->show();
            historySummaryLabel->setText(tr("기록 조회 실패"));
        }
        reply->deleteLater();
    });
}

void MainWindow::populateHistory(const QJsonArray &records)
{
    historyTable->setRowCount(records.size());
    int arrivedCount = 0;
    int canceledCount = 0;
    int failedCount = 0;
    QHash<QString, int> destinationCounts;
    for (qsizetype row = 0; row < records.size(); ++row) {
        const QJsonObject record = records.at(row).toObject();
        const QDateTime started = QDateTime::fromString(
            record.value(QStringLiteral("started_at")).toString(), Qt::ISODateWithMs);
        const QString endedText = record.value(QStringLiteral("ended_at")).toString();
        const QDateTime ended = QDateTime::fromString(endedText, Qt::ISODateWithMs);
        const bool inProgress = endedText.isEmpty();
        const QString outcome = record.value(QStringLiteral("outcome")).toString();
        arrivedCount += outcome == QStringLiteral("arrived") ? 1 : 0;
        canceledCount += outcome == QStringLiteral("canceled") ? 1 : 0;
        failedCount += outcome == QStringLiteral("failed") ? 1 : 0;
        const QString destinationName =
            record.value(QStringLiteral("destination_name")).toString();
        ++destinationCounts[destinationName];
        const qint64 seconds = inProgress ? 0 : started.secsTo(ended);
        const QString duration = inProgress
            ? tr("—")
            : seconds >= 3600
                ? tr("%1시간 %2분").arg(seconds / 3600).arg((seconds % 3600) / 60)
                : tr("%1분 %2초").arg(seconds / 60).arg(seconds % 60);
        const QStringList values{
            destinationName,
            started.isValid() ? started.toString(QStringLiteral("yyyy.MM.dd  HH:mm:ss")) : tr("—"),
            ended.isValid() ? ended.toString(QStringLiteral("yyyy.MM.dd  HH:mm:ss")) : tr("—"),
            duration};
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            item->setTextAlignment(column == 0 ? Qt::AlignLeft | Qt::AlignVCenter
                                               : Qt::AlignCenter);
            historyTable->setItem(static_cast<int>(row), column, item);
        }
        auto *statusCell = new QWidget;
        auto *statusLayout = new QHBoxLayout(statusCell);
        statusLayout->setContentsMargins(8, 5, 8, 5);
        QString statusText;
        QString statusStyle;
        if (inProgress) {
            statusText = tr("●  운행 중");
            statusStyle = QStringLiteral("background:#FFF2D9;color:#9A5A10;border-radius:12px;padding:3px 10px;font-size:13px;font-weight:700;");
        } else if (outcome == QStringLiteral("arrived")) {
            statusText = tr("✓  목적지 안내 완료");
            statusStyle = QStringLiteral("background:#E8F7EF;color:#247A5B;border-radius:12px;padding:3px 10px;font-size:13px;font-weight:700;");
        } else if (outcome == QStringLiteral("canceled")) {
            statusText = tr("■  안내 취소");
            statusStyle = QStringLiteral("background:#FFF0E8;color:#B45B25;border-radius:12px;padding:3px 10px;font-size:13px;font-weight:700;");
        } else if (outcome == QStringLiteral("failed")) {
            statusText = tr("!  안내 실패");
            statusStyle = QStringLiteral("background:#FDEBEC;color:#B43D48;border-radius:12px;padding:3px 10px;font-size:13px;font-weight:700;");
        } else {
            statusText = tr("종료");
            statusStyle = QStringLiteral("background:#EEF1F5;color:#657186;border-radius:12px;padding:3px 10px;font-size:13px;font-weight:700;");
        }
        auto *badge = new QLabel(statusText);
        badge->setAlignment(Qt::AlignCenter);
        badge->setStyleSheet(statusStyle);
        statusLayout->addWidget(badge);
        historyTable->setCellWidget(static_cast<int>(row), 4, statusCell);
    }

    historySummaryLabel->setText(tr("최근 운행 기록 %1건 · 최신순").arg(records.size()));
    QString popular = tr("—");
    int popularCount = 0;
    for (auto it = destinationCounts.cbegin(); it != destinationCounts.cend(); ++it) {
        if (it.value() > popularCount) {
            popular = it.key();
            popularCount = it.value();
        }
    }
    historyTotalValue->setText(QString::number(records.size()));
    historyArrivedValue->setText(QString::number(arrivedCount));
    historyCanceledValue->setText(QString::number(canceledCount));
    historyFailedValue->setText(QString::number(failedCount));
    if (records.isEmpty()) {
        historyPopularValue->setText(tr("—"));
    } else {
        const int percentage = qRound(100.0 * popularCount / records.size());
        historyPopularValue->setText(tr("%1  (%2%)").arg(popular).arg(percentage));
    }
    const bool empty = records.isEmpty();
    historyTable->setVisible(!empty);
    historyEmptyLabel->setText(tr("아직 저장된 운행 기록이 없습니다."));
    historyEmptyLabel->setVisible(empty);
}

void MainWindow::startCameraStream()
{
    const QString rtpCaps =
        QStringLiteral("application/x-rtp,media=video,encoding-name=H264,"
                       "payload=96,clock-rate=90000");
    const QString rawCaps =
        QStringLiteral("video/x-raw,format=RGB,width=%1,height=%2,"
                       "pixel-aspect-ratio=1/1")
            .arg(kVideoWidth)
            .arg(kVideoHeight);

    const QStringList arguments = {
        QStringLiteral("-q"),
        QStringLiteral("udpsrc"), QStringLiteral("port=5000"),
        QStringLiteral("reuse=false"),
        QStringLiteral("caps=%1").arg(rtpCaps),
        QStringLiteral("!"),
        QStringLiteral("rtpjitterbuffer"), QStringLiteral("latency=100"),
        QStringLiteral("drop-on-latency=true"),
        QStringLiteral("!"), QStringLiteral("rtph264depay"),
        QStringLiteral("!"), QStringLiteral("h264parse"),
        QStringLiteral("!"), QStringLiteral("avdec_h264"),
        QStringLiteral("!"), QStringLiteral("videoconvert"),
        QStringLiteral("!"), QStringLiteral("videoscale"),
        QStringLiteral("!"), rawCaps,
        QStringLiteral("!"), QStringLiteral("fdsink"),
        QStringLiteral("fd=1"), QStringLiteral("sync=false")
    };

    cameraBuffer.clear();
    cameraProcess->start(QStringLiteral("gst-launch-1.0"), arguments,
                         QIODevice::ReadOnly);

    QTimer::singleShot(5000, this, [this] {
        if (currentFrame.isNull()
            && cameraProcess->state() == QProcess::Running) {
            setCameraStatus(
                tr("영상 패킷을 기다리는 중입니다 (UDP 5000)"), false);
        }
    });
}

void MainWindow::readCameraFrames()
{
    cameraBuffer.append(cameraProcess->readAllStandardOutput());

    // If rendering falls behind, retain only the newest complete frame.
    if (cameraBuffer.size() >= kFrameSize * 2) {
        const qsizetype completeFrames = cameraBuffer.size() / kFrameSize;
        cameraBuffer.remove(0, (completeFrames - 1) * kFrameSize);
    }

    while (cameraBuffer.size() >= kFrameSize) {
        const QImage image(
            reinterpret_cast<const uchar *>(cameraBuffer.constData()),
            kVideoWidth, kVideoHeight, kVideoWidth * kBytesPerPixel,
            QImage::Format_RGB888);
        currentFrame = QPixmap::fromImage(image.copy());
        cameraBuffer.remove(0, kFrameSize);
    }

    if (!currentFrame.isNull()) {
        updateCameraFrame();
        setCameraStatus(QString(), true);
    }
}

void MainWindow::updateCameraFrame()
{
    ui->cameraViewLabel->setPixmap(
        currentFrame.scaled(ui->cameraViewLabel->size(),
                            Qt::KeepAspectRatio,
                            Qt::SmoothTransformation));
}

void MainWindow::setCameraStatus(const QString &message, bool streaming)
{
    if (streaming)
        return;

    currentFrame = QPixmap();
    ui->cameraViewLabel->setPixmap(QPixmap());
    ui->cameraViewLabel->setText(message);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (historyPage)
        historyPage->setGeometry(ui->centralwidget->rect());
    if (!currentFrame.isNull())
        updateCameraFrame();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if ((historyPage && historyPage->isVisible())
        || !ui->manualMoveButton->isChecked()) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    double linearX = 0.0;
    double angularZ = 0.0;
    switch (event->key()) {
    case Qt::Key_W:
        linearX = 0.15;
        break;
    case Qt::Key_S:
        linearX = -0.15;
        break;
    case Qt::Key_A:
        angularZ = 1.0;
        break;
    case Qt::Key_D:
        angularZ = -1.0;
        break;
    default:
        QMainWindow::keyPressEvent(event);
        return;
    }

    // A physical key press publishes one command. Ignore OS key-repeat.
    if (!event->isAutoRepeat())
        publishVelocity(linearX, angularZ);
    event->accept();
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    const bool movementKey = event->key() == Qt::Key_W
                             || event->key() == Qt::Key_A
                             || event->key() == Qt::Key_S
                             || event->key() == Qt::Key_D;

    if (ui->manualMoveButton->isChecked()
        && movementKey && !event->isAutoRepeat()) {
        publishVelocity(0.0, 0.0);
        event->accept();
        return;
    }
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::sendManualMode(bool enabled)
{
    publishVelocity(0.0, 0.0);

    ui->manualMoveButton->setText(
        enabled ? tr("✓  수동 이동 중") : tr("↔  수동 이동"));
    ui->manualCaptionLabel->setStyleSheet(QString());
    ui->manualCaptionLabel->setText(
        enabled
            ? tr("수동 운행 모드입니다")
            : tr("키보드 또는 조작 화면으로 로봇을 직접 이동"));

    QNetworkRequest request(
        QUrl(kServerBaseUrl + QStringLiteral("/api/manual-mode")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setTransferTimeout(5000);

    // The server stores this state for the Web HMI and publishes the
    // /manual_mode ROS topic for the robot.
    const QJsonObject command{{QStringLiteral("manual_mode"), enabled}};
    const QByteArray body =
        QJsonDocument(command).toJson(QJsonDocument::Compact);
    request.setHeader(QNetworkRequest::ContentLengthHeader, body.size());
    QNetworkReply *reply = networkManager->post(request, body);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool succeeded = reply->error() == QNetworkReply::NoError
                               && statusCode >= 200 && statusCode < 300;
        setServerConnected(succeeded);
        reply->deleteLater();
    });
}

void MainWindow::checkServerConnection()
{
    QNetworkRequest request(
        QUrl(kServerBaseUrl + QStringLiteral("/api/status")));
    request.setTransferTimeout(3000);

    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool connected = reply->error() == QNetworkReply::NoError
                               && statusCode >= 200 && statusCode < 300;
        setServerConnected(connected);

        if (connected) {
            QJsonParseError error;
            const QJsonDocument document =
                QJsonDocument::fromJson(reply->readAll(), &error);
            if (error.error == QJsonParseError::NoError && document.isObject()) {
                const QJsonValue destination =
                    document.object().value(QStringLiteral("destination"));
                setDestination(destination.isString()
                                   ? destination.toString()
                                   : QString());
            }
        }
        reply->deleteLater();
    });
}

void MainWindow::setServerConnected(bool connected)
{
    serverConnected = connected;
    ui->connectionText->setText(
        connected ? tr("서버 연결됨") : tr("서버 연결 안 됨"));
    ui->connectionText->setStyleSheet(
        connected ? QStringLiteral("color: #167345;")
                  : QStringLiteral("color: #A56816;"));
    ui->connectionDot->setStyleSheet(
        connected ? QStringLiteral("background-color: #22A861;")
                  : QStringLiteral("background-color: #E6A23C;"));
    ui->connectionBadge->setStyleSheet(
        connected
            ? QStringLiteral("background-color: #EAF8F0;"
                             "border: 1px solid #BFE8CE;"
                             "border-radius: 16px;")
            : QStringLiteral("background-color: #FFF7E8;"
                             "border: 1px solid #F1D39A;"
                             "border-radius: 16px;"));
}

void MainWindow::publishVelocity(double linearX, double angularZ)
{
    startVelocityPublisher(velocityPublisher, linearX, angularZ);
}

void MainWindow::startVelocityPublisher(QProcess *process,
                                        double linearX, double angularZ)
{
    if (process->state() == QProcess::NotRunning) {
        process->setStandardOutputFile(QProcess::nullDevice());
        process->setStandardErrorFile(QProcess::nullDevice());
        const QString helper = QCoreApplication::applicationDirPath()
                               + QStringLiteral("/velocity_publisher.py");
        // Qt Creator or a directly launched binary may not inherit a sourced
        // ROS environment. Source Jazzy for the helper process explicitly.
        process->start(
            QStringLiteral("/bin/bash"),
            {QStringLiteral("-c"),
             QStringLiteral("source /opt/ros/jazzy/setup.bash && "
                            "exec /usr/bin/python3 \"$1\" \"$2\""),
             QStringLiteral("manager-screen-velocity"),
             helper,
             kVelocityTopic});
        if (!process->waitForStarted(2000))
            return;
    }

    const QByteArray command = QStringLiteral("%1 %2\n")
                                   .arg(linearX, 0, 'f', 2)
                                   .arg(angularZ, 0, 'f', 2)
                                   .toUtf8();
    process->write(command);
}
