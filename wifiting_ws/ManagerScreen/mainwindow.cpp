#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QImage>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QTimer>

namespace {
constexpr int kVideoWidth = 640;
constexpr int kVideoHeight = 360;
constexpr int kBytesPerPixel = 3;
constexpr qsizetype kFrameSize =
    kVideoWidth * kVideoHeight * kBytesPerPixel;

// Real robot mode: use /cmd_vel and the REST server on this computer.
constexpr bool kLocalManualTestMode = false;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , cameraProcess(new QProcess(this))
    , manualModePublisher(new QProcess(this))
    , velocityPublisher(new QProcess(this))
    , networkManager(new QNetworkAccessManager(this))
    , serverBaseUrl(qEnvironmentVariable(
          "MANAGER_SERVER_URL",
          QStringLiteral("http://127.0.0.1:8080")))
    , offlineMode(qEnvironmentVariableIntValue("MANAGER_OFFLINE_MODE") == 1)
{
    ui->setupUi(this);

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

    if (kLocalManualTestMode) {
        ui->connectionText->setText(tr("로컬 테스트 모드"));
        ui->connectionText->setStyleSheet(
            QStringLiteral("color: #245EC7;"));
        ui->connectionDot->setStyleSheet(
            QStringLiteral("background-color: #3478E5;"));
        QTimer::singleShot(0, this, &MainWindow::startAutonomousTestMotion);
    } else if (offlineMode) {
        ui->connectionText->setText(tr("서버 없이 실행 중"));
        ui->connectionText->setStyleSheet(
            QStringLiteral("color: #245EC7;"));
        ui->connectionDot->setStyleSheet(
            QStringLiteral("background-color: #3478E5;"));
    } else {
        setServerConnected(false);
        checkServerConnection();
        auto *serverCheckTimer = new QTimer(this);
        serverCheckTimer->setInterval(5000);
        connect(serverCheckTimer, &QTimer::timeout,
                this, &MainWindow::checkServerConnection);
        serverCheckTimer->start();
    }

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
    stopAutonomousTestMotion();
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
        linearX = 0.05;
        break;
    case Qt::Key_S:
        linearX = -0.05;
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
    const bool previousState = !enabled;
    publishManualMode(enabled);
    publishVelocity(0.0, 0.0);

    if (kLocalManualTestMode) {
        if (enabled)
            stopAutonomousTestMotion();

        ui->manualMoveButton->setText(
            enabled ? tr("✓  수동 이동 중") : tr("↔  수동 이동"));
        ui->manualCaptionLabel->setStyleSheet(QString());
        ui->manualCaptionLabel->setText(
            enabled ? tr("W/A/S/D로 로봇을 조작합니다 (키를 놓으면 정지)")
                    : tr("키보드 또는 조작 화면으로 로봇을 직접 이동"));
        ui->driveStatusValueLabel->setText(
            enabled ? tr("수동 운행") : tr("대기 중"));
        if (!enabled) {
            velocityPublisher->terminate();
            if (!velocityPublisher->waitForFinished(500))
                velocityPublisher->kill();
            startAutonomousTestMotion();
        }
        return;
    }

    if (offlineMode) {
        ui->manualMoveButton->setText(
            enabled ? tr("✓  수동 이동 중") : tr("↔  수동 이동"));
        ui->manualCaptionLabel->setStyleSheet(QString());
        ui->manualCaptionLabel->setText(
            enabled ? tr("서버 없이 ROS 2 수동 운행 중")
                    : tr("키보드 또는 조작 화면으로 로봇을 직접 이동"));
        ui->driveStatusValueLabel->setText(
            enabled ? tr("수동 운행") : tr("대기 중"));
        return;
    }

    ui->manualMoveButton->setEnabled(false);
    ui->manualMoveButton->setText(
        enabled ? tr("수동 모드 시작 중...") : tr("수동 모드 종료 중..."));
    ui->manualCaptionLabel->setStyleSheet(QString());
    ui->manualCaptionLabel->setText(tr("서버에 운행 모드를 전송하고 있습니다"));

    QNetworkRequest request(
        QUrl(serverBaseUrl + QStringLiteral("/api/command")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setTransferTimeout(5000);

    // Entering or leaving manual mode keeps autonomous movement stopped.
    const QJsonObject command{
        {QStringLiteral("command"), QStringLiteral("stop")}};
    const QByteArray body =
        QJsonDocument(command).toJson(QJsonDocument::Compact);
    request.setHeader(QNetworkRequest::ContentLengthHeader, body.size());
    QNetworkReply *reply = networkManager->post(request, body);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, enabled, previousState] {
                const QByteArray responseBody = reply->readAll();
                const int statusCode =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                        .toInt();
                const bool succeeded =
                    reply->error() == QNetworkReply::NoError
                    && statusCode >= 200 && statusCode < 300;

                ui->manualMoveButton->setEnabled(true);
                if (succeeded) {
                    setServerConnected(true);
                    ui->manualMoveButton->setText(
                        enabled ? tr("✓  수동 이동 중")
                                : tr("↔  수동 이동"));
                    ui->manualCaptionLabel->setStyleSheet(QString());
                    ui->manualCaptionLabel->setText(
                        enabled
                            ? tr("서버에 수동 운행 모드가 설정되었습니다")
                            : tr("키보드 또는 조작 화면으로 로봇을 직접 이동"));
                } else {
                    setServerConnected(false);
                    publishManualMode(previousState);
                    {
                        const QSignalBlocker blocker(ui->manualMoveButton);
                        ui->manualMoveButton->setChecked(previousState);
                    }
                    ui->manualMoveButton->setText(
                        previousState ? tr("✓  수동 이동 중")
                                      : tr("↔  수동 이동"));
                    ui->manualCaptionLabel->setStyleSheet(
                        QStringLiteral("color: #D63C42;"));
                    const QString serverMessage =
                        QString::fromUtf8(responseBody).trimmed();
                    ui->manualCaptionLabel->setText(
                        serverMessage.isEmpty()
                            ? tr("서버 전송 실패: %1")
                                  .arg(reply->errorString())
                            : tr("서버 오류(%1): %2")
                                  .arg(statusCode)
                                  .arg(serverMessage));
                }
                reply->deleteLater();
            });
}

void MainWindow::checkServerConnection()
{
    QNetworkRequest request(
        QUrl(serverBaseUrl + QStringLiteral("/api/status")));
    request.setTransferTimeout(3000);

    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        setServerConnected(reply->error() == QNetworkReply::NoError
                           && statusCode >= 200 && statusCode < 300);
        reply->deleteLater();
    });
}

