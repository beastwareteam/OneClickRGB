/*---------------------------------------------------------*\
| MainWindow.h                                              |
|                                                           |
| OneClickRGB GUI - Professional Qt Interface               |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#pragma once

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <memory>
#include <vector>
#include <string>

namespace OneClickRGB {

// Forward declarations (avoid heavy includes in header)
class DeviceManager;
class DeviceCard;
class QuickActions;

// Simplified device info used by the GUI
struct UIDeviceInfo {
    std::string name;
    std::string vendor;
    std::string type;
    std::string hardwareId;
    bool connected;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void initialize();
    void refreshDevices();

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onDeviceDetected(const std::string& deviceName);
    void onDeviceRemoved(const std::string& deviceName);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowWindow();
    void onSettings();
    void onAbout();
    void onQuit();
    void onAutoRefresh();

private:
    void setupUI();
    void setupTrayIcon();
    void setupMenuBar();
    void setupStatusBar();
    void loadSettings();
    void saveSettings();
    void updateWindowTitle();

    // Core components
    std::unique_ptr<DeviceManager> deviceManager_;

    // UI components
    QWidget* centralWidget_;
    QVBoxLayout* mainLayout_;
    QHBoxLayout* contentLayout_;

    // Device list area
    QScrollArea* deviceScrollArea_;
    QWidget* deviceContainer_;
    QVBoxLayout* deviceLayout_;

    // Quick actions panel
    QuickActions* quickActions_;

    // Device cards
    std::vector<std::unique_ptr<DeviceCard>> deviceCards_;

    // Tray icon
    QSystemTrayIcon* trayIcon_;
    QMenu* trayMenu_;

    // Menu actions
    QAction* showAction_;
    QAction* settingsAction_;
    QAction* aboutAction_;
    QAction* quitAction_;

    // Auto-refresh timer
    QTimer* refreshTimer_;

    // Settings
    bool minimizeToTray_;
    bool startMinimized_;
    int refreshInterval_; // seconds

    // Status bar
    QLabel* statusLabel_;
    QLabel* deviceCountLabel_;
};

} // namespace OneClickRGB
