#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QByteArray>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QProcess;
class QNetworkAccessManager;
class QKeyEvent;
class QResizeEvent;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void startCameraStream();
    void readCameraFrames();
    void updateCameraFrame();
    void setCameraStatus(const QString &message, bool streaming);
    void startRobotStatusMonitor();
    void readRobotStatusEvents();
    void setDestination(const QString &destinationId);
    void checkServerConnection();
    void setServerConnected(bool connected);
    void sendManualMode(bool enabled);
    void publishManualMode(bool enabled);
    void publishVelocity(double linearX, double angularZ);
    void startVelocityPublisher(QProcess *process,
                                double linearX, double angularZ);

    Ui::MainWindow *ui;
    QProcess *cameraProcess;
    QProcess *manualModePublisher;
    QProcess *velocityPublisher;
    QProcess *robotStatusMonitor;
    QNetworkAccessManager *networkManager;
    QByteArray cameraBuffer;
    QPixmap currentFrame;
    bool serverConnected = false;
};
#endif // MAINWINDOW_H
