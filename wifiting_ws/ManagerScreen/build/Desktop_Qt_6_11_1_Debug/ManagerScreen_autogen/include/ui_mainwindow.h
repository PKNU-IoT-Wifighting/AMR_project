/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *rootLayout;
    QHBoxLayout *headerLayout;
    QVBoxLayout *titleLayout;
    QLabel *titleLabel;
    QLabel *subtitleLabel;
    QSpacerItem *headerSpacer;
    QFrame *connectionBadge;
    QHBoxLayout *connectionBadgeLayout;
    QLabel *connectionDot;
    QLabel *connectionText;
    QFrame *cameraCard;
    QVBoxLayout *cameraCardLayout;
    QHBoxLayout *cameraHeaderRowLayout;
    QHBoxLayout *cameraViewportRowLayout;
    QFrame *robotStatusPanel;
    QVBoxLayout *robotStatusPanelLayout;
    QLabel *statusPanelTitleLabel;
    QLabel *statusPanelDescriptionLabel;
    QFrame *speedStatusCard;
    QHBoxLayout *speedStatusCardLayout;
    QLabel *speedTitleLabel;
    QSpacerItem *speedStatusSpacer;
    QLabel *speedValueLabel;
    QFrame *batteryStatusCard;
    QHBoxLayout *batteryStatusCardLayout;
    QLabel *batteryTitleLabel;
    QSpacerItem *batteryStatusSpacer;
    QLabel *batteryValueLabel;
    QFrame *temperatureStatusCard;
    QHBoxLayout *temperatureStatusCardLayout;
    QLabel *temperatureTitleLabel;
    QSpacerItem *temperatureStatusSpacer;
    QLabel *temperatureValueLabel;
    QFrame *driveStatusCard;
    QHBoxLayout *driveStatusCardLayout;
    QLabel *driveStatusTitleLabel;
    QSpacerItem *driveStatusSpacer;
    QLabel *driveStatusValueLabel;
    QSpacerItem *statusCameraGapSpacer;
    QFrame *cameraViewport;
    QVBoxLayout *cameraViewportLayout;
    QSpacerItem *cameraTopSpacer;
    QLabel *cameraViewLabel;
    QLabel *cameraHintLabel;
    QSpacerItem *cameraBottomSpacer;
    QLabel *cameraSourceLabel;
    QHBoxLayout *actionLayout;
    QVBoxLayout *manualActionLayout;
    QPushButton *manualMoveButton;
    QLabel *manualCaptionLabel;
    QVBoxLayout *emergencyActionLayout;
    QPushButton *emergencyStopButton;
    QLabel *emergencyCaptionLabel;
    QVBoxLayout *logActionLayout;
    QPushButton *logViewButton;
    QLabel *logCaptionLabel;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1180, 760);
        MainWindow->setMinimumSize(QSize(900, 620));
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        centralwidget->setStyleSheet(QString::fromUtf8("QWidget#centralwidget {\n"
"    background-color: #F3F6FA;\n"
"    color: #172033;\n"
"    font-family: \"Noto Sans CJK KR\", \"Noto Sans KR\", \"Sans Serif\";\n"
"}\n"
"\n"
"/* \355\227\244\353\215\224 */\n"
"QLabel#titleLabel {\n"
"    color: #172033;\n"
"    font-size: 27px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QLabel#subtitleLabel {\n"
"    color: #778198;\n"
"    font-size: 13px;\n"
"}\n"
"\n"
"QFrame#connectionBadge {\n"
"    background-color: #EAF8F0;\n"
"    border: 1px solid #BFE8CE;\n"
"    border-radius: 16px;\n"
"}\n"
"\n"
"QLabel#connectionDot {\n"
"    background-color: #22A861;\n"
"    border-radius: 5px;\n"
"    min-width: 10px;\n"
"    max-width: 10px;\n"
"    min-height: 10px;\n"
"    max-height: 10px;\n"
"}\n"
"\n"
"QLabel#connectionText {\n"
"    color: #167345;\n"
"    font-size: 13px;\n"
"    font-weight: 600;\n"
"}\n"
"\n"
"/* \354\271\264\353\251\224\353\235\274 \354\271\264\353\223\234 */\n"
"QFrame#cameraCard {\n"
"    background-color: #FFFFFF;\n"
"    border: 1px solid #E1E7F0;\n"
""
                        "    border-radius: 18px;\n"
"}\n"
"\n"
"QLabel#cameraTitleLabel {\n"
"    color: #1B2437;\n"
"    font-size: 18px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QLabel#cameraDescriptionLabel {\n"
"    color: #8A94A8;\n"
"    font-size: 12px;\n"
"}\n"
"\n"
"QFrame#streamBadge {\n"
"    background-color: #FFF1F0;\n"
"    border: 1px solid #FFD1CD;\n"
"    border-radius: 13px;\n"
"}\n"
"\n"
"QLabel#streamDot {\n"
"    background-color: #E64940;\n"
"    border-radius: 4px;\n"
"    min-width: 8px;\n"
"    max-width: 8px;\n"
"    min-height: 8px;\n"
"    max-height: 8px;\n"
"}\n"
"\n"
"QLabel#streamStatusLabel {\n"
"    color: #C63B34;\n"
"    font-size: 12px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QFrame#cameraViewport {\n"
"    background-color: #111827;\n"
"    border: 1px solid #263247;\n"
"    border-radius: 14px;\n"
"}\n"
"\n"
"QLabel#cameraViewLabel {\n"
"    background-color: transparent;\n"
"    border: none;\n"
"    color: #D8DFEA;\n"
"    font-size: 20px;\n"
"    font-weight: 600;\n"
"}\n"
"\n"
"QLabel#camer"
                        "aHintLabel {\n"
"    color: #8792A7;\n"
"    font-size: 12px;\n"
"}\n"
"\n"
"QLabel#cameraSourceLabel {\n"
"    color: #7A8497;\n"
"    font-size: 12px;\n"
"}\n"
"\n"
"/* \353\241\234\353\264\207 \354\203\201\355\203\234 \354\240\225\353\263\264 */\n"
"QFrame#robotStatusPanel {\n"
"    background-color: transparent;\n"
"    border: none;\n"
"}\n"
"\n"
"QLabel#statusPanelTitleLabel {\n"
"    color: #1B2437;\n"
"    font-size: 17px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QLabel#statusPanelDescriptionLabel {\n"
"    color: #8A94A8;\n"
"    font-size: 12px;\n"
"}\n"
"\n"
"QFrame#speedStatusCard,\n"
"QFrame#batteryStatusCard,\n"
"QFrame#temperatureStatusCard,\n"
"QFrame#driveStatusCard {\n"
"    background-color: #F7F9FC;\n"
"    border: 1px solid #E2E8F1;\n"
"    border-radius: 12px;\n"
"}\n"
"\n"
"QLabel#speedTitleLabel,\n"
"QLabel#batteryTitleLabel,\n"
"QLabel#temperatureTitleLabel,\n"
"QLabel#driveStatusTitleLabel {\n"
"    color: #737E92;\n"
"    font-size: 12px;\n"
"    font-weight: 600;\n"
"}\n"
"\n"
"QLab"
                        "el#speedValueLabel,\n"
"QLabel#batteryValueLabel,\n"
"QLabel#temperatureValueLabel,\n"
"QLabel#driveStatusValueLabel {\n"
"    color: #1C2940;\n"
"    font-size: 19px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QLabel#driveStatusValueLabel {\n"
"    color: #247A5B;\n"
"}\n"
"\n"
"/* \355\225\230\353\213\250 \352\270\260\353\212\245 \353\262\204\355\212\274 */\n"
"QPushButton {\n"
"    border-radius: 14px;\n"
"    min-height: 88px;\n"
"    padding: 10px 22px;\n"
"    font-size: 19px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QPushButton#manualMoveButton {\n"
"    color: #245EC7;\n"
"    background-color: #FFFFFF;\n"
"    border: 2px solid #AFC9F8;\n"
"}\n"
"\n"
"QPushButton#manualMoveButton:hover {\n"
"    background-color: #EEF5FF;\n"
"    border-color: #5B8FEA;\n"
"}\n"
"\n"
"QPushButton#manualMoveButton:pressed {\n"
"    background-color: #DCEAFF;\n"
"}\n"
"\n"
"QPushButton#manualMoveButton:checked {\n"
"    color: #FFFFFF;\n"
"    background-color: #3478E5;\n"
"    border-color: #3478E5;\n"
"}\n"
"\n"
"QPushBu"
                        "tton#manualMoveButton:checked:hover {\n"
"    background-color: #2868CC;\n"
"    border-color: #2868CC;\n"
"}\n"
"\n"
"QPushButton#emergencyStopButton {\n"
"    color: #FFFFFF;\n"
"    background-color: #E5484D;\n"
"    border: 2px solid #E5484D;\n"
"}\n"
"\n"
"QPushButton#emergencyStopButton:hover {\n"
"    background-color: #D63C42;\n"
"    border-color: #D63C42;\n"
"}\n"
"\n"
"QPushButton#emergencyStopButton:pressed {\n"
"    background-color: #BF3137;\n"
"    border-color: #BF3137;\n"
"}\n"
"\n"
"QPushButton#logViewButton {\n"
"    color: #216A66;\n"
"    background-color: #FFFFFF;\n"
"    border: 2px solid #A7DCD8;\n"
"}\n"
"\n"
"QPushButton#logViewButton:hover {\n"
"    background-color: #ECFAF8;\n"
"    border-color: #55B7AF;\n"
"}\n"
"\n"
"QPushButton#logViewButton:pressed {\n"
"    background-color: #D7F2EF;\n"
"}\n"
"\n"
"QLabel#manualCaptionLabel,\n"
"QLabel#emergencyCaptionLabel,\n"
"QLabel#logCaptionLabel {\n"
"    color: #8892A5;\n"
"    font-size: 12px;\n"
"}"));
        rootLayout = new QVBoxLayout(centralwidget);
        rootLayout->setSpacing(20);
        rootLayout->setObjectName("rootLayout");
        rootLayout->setContentsMargins(34, 28, 34, 30);
        headerLayout = new QHBoxLayout();
        headerLayout->setSpacing(16);
        headerLayout->setObjectName("headerLayout");
        titleLayout = new QVBoxLayout();
        titleLayout->setSpacing(3);
        titleLayout->setObjectName("titleLayout");
        titleLabel = new QLabel(centralwidget);
        titleLabel->setObjectName("titleLabel");

        titleLayout->addWidget(titleLabel);

        subtitleLabel = new QLabel(centralwidget);
        subtitleLabel->setObjectName("subtitleLabel");

        titleLayout->addWidget(subtitleLabel);


        headerLayout->addLayout(titleLayout);

        headerSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        headerLayout->addItem(headerSpacer);

        connectionBadge = new QFrame(centralwidget);
        connectionBadge->setObjectName("connectionBadge");
        connectionBadge->setMinimumSize(QSize(122, 34));
        connectionBadge->setMaximumSize(QSize(170, 34));
        connectionBadge->setFrameShape(QFrame::Shape::NoFrame);
        connectionBadgeLayout = new QHBoxLayout(connectionBadge);
        connectionBadgeLayout->setSpacing(8);
        connectionBadgeLayout->setObjectName("connectionBadgeLayout");
        connectionBadgeLayout->setContentsMargins(13, 7, 13, 7);
        connectionDot = new QLabel(connectionBadge);
        connectionDot->setObjectName("connectionDot");

        connectionBadgeLayout->addWidget(connectionDot);

        connectionText = new QLabel(connectionBadge);
        connectionText->setObjectName("connectionText");

        connectionBadgeLayout->addWidget(connectionText);


        headerLayout->addWidget(connectionBadge);


        rootLayout->addLayout(headerLayout);

        cameraCard = new QFrame(centralwidget);
        cameraCard->setObjectName("cameraCard");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(cameraCard->sizePolicy().hasHeightForWidth());
        cameraCard->setSizePolicy(sizePolicy);
        cameraCard->setMaximumSize(QSize(16777215, 600));
        cameraCard->setFrameShape(QFrame::Shape::NoFrame);
        cameraCardLayout = new QVBoxLayout(cameraCard);
        cameraCardLayout->setSpacing(15);
        cameraCardLayout->setObjectName("cameraCardLayout");
        cameraCardLayout->setContentsMargins(22, 18, 22, 20);
        cameraHeaderRowLayout = new QHBoxLayout();
        cameraHeaderRowLayout->setSpacing(0);
        cameraHeaderRowLayout->setObjectName("cameraHeaderRowLayout");
        cameraHeaderRowLayout->setContentsMargins(0, 0, 0, 0);

        cameraCardLayout->addLayout(cameraHeaderRowLayout);

        cameraViewportRowLayout = new QHBoxLayout();
        cameraViewportRowLayout->setSpacing(0);
        cameraViewportRowLayout->setObjectName("cameraViewportRowLayout");
        cameraViewportRowLayout->setContentsMargins(0, 0, 0, 0);
        robotStatusPanel = new QFrame(cameraCard);
        robotStatusPanel->setObjectName("robotStatusPanel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(robotStatusPanel->sizePolicy().hasHeightForWidth());
        robotStatusPanel->setSizePolicy(sizePolicy1);
        robotStatusPanel->setMinimumSize(QSize(360, 300));
        robotStatusPanel->setMaximumSize(QSize(400, 300));
        robotStatusPanel->setFrameShape(QFrame::Shape::NoFrame);
        robotStatusPanelLayout = new QVBoxLayout(robotStatusPanel);
        robotStatusPanelLayout->setSpacing(9);
        robotStatusPanelLayout->setObjectName("robotStatusPanelLayout");
        robotStatusPanelLayout->setContentsMargins(0, 0, 0, 0);
        statusPanelTitleLabel = new QLabel(robotStatusPanel);
        statusPanelTitleLabel->setObjectName("statusPanelTitleLabel");

        robotStatusPanelLayout->addWidget(statusPanelTitleLabel);

        statusPanelDescriptionLabel = new QLabel(robotStatusPanel);
        statusPanelDescriptionLabel->setObjectName("statusPanelDescriptionLabel");

        robotStatusPanelLayout->addWidget(statusPanelDescriptionLabel);

        speedStatusCard = new QFrame(robotStatusPanel);
        speedStatusCard->setObjectName("speedStatusCard");
        speedStatusCard->setMinimumSize(QSize(0, 51));
        speedStatusCard->setFrameShape(QFrame::Shape::NoFrame);
        speedStatusCardLayout = new QHBoxLayout(speedStatusCard);
        speedStatusCardLayout->setObjectName("speedStatusCardLayout");
        speedStatusCardLayout->setContentsMargins(16, 8, 16, 8);
        speedTitleLabel = new QLabel(speedStatusCard);
        speedTitleLabel->setObjectName("speedTitleLabel");

        speedStatusCardLayout->addWidget(speedTitleLabel);

        speedStatusSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        speedStatusCardLayout->addItem(speedStatusSpacer);

        speedValueLabel = new QLabel(speedStatusCard);
        speedValueLabel->setObjectName("speedValueLabel");
        speedValueLabel->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        speedStatusCardLayout->addWidget(speedValueLabel);


        robotStatusPanelLayout->addWidget(speedStatusCard);

        batteryStatusCard = new QFrame(robotStatusPanel);
        batteryStatusCard->setObjectName("batteryStatusCard");
        batteryStatusCard->setMinimumSize(QSize(0, 51));
        batteryStatusCard->setFrameShape(QFrame::Shape::NoFrame);
        batteryStatusCardLayout = new QHBoxLayout(batteryStatusCard);
        batteryStatusCardLayout->setObjectName("batteryStatusCardLayout");
        batteryStatusCardLayout->setContentsMargins(16, 8, 16, 8);
        batteryTitleLabel = new QLabel(batteryStatusCard);
        batteryTitleLabel->setObjectName("batteryTitleLabel");

        batteryStatusCardLayout->addWidget(batteryTitleLabel);

        batteryStatusSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        batteryStatusCardLayout->addItem(batteryStatusSpacer);

        batteryValueLabel = new QLabel(batteryStatusCard);
        batteryValueLabel->setObjectName("batteryValueLabel");
        batteryValueLabel->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        batteryStatusCardLayout->addWidget(batteryValueLabel);


        robotStatusPanelLayout->addWidget(batteryStatusCard);

        temperatureStatusCard = new QFrame(robotStatusPanel);
        temperatureStatusCard->setObjectName("temperatureStatusCard");
        temperatureStatusCard->setMinimumSize(QSize(0, 51));
        temperatureStatusCard->setFrameShape(QFrame::Shape::NoFrame);
        temperatureStatusCardLayout = new QHBoxLayout(temperatureStatusCard);
        temperatureStatusCardLayout->setObjectName("temperatureStatusCardLayout");
        temperatureStatusCardLayout->setContentsMargins(16, 8, 16, 8);
        temperatureTitleLabel = new QLabel(temperatureStatusCard);
        temperatureTitleLabel->setObjectName("temperatureTitleLabel");

        temperatureStatusCardLayout->addWidget(temperatureTitleLabel);

        temperatureStatusSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        temperatureStatusCardLayout->addItem(temperatureStatusSpacer);

        temperatureValueLabel = new QLabel(temperatureStatusCard);
        temperatureValueLabel->setObjectName("temperatureValueLabel");
        temperatureValueLabel->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        temperatureStatusCardLayout->addWidget(temperatureValueLabel);


        robotStatusPanelLayout->addWidget(temperatureStatusCard);

        driveStatusCard = new QFrame(robotStatusPanel);
        driveStatusCard->setObjectName("driveStatusCard");
        driveStatusCard->setMinimumSize(QSize(0, 51));
        driveStatusCard->setFrameShape(QFrame::Shape::NoFrame);
        driveStatusCardLayout = new QHBoxLayout(driveStatusCard);
        driveStatusCardLayout->setObjectName("driveStatusCardLayout");
        driveStatusCardLayout->setContentsMargins(16, 8, 16, 8);
        driveStatusTitleLabel = new QLabel(driveStatusCard);
        driveStatusTitleLabel->setObjectName("driveStatusTitleLabel");

        driveStatusCardLayout->addWidget(driveStatusTitleLabel);

        driveStatusSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        driveStatusCardLayout->addItem(driveStatusSpacer);

        driveStatusValueLabel = new QLabel(driveStatusCard);
        driveStatusValueLabel->setObjectName("driveStatusValueLabel");
        driveStatusValueLabel->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        driveStatusCardLayout->addWidget(driveStatusValueLabel);


        robotStatusPanelLayout->addWidget(driveStatusCard);


        cameraViewportRowLayout->addWidget(robotStatusPanel);

        statusCameraGapSpacer = new QSpacerItem(28, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        cameraViewportRowLayout->addItem(statusCameraGapSpacer);

        cameraViewport = new QFrame(cameraCard);
        cameraViewport->setObjectName("cameraViewport");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(cameraViewport->sizePolicy().hasHeightForWidth());
        cameraViewport->setSizePolicy(sizePolicy2);
        cameraViewport->setMinimumSize(QSize(680, 400));
        cameraViewport->setMaximumSize(QSize(680, 400));
        cameraViewport->setFrameShape(QFrame::Shape::NoFrame);
        cameraViewportLayout = new QVBoxLayout(cameraViewport);
        cameraViewportLayout->setSpacing(7);
        cameraViewportLayout->setObjectName("cameraViewportLayout");
        cameraViewportLayout->setContentsMargins(18, 15, 18, 14);
        cameraTopSpacer = new QSpacerItem(20, 30, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        cameraViewportLayout->addItem(cameraTopSpacer);

        cameraViewLabel = new QLabel(cameraViewport);
        cameraViewLabel->setObjectName("cameraViewLabel");
        cameraViewLabel->setScaledContents(false);
        cameraViewLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        cameraViewportLayout->addWidget(cameraViewLabel);

        cameraHintLabel = new QLabel(cameraViewport);
        cameraHintLabel->setObjectName("cameraHintLabel");
        cameraHintLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        cameraViewportLayout->addWidget(cameraHintLabel);

        cameraBottomSpacer = new QSpacerItem(20, 30, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        cameraViewportLayout->addItem(cameraBottomSpacer);

        cameraSourceLabel = new QLabel(cameraViewport);
        cameraSourceLabel->setObjectName("cameraSourceLabel");
        cameraSourceLabel->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        cameraViewportLayout->addWidget(cameraSourceLabel);


        cameraViewportRowLayout->addWidget(cameraViewport);


        cameraCardLayout->addLayout(cameraViewportRowLayout);


        rootLayout->addWidget(cameraCard);

        actionLayout = new QHBoxLayout();
        actionLayout->setSpacing(18);
        actionLayout->setObjectName("actionLayout");
        manualActionLayout = new QVBoxLayout();
        manualActionLayout->setSpacing(6);
        manualActionLayout->setObjectName("manualActionLayout");
        manualMoveButton = new QPushButton(centralwidget);
        manualMoveButton->setObjectName("manualMoveButton");
        manualMoveButton->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        manualMoveButton->setCheckable(true);

        manualActionLayout->addWidget(manualMoveButton);

        manualCaptionLabel = new QLabel(centralwidget);
        manualCaptionLabel->setObjectName("manualCaptionLabel");
        manualCaptionLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        manualActionLayout->addWidget(manualCaptionLabel);


        actionLayout->addLayout(manualActionLayout);

        emergencyActionLayout = new QVBoxLayout();
        emergencyActionLayout->setSpacing(6);
        emergencyActionLayout->setObjectName("emergencyActionLayout");
        emergencyStopButton = new QPushButton(centralwidget);
        emergencyStopButton->setObjectName("emergencyStopButton");
        emergencyStopButton->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));

        emergencyActionLayout->addWidget(emergencyStopButton);

        emergencyCaptionLabel = new QLabel(centralwidget);
        emergencyCaptionLabel->setObjectName("emergencyCaptionLabel");
        emergencyCaptionLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        emergencyActionLayout->addWidget(emergencyCaptionLabel);


        actionLayout->addLayout(emergencyActionLayout);

        logActionLayout = new QVBoxLayout();
        logActionLayout->setSpacing(6);
        logActionLayout->setObjectName("logActionLayout");
        logViewButton = new QPushButton(centralwidget);
        logViewButton->setObjectName("logViewButton");
        logViewButton->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));

        logActionLayout->addWidget(logViewButton);

        logCaptionLabel = new QLabel(centralwidget);
        logCaptionLabel->setObjectName("logCaptionLabel");
        logCaptionLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        logActionLayout->addWidget(logCaptionLabel);


        actionLayout->addLayout(logActionLayout);


        rootLayout->addLayout(actionLayout);

        MainWindow->setCentralWidget(centralwidget);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "\354\225\210\353\202\264 \353\241\234\353\264\207 \352\264\200\354\240\234 \354\213\234\354\212\244\355\205\234", nullptr));
        titleLabel->setText(QCoreApplication::translate("MainWindow", "\354\225\210\353\202\264 \353\241\234\353\264\207 \352\264\200\354\240\234 \354\213\234\354\212\244\355\205\234", nullptr));
        subtitleLabel->setText(QCoreApplication::translate("MainWindow", "\354\271\264\353\251\224\353\235\274 \354\230\201\354\203\201 \355\231\225\354\235\270\352\263\274 \354\243\274\354\232\224 \352\270\260\353\212\245\354\235\204 \355\225\234 \355\231\224\353\251\264\354\227\220\354\204\234 \354\240\234\354\226\264\355\225\251\353\213\210\353\213\244.", nullptr));
        connectionDot->setText(QString());
        connectionText->setText(QCoreApplication::translate("MainWindow", "\354\213\234\354\212\244\355\205\234 \354\227\260\352\262\260\353\220\250", nullptr));
        statusPanelTitleLabel->setText(QCoreApplication::translate("MainWindow", "\353\241\234\353\264\207 \354\203\201\355\203\234", nullptr));
        statusPanelDescriptionLabel->setText(QCoreApplication::translate("MainWindow", "\355\230\204\354\236\254 \354\204\274\354\204\234 \353\260\217 \354\243\274\355\226\211 \354\240\225\353\263\264\353\245\274 \355\221\234\354\213\234\355\225\251\353\213\210\353\213\244.", nullptr));
        speedTitleLabel->setText(QCoreApplication::translate("MainWindow", "\354\206\215\353\217\204", nullptr));
        speedValueLabel->setText(QCoreApplication::translate("MainWindow", "0.0 m/s", nullptr));
        batteryTitleLabel->setText(QCoreApplication::translate("MainWindow", "\353\260\260\355\204\260\353\246\254", nullptr));
        batteryValueLabel->setText(QCoreApplication::translate("MainWindow", "100 %", nullptr));
        temperatureTitleLabel->setText(QCoreApplication::translate("MainWindow", "\354\230\250\353\217\204", nullptr));
        temperatureValueLabel->setText(QCoreApplication::translate("MainWindow", "-- \302\260C", nullptr));
        driveStatusTitleLabel->setText(QCoreApplication::translate("MainWindow", "\354\243\274\355\226\211 \354\203\201\355\203\234", nullptr));
        driveStatusValueLabel->setText(QCoreApplication::translate("MainWindow", "\353\214\200\352\270\260 \354\244\221", nullptr));
        cameraViewLabel->setText(QCoreApplication::translate("MainWindow", "\354\271\264\353\251\224\353\235\274 \354\212\244\355\212\270\353\246\274\354\235\204 \352\270\260\353\213\244\353\246\254\352\263\240 \354\236\210\354\212\265\353\213\210\353\213\244", nullptr));
        cameraHintLabel->setText(QCoreApplication::translate("MainWindow", "\354\204\234\353\262\204 \354\227\260\352\262\260 \355\233\204 \354\230\201\354\203\201\354\235\264 \354\236\220\353\217\231\354\234\274\353\241\234 \355\221\234\354\213\234\353\220\251\353\213\210\353\213\244.", nullptr));
        cameraSourceLabel->setText(QCoreApplication::translate("MainWindow", "VIDEO SOURCE  \302\267  RASPBERRY PI CAMERA", nullptr));
        manualMoveButton->setText(QCoreApplication::translate("MainWindow", "\342\206\224  \354\210\230\353\217\231 \354\235\264\353\217\231", nullptr));
        manualCaptionLabel->setText(QCoreApplication::translate("MainWindow", "\355\202\244\353\263\264\353\223\234 \353\230\220\353\212\224 \354\241\260\354\236\221 \355\231\224\353\251\264\354\234\274\353\241\234 \353\241\234\353\264\207\354\235\204 \354\247\201\354\240\221 \354\235\264\353\217\231", nullptr));
        emergencyStopButton->setText(QCoreApplication::translate("MainWindow", "\342\226\240  \352\270\264\352\270\211 \354\240\225\354\247\200", nullptr));
        emergencyCaptionLabel->setText(QCoreApplication::translate("MainWindow", "\354\243\274\355\226\211 \354\244\221\354\235\270 \353\241\234\353\264\207\354\235\204 \354\246\211\354\213\234 \354\240\225\354\247\200", nullptr));
        logViewButton->setText(QCoreApplication::translate("MainWindow", "\342\226\244  \352\270\260\353\241\235 \353\263\264\352\270\260", nullptr));
        logCaptionLabel->setText(QCoreApplication::translate("MainWindow", "\354\243\274\355\226\211 \352\270\260\353\241\235, \354\235\264\353\262\244\355\212\270, \352\262\275\352\263\240 \353\202\264\354\227\255 \355\231\225\354\235\270", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
