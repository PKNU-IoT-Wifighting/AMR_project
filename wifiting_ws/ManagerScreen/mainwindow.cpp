#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QImage>
#include <QHash>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QKeyEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QResizeEvent>
#include <QTimer>

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
    , manualModePublisher(new QProcess(this))
    , velocityPublisher(new QProcess(this))
    , robotStatusMonitor(new QProcess(this))
    , networkManager(new QNetworkAccessManager(this))
{
    ui->setupUi(this);

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

    manualModePublisher->terminate();
    if (!manualModePublisher->waitForFinished(500))
        manualModePublisher->kill();

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

    robotStatusMonitor->setProcessChannelMode(QProcess::SeparateChannels);
    connect(robotStatusMonitor, &QProcess::readyReadStandardOutput,
            this, &MainWindow::readRobotStatusEvents);
    connect(robotStatusMonitor,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                ui->robotConnectionValueLabel->setText(tr("연결 안 됨"));
                ui->robotConnectionValueLabel->setStyleSheet(
                    QStringLiteral("color: #A56816;"));
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
    if (!currentFrame.isNull())
        updateCameraFrame();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (!ui->manualMoveButton->isChecked()) {
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
    publishManualMode(enabled);
    publishVelocity(0.0, 0.0);

    ui->manualMoveButton->setText(
        enabled ? tr("✓  수동 이동 중") : tr("↔  수동 이동"));
    ui->manualCaptionLabel->setStyleSheet(QString());
    ui->manualCaptionLabel->setText(
        enabled
            ? tr("수동 운행 모드입니다")
            : tr("키보드 또는 조작 화면으로 로봇을 직접 이동"));

    // Manual driving and ROS commands must work even without the HTTP server.
    // Only notify the web HMI when the periodic connection check says the
    // server is currently reachable.
    if (!serverConnected)
        return;

    QNetworkRequest request(
        QUrl(kServerBaseUrl + QStringLiteral("/api/command")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setTransferTimeout(5000);

    // Update the local UI immediately. The request only notifies the server so
    // the web HMI can be locked; its response never controls the button state.
    const QJsonObject command{
        {QStringLiteral("command"), QStringLiteral("stop")},
        {QStringLiteral("manual_mode"), enabled}};
    const QByteArray body =
        QJsonDocument(command).toJson(QJsonDocument::Compact);
    request.setHeader(QNetworkRequest::ContentLengthHeader, body.size());
    QNetworkReply *reply = networkManager->post(request, body);

    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
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

void MainWindow::publishManualMode(bool enabled)
{
    const QString ros2 =
        QStringLiteral("/opt/ros/jazzy/bin/ros2");
    const QString topic = qEnvironmentVariable(
        "MANUAL_MODE_TOPIC", QStringLiteral("/manual_mode"));
    const QString message = enabled
                                ? QStringLiteral("{data: true}")
                                : QStringLiteral("{data: false}");

    if (manualModePublisher->state() != QProcess::NotRunning) {
        manualModePublisher->terminate();
        if (!manualModePublisher->waitForFinished(500))
            manualModePublisher->kill();
    }

    if (enabled) {
        // Keep publishing while manual mode is active so a Raspberry Pi that
        // joins later also receives the stop/manual-mode state.
        manualModePublisher->start(
            ros2,
            {QStringLiteral("topic"), QStringLiteral("pub"),
             QStringLiteral("--rate"), QStringLiteral("2"),
             topic, QStringLiteral("std_msgs/msg/Bool"), message});
        return;
    }

    // Publish the inactive state once, using a short-lived child process.
    auto *inactivePublisher = new QProcess(this);
    connect(inactivePublisher,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            inactivePublisher, &QObject::deleteLater);
    inactivePublisher->start(
        ros2,
        {QStringLiteral("topic"), QStringLiteral("pub"),
         QStringLiteral("--once"),
         topic, QStringLiteral("std_msgs/msg/Bool"), message});
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
        process->start(QStringLiteral("/usr/bin/python3"),
                       {helper, kVelocityTopic});
        if (!process->waitForStarted(2000))
            return;
    }

    const QByteArray command = QStringLiteral("%1 %2\n")
                                   .arg(linearX, 0, 'f', 2)
                                   .arg(angularZ, 0, 'f', 2)
                                   .toUtf8();
    process->write(command);
}
