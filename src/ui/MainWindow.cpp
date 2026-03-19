/*---------------------------------------------------------*\
| MainWindow.cpp                                            |
|                                                           |
| OneClickRGB GUI Implementation                            |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "MainWindow.h"
#include <QApplication>
#include <QStyle>
#include <QGroupBox>
#include <QMessageBox>

namespace OneClickRGB {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , currentColor(255, 255, 255)
{
    setWindowTitle("OneClickRGB");
    setMinimumSize(400, 500);
    resize(450, 550);

    setupUI();
    setupTrayIcon();
    refreshDeviceList();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Title
    QLabel* titleLabel = new QLabel("OneClickRGB", this);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #2196F3;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Quick Colors Group
    QGroupBox* colorGroup = new QGroupBox("Quick Colors", this);
    QGridLayout* colorLayout = new QGridLayout(colorGroup);
    colorLayout->setSpacing(10);

    btnRed = new QPushButton("RED", this);
    btnRed->setStyleSheet("background-color: #FF0000; color: white; font-weight: bold; padding: 15px;");
    btnRed->setMinimumHeight(50);
    connect(btnRed, &QPushButton::clicked, this, &MainWindow::onRedClicked);

    btnGreen = new QPushButton("GREEN", this);
    btnGreen->setStyleSheet("background-color: #00FF00; color: black; font-weight: bold; padding: 15px;");
    btnGreen->setMinimumHeight(50);
    connect(btnGreen, &QPushButton::clicked, this, &MainWindow::onGreenClicked);

    btnBlue = new QPushButton("BLUE", this);
    btnBlue->setStyleSheet("background-color: #0000FF; color: white; font-weight: bold; padding: 15px;");
    btnBlue->setMinimumHeight(50);
    connect(btnBlue, &QPushButton::clicked, this, &MainWindow::onBlueClicked);

    btnWhite = new QPushButton("WHITE", this);
    btnWhite->setStyleSheet("background-color: #FFFFFF; color: black; font-weight: bold; padding: 15px; border: 1px solid #ccc;");
    btnWhite->setMinimumHeight(50);
    connect(btnWhite, &QPushButton::clicked, this, &MainWindow::onWhiteClicked);

    btnCustom = new QPushButton("CUSTOM...", this);
    btnCustom->setStyleSheet("background-color: #9C27B0; color: white; font-weight: bold; padding: 15px;");
    btnCustom->setMinimumHeight(50);
    connect(btnCustom, &QPushButton::clicked, this, &MainWindow::onCustomColorClicked);

    btnOff = new QPushButton("OFF", this);
    btnOff->setStyleSheet("background-color: #333333; color: white; font-weight: bold; padding: 15px;");
    btnOff->setMinimumHeight(50);
    connect(btnOff, &QPushButton::clicked, this, &MainWindow::onOffClicked);

    colorLayout->addWidget(btnRed, 0, 0);
    colorLayout->addWidget(btnGreen, 0, 1);
    colorLayout->addWidget(btnBlue, 0, 2);
    colorLayout->addWidget(btnWhite, 1, 0);
    colorLayout->addWidget(btnCustom, 1, 1);
    colorLayout->addWidget(btnOff, 1, 2);

    mainLayout->addWidget(colorGroup);

    // Brightness Slider
    QGroupBox* brightnessGroup = new QGroupBox("Brightness", this);
    QHBoxLayout* brightnessLayout = new QHBoxLayout(brightnessGroup);

    QLabel* dimLabel = new QLabel("Dim", this);
    brightnessSlider = new QSlider(Qt::Horizontal, this);
    brightnessSlider->setRange(0, 100);
    brightnessSlider->setValue(100);
    connect(brightnessSlider, &QSlider::valueChanged, this, &MainWindow::onBrightnessChanged);
    QLabel* brightLabel = new QLabel("Bright", this);

    brightnessLayout->addWidget(dimLabel);
    brightnessLayout->addWidget(brightnessSlider);
    brightnessLayout->addWidget(brightLabel);

    mainLayout->addWidget(brightnessGroup);

    // Device List
    QGroupBox* deviceGroup = new QGroupBox("Detected Devices", this);
    QVBoxLayout* deviceLayout = new QVBoxLayout(deviceGroup);

    deviceList = new QListWidget(this);
    deviceList->setMinimumHeight(120);
    deviceLayout->addWidget(deviceList);

    btnRefresh = new QPushButton("Refresh Devices", this);
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    deviceLayout->addWidget(btnRefresh);

    mainLayout->addWidget(deviceGroup);

    // Status
    statusLabel = new QLabel("Ready", this);
    statusLabel->setStyleSheet("color: #666; font-style: italic;");
    statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(statusLabel);

    mainLayout->addStretch();
}

void MainWindow::setupTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    trayIcon->setToolTip("OneClickRGB");

    trayMenu = new QMenu(this);
    trayMenu->addAction("Red", this, &MainWindow::onRedClicked);
    trayMenu->addAction("Green", this, &MainWindow::onGreenClicked);
    trayMenu->addAction("Blue", this, &MainWindow::onBlueClicked);
    trayMenu->addAction("White", this, &MainWindow::onWhiteClicked);
    trayMenu->addSeparator();
    trayMenu->addAction("Off", this, &MainWindow::onOffClicked);
    trayMenu->addSeparator();
    trayMenu->addAction("Show", this, &MainWindow::show);
    trayMenu->addAction("Quit", qApp, &QApplication::quit);

    trayIcon->setContextMenu(trayMenu);
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);

    trayIcon->show();
}

void MainWindow::refreshDeviceList()
{
    deviceList->clear();

    auto& manager = DeviceManager::Get();
    manager.QuickScan();

    auto devices = manager.GetDevices();

    for (auto* device : devices)
    {
        QString item = QString::fromStdString(device->GetName());
        item += " [" + QString::fromStdString(device->GetHardwareId()) + "]";
        deviceList->addItem(item);
    }

    statusLabel->setText(QString("Found %1 device(s)").arg(devices.size()));
}

void MainWindow::applyColorToAll(const RGBColor& color)
{
    currentColor = color;

    auto& manager = DeviceManager::Get();
    manager.SetAllDevicesColor(color);
    manager.UpdateAll();

    statusLabel->setText(QString("Applied RGB(%1, %2, %3)")
        .arg(color.r).arg(color.g).arg(color.b));
}

void MainWindow::onRedClicked()
{
    applyColorToAll(RGBColor::Red());
}

void MainWindow::onGreenClicked()
{
    applyColorToAll(RGBColor::Green());
}

void MainWindow::onBlueClicked()
{
    applyColorToAll(RGBColor::Blue());
}

void MainWindow::onWhiteClicked()
{
    applyColorToAll(RGBColor::White());
}

void MainWindow::onOffClicked()
{
    applyColorToAll(RGBColor::Black());
    statusLabel->setText("All devices OFF");
}

void MainWindow::onCustomColorClicked()
{
    QColor initial(currentColor.r, currentColor.g, currentColor.b);
    QColor color = QColorDialog::getColor(initial, this, "Select Color");

    if (color.isValid())
    {
        applyColorToAll(RGBColor(color.red(), color.green(), color.blue()));
    }
}

void MainWindow::onRefreshClicked()
{
    statusLabel->setText("Scanning...");
    refreshDeviceList();
}

void MainWindow::onBrightnessChanged(int value)
{
    auto& manager = DeviceManager::Get();
    manager.SetAllBrightness(static_cast<uint8_t>(value));

    statusLabel->setText(QString("Brightness: %1%").arg(value));
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
    {
        show();
        raise();
        activateWindow();
    }
}

} // namespace OneClickRGB
