#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QGroupBox>
#include <QColorDialog>
#include <memory>

#include <string>

namespace OneClickRGB {

struct RGBColor;
class RGBDevice;

// Simple device info struct to avoid including RGBDevice.h
struct UIDeviceInfo {
    std::string name;
    std::string vendor;
    std::string type;
    std::string hardwareId;
    bool connected;
};

class DeviceCard : public QWidget {
    Q_OBJECT

public:
    explicit DeviceCard(const UIDeviceInfo& info, QWidget* parent = nullptr);
    ~DeviceCard() override = default;

    const UIDeviceInfo& getDeviceInfo() const { return deviceInfo_; }

signals:
    void colorChanged(const RGBColor& color);
    void brightnessChanged(int brightness);
    void effectChanged(const QString& effect);

private slots:
    void onColorButtonClicked();
    void onBrightnessSliderChanged(int value);
    void onEffectButtonClicked();
    void onOffButtonClicked();
    void onUpdateDevice();

private:
    void setupUI();
    void updateColorDisplay();
    void applyColorToDevice(const RGBColor& color);

    UIDeviceInfo deviceInfo_;

    // UI components
    QGroupBox* groupBox_;
    QVBoxLayout* mainLayout_;

    // Device info
    QLabel* deviceNameLabel_;
    QLabel* deviceTypeLabel_;
    QLabel* statusLabel_;

    // Color controls
    QHBoxLayout* colorLayout_;
    QPushButton* colorButton_;
    QLabel* colorValueLabel_;

    // Brightness control
    QHBoxLayout* brightnessLayout_;
    QLabel* brightnessLabel_;
    QSlider* brightnessSlider_;
    QLabel* brightnessValueLabel_;

    // Quick actions
    QHBoxLayout* actionLayout_;
    QPushButton* redButton_;
    QPushButton* greenButton_;
    QPushButton* blueButton_;
    QPushButton* whiteButton_;
    QPushButton* offButton_;
    QPushButton* effectButton_;

    // Current state
    RGBColor currentColor_;
    int currentBrightness_;
};

} // namespace OneClickRGB