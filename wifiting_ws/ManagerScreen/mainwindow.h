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
class QJsonArray;
class QLabel;
class QTableWidget;
class QWidget;
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
    void createHistoryPage();
    void showHistoryPage();
    void hideHistoryPage();
    void loadHistory();
    void populateHistory(const QJsonArray &records);
    void checkServerConnection();
    void setServerConnected(bool connected);
    void sendManualMode(bool enabled);
    void publishVelocity(double linearX, double angularZ);
    void startVelocityPublisher(QProcess *process,
                                double linearX, double angularZ);

    Ui::MainWindow *ui;
    QProcess *cameraProcess;
    QProcess *velocityPublisher;
    QProcess *robotStatusMonitor;
    QNetworkAccessManager *networkManager;
    QWidget *historyPage = nullptr;
    QTableWidget *historyTable = nullptr;
    QLabel *historySummaryLabel = nullptr;
    QLabel *historyEmptyLabel = nullptr;
    QLabel *historyTotalValue = nullptr;
    QLabel *historyArrivedValue = nullptr;
    QLabel *historyCanceledValue = nullptr;
    QLabel *historyFailedValue = nullptr;
    QLabel *historyPopularValue = nullptr;
    QByteArray cameraBuffer;
    QPixmap currentFrame;
    bool serverConnected = false;
};
#endif // MAINWINDOW_H
