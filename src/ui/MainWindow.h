/*---------------------------------------------------------*\
| MainWindow.h                                              |
|                                                           |
| OneClickRGB GUI - Simple Qt Interface                     |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QSlider>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QColorDialog>
#include "../core/DeviceManager.h"

namespace OneClickRGB {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onRedClicked();
    void onGreenClicked();
    void onBlueClicked();
    void onWhiteClicked();
    void onOffClicked();
    void onCustomColorClicked();
    void onRefreshClicked();
    void onBrightnessChanged(int value);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupUI();
    void setupTrayIcon();
    void refreshDeviceList();
    void applyColorToAll(const RGBColor& color);

    // UI Elements
    QWidget* centralWidget;
    QListWidget* deviceList;
    QLabel* statusLabel;
    QSlider* brightnessSlider;

    // Quick Color Buttons
    QPushButton* btnRed;
    QPushButton* btnGreen;
    QPushButton* btnBlue;
    QPushButton* btnWhite;
    QPushButton* btnOff;
    QPushButton* btnCustom;
    QPushButton* btnRefresh;

    // System Tray
    QSystemTrayIcon* trayIcon;
    QMenu* trayMenu;

    // Current state
    RGBColor currentColor;
};

} // namespace OneClickRGB