void MainWindow::setServerConnected(bool connected)
{
    ui->connectionText->setText(
        connected ? tr("서버 연결됨") : tr("서버 연결 중..."));
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

void MainWindow::startAutonomousTestMotion()
{
    // A simple curved trajectory stands in for Nav2 during the local test.
    publishVelocity(0.3, 0.15);
    ui->driveStatusValueLabel->setText(tr("자율 주행 테스트"));
}

void MainWindow::stopAutonomousTestMotion()
{
    publishVelocity(0.0, 0.0);
}

void MainWindow::startVelocityPublisher(QProcess *process,
                                        double linearX, double angularZ)
{
    const QString defaultTopic = kLocalManualTestMode
                                     ? QStringLiteral("/model/vehicle_blue/cmd_vel")
                                     : QStringLiteral("/cmd_vel");
    const QString topic = qEnvironmentVariable(
        "CMD_VEL_TOPIC", defaultTopic);
    if (process->state() == QProcess::NotRunning) {
        process->setStandardOutputFile(QProcess::nullDevice());
        process->setStandardErrorFile(QProcess::nullDevice());
        const QString helper = QCoreApplication::applicationDirPath()
                               + QStringLiteral("/velocity_publisher.py");
        process->start(QStringLiteral("/usr/bin/python3"), {helper, topic});
        if (!process->waitForStarted(2000))
            return;
    }

    const QByteArray command = QStringLiteral("%1 %2\n")
                                   .arg(linearX, 0, 'f', 2)
                                   .arg(angularZ, 0, 'f', 2)
                                   .toUtf8();
    process->write(command);
}
