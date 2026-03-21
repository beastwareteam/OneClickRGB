/*---------------------------------------------------------*\
| MainWindow.cpp                                            |
|                                                           |
| OneClickRGB GUI Implementation                            |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "MainWindow.h"
#include "DeviceCard.h"
#include "QuickActions.h"
#include "../core/DeviceManager.h"
#include <QApplication>
#include <QCloseEvent>
#include <QMessageBox>
#include <QSettings>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QMenuBar>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QStyle>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>

namespace OneClickRGB {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , minimizeToTray_(true)
    , startMinimized_(false)
    , refreshInterval_(30)
{
    setWindowTitle("OneClickRGB v1.0.0");
    setWindowIcon(QIcon(":/icons/app.png"));
    setMinimumSize(800, 600);
    resize(1000, 700);

    loadSettings();
    setupUI();
    setupTrayIcon();
    setupMenuBar();
    setupStatusBar();

    // Initialize device manager
    deviceManager_ = std::make_unique<DeviceManager>();

    // Setup auto-refresh timer
    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &MainWindow::onAutoRefresh);
    if (refreshInterval_ > 0) {
        refreshTimer_->start(refreshInterval_ * 1000);
    }

    updateWindowTitle();
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::initialize()
{
    refreshDevices();

    if (startMinimized_) {
        hide();
        if (trayIcon_) {
            trayIcon_->showMessage("OneClickRGB",
                "Application started in system tray",
                QSystemTrayIcon::Information, 2000);
        }
    }
}

void MainWindow::setupUI()
{
    centralWidget_ = new QWidget(this);
    setCentralWidget(centralWidget_);

    mainLayout_ = new QVBoxLayout(centralWidget_);
    mainLayout_->setContentsMargins(10, 10, 10, 10);
    mainLayout_->setSpacing(10);

    contentLayout_ = new QHBoxLayout();
    contentLayout_->setSpacing(15);

    // Device list area
    deviceScrollArea_ = new QScrollArea(this);
    deviceScrollArea_->setWidgetResizable(true);
    deviceScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    deviceScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    deviceScrollArea_->setMinimumWidth(400);
    deviceScrollArea_->setMaximumWidth(600);

    deviceContainer_ = new QWidget();
    deviceLayout_ = new QVBoxLayout(deviceContainer_);
    deviceLayout_->setSpacing(10);
    deviceLayout_->setContentsMargins(10, 10, 10, 10);

    deviceScrollArea_->setWidget(deviceContainer_);
    contentLayout_->addWidget(deviceScrollArea_);

    // Quick actions panel
    quickActions_ = new QuickActions(this);
    connect(quickActions_, &QuickActions::colorChanged,
            this, &MainWindow::onDeviceDetected); // TODO: Connect to device manager
    contentLayout_->addWidget(quickActions_);

    mainLayout_->addLayout(contentLayout_);
}

void MainWindow::setupTrayIcon()
{
    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setIcon(QIcon(":/icons/tray.png"));
    trayIcon_->setToolTip("OneClickRGB - RGB Lighting Control");

    trayMenu_ = new QMenu(this);

    showAction_ = trayMenu_->addAction("Show Window");
    connect(showAction_, &QAction::triggered, this, &MainWindow::onShowWindow);

    trayMenu_->addSeparator();

    settingsAction_ = trayMenu_->addAction("Settings");
    connect(settingsAction_, &QAction::triggered, this, &MainWindow::onSettings);

    trayMenu_->addSeparator();

    aboutAction_ = trayMenu_->addAction("About");
    connect(aboutAction_, &QAction::triggered, this, &MainWindow::onAbout);

    quitAction_ = trayMenu_->addAction("Quit");
    connect(quitAction_, &QAction::triggered, this, &MainWindow::onQuit);

    trayIcon_->setContextMenu(trayMenu_);
    connect(trayIcon_, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayIconActivated);

    trayIcon_->show();
}

void MainWindow::setupMenuBar()
{
    QMenuBar* menuBar = this->menuBar();

    // File menu
    QMenu* fileMenu = menuBar->addMenu("&File");

    QAction* refreshAction = fileMenu->addAction("&Refresh Devices");
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshDevices);

    fileMenu->addSeparator();

    QAction* settingsAction = fileMenu->addAction("&Settings");
    settingsAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettings);

    fileMenu->addSeparator();

    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MainWindow::onQuit);

    // View menu
    QMenu* viewMenu = menuBar->addMenu("&View");

    QAction* minimizeAction = viewMenu->addAction("&Minimize to Tray");
    minimizeAction->setCheckable(true);
    minimizeAction->setChecked(minimizeToTray_);
    connect(minimizeAction, &QAction::toggled, [this](bool checked) {
        minimizeToTray_ = checked;
    });

    // Help menu
    QMenu* helpMenu = menuBar->addMenu("&Help");

    QAction* aboutAction = helpMenu->addAction("&About OneClickRGB");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    QAction* aboutQtAction = helpMenu->addAction("About &Qt");
    connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::setupStatusBar()
{
    statusBar();

    statusLabel_ = new QLabel("Ready");
    statusBar()->addWidget(statusLabel_);

    statusBar()->addPermanentWidget(new QLabel(" | "));

    deviceCountLabel_ = new QLabel("No devices detected");
    statusBar()->addPermanentWidget(deviceCountLabel_);
}

void MainWindow::refreshDevices()
{
    if (!deviceManager_) return;

    statusLabel_->setText("Scanning for devices...");

    // Clear existing device cards
    for (auto& card : deviceCards_) {
        deviceLayout_->removeWidget(card.get());
        card.reset();
    }
    deviceCards_.clear();

    // Scan for devices
    deviceManager_->QuickScan();
    auto devices = deviceManager_->GetDevices();

    // Create device cards
    for (auto* device : devices) {
        UIDeviceInfo info;
        info.name = device->GetName();
        info.vendor = device->GetVendor();
        info.type = "RGB Device"; // Simplified for now
        info.hardwareId = device->GetHardwareId();
        info.connected = device->IsConnected();

        auto deviceCard = std::make_unique<DeviceCard>(info, this);
        deviceLayout_->addWidget(deviceCard.get());
        deviceCards_.push_back(std::move(deviceCard));
    }

    // Update status
    deviceCountLabel_->setText(QString("Devices: %1").arg(devices.size()));
    statusLabel_->setText("Device scan complete");

    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    QString title = "OneClickRGB v1.0.0";
    if (deviceManager_) {
        size_t deviceCount = deviceManager_->GetDevices().size();
        if (deviceCount > 0) {
            title += QString(" - %1 device(s)").arg(deviceCount);
        }
    }
    setWindowTitle(title);
}

void MainWindow::loadSettings()
{
    QSettings settings("OneClickRGB", "GUI");

    minimizeToTray_ = settings.value("minimizeToTray", true).toBool();
    startMinimized_ = settings.value("startMinimized", false).toBool();
    refreshInterval_ = settings.value("refreshInterval", 30).toInt();

    // Restore window geometry
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::saveSettings()
{
    QSettings settings("OneClickRGB", "GUI");

    settings.setValue("minimizeToTray", minimizeToTray_);
    settings.setValue("startMinimized", startMinimized_);
    settings.setValue("refreshInterval", refreshInterval_);

    // Save window geometry
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (minimizeToTray_ && trayIcon_ && trayIcon_->isVisible()) {
        hide();
        event->ignore();
        if (trayIcon_) {
            trayIcon_->showMessage("OneClickRGB",
                "Application minimized to system tray",
                QSystemTrayIcon::Information, 2000);
        }
    } else {
        saveSettings();
        event->accept();
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized() && minimizeToTray_ && trayIcon_ && trayIcon_->isVisible()) {
            hide();
            event->ignore();
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::onDeviceDetected(const std::string& deviceName)
{
    statusLabel_->setText(QString("Device detected: %1").arg(QString::fromStdString(deviceName)));
    refreshDevices();
}

void MainWindow::onDeviceRemoved(const std::string& deviceName)
{
    statusLabel_->setText(QString("Device removed: %1").arg(QString::fromStdString(deviceName)));
    refreshDevices();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        onShowWindow();
    }
}

void MainWindow::onShowWindow()
{
    show();
    raise();
    activateWindow();
}

void MainWindow::onSettings()
{
    SettingsDialog dialog(this);
    dialog.setMinimizeToTray(minimizeToTray_);
    dialog.setStartMinimized(startMinimized_);
    dialog.setRefreshInterval(refreshInterval_);

    if (dialog.exec() == QDialog::Accepted) {
        minimizeToTray_ = dialog.getMinimizeToTray();
        startMinimized_ = dialog.getStartMinimized();
        refreshInterval_ = dialog.getRefreshInterval();

        // Update timer
        if (refreshInterval_ > 0) {
            refreshTimer_->start(refreshInterval_ * 1000);
        } else {
            refreshTimer_->stop();
        }

        saveSettings();
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "About OneClickRGB",
        "<h3>OneClickRGB v1.0.0</h3>"
        "<p>Professional RGB lighting control application</p>"
        "<p><b>Features:</b></p>"
        "<ul>"
        "<li>Modular device support</li>"
        "<li>Real-time device detection</li>"
        "<li>System tray integration</li>"
        "<li>Cross-platform compatibility</li>"
        "</ul>"
        "<p><b>Supported Devices:</b></p>"
        "<p>ASUS Aura, SteelSeries, Corsair, Razer, Logitech and more</p>"
        "<p><a href='https://github.com/beastwareteam/OneClickRGB'>GitHub Repository</a></p>"
    );
}

void MainWindow::onQuit()
{
    saveSettings();
    QApplication::quit();
}

void MainWindow::onAutoRefresh()
{
    if (isVisible()) {
        refreshDevices();
    }
}

} // namespace OneClickRGB

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
