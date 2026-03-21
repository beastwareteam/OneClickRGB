/*---------------------------------------------------------*\
| DeviceCard.cpp                                            |
|                                                           |
| Device Control Widget Implementation                      |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "DeviceCard.h"
#include <QColorDialog>
#include <QMessageBox>
#include <QApplication>
#include <QStyle>

// Forward declarations to avoid including RGBDevice.h
struct RGBColor {
    uint8_t r = 255, g = 255, b = 255;
    RGBColor(uint8_t red = 255, uint8_t green = 255, uint8_t blue = 255) : r(red), g(green), b(blue) {}
    static RGBColor Red() { return RGBColor(255, 0, 0); }
    static RGBColor Green() { return RGBColor(0, 255, 0); }
    static RGBColor Blue() { return RGBColor(0, 0, 255); }
};

namespace OneClickRGB {

DeviceCard::DeviceCard(const UIDeviceInfo& info, QWidget* parent)
    : QWidget(parent)
    , deviceInfo_(info)
    , currentColor_(255, 255, 255)
    , currentBrightness_(100)
{
    setFixedHeight(200);
    setMinimumWidth(350);

    setupUI();
    updateColorDisplay();
}

void DeviceCard::setupUI()
{
    setFixedHeight(200);
    setMinimumWidth(350);

    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(10, 10, 10, 10);
    mainLayout_->setSpacing(8);

    // Device info section
    QHBoxLayout* infoLayout = new QHBoxLayout();
    infoLayout->setSpacing(10);

    deviceNameLabel_ = new QLabel(QString::fromStdString(deviceInfo_.name), this);
    deviceNameLabel_->setStyleSheet("font-weight: bold; font-size: 14px;");
    infoLayout->addWidget(deviceNameLabel_);

    infoLayout->addStretch();

    deviceTypeLabel_ = new QLabel(QString::fromStdString(deviceInfo_.type), this);
    deviceTypeLabel_->setStyleSheet("color: #666; font-size: 12px;");
    infoLayout->addWidget(deviceTypeLabel_);

    mainLayout_->addLayout(infoLayout);

    // Status
    statusLabel_ = new QLabel(deviceInfo_.connected ? "Connected" : "Disconnected", this);
    statusLabel_->setStyleSheet(deviceInfo_.connected ? "color: #4CAF50; font-size: 11px;" : "color: #f44336; font-size: 11px;");
    mainLayout_->addWidget(statusLabel_);

    // Color control section
    QHBoxLayout* colorControlLayout = new QHBoxLayout();
    colorControlLayout->setSpacing(10);

    QLabel* colorLabel = new QLabel("Color:", this);
    colorControlLayout->addWidget(colorLabel);

    colorButton_ = new QPushButton("Choose Color", this);
    colorButton_->setFixedWidth(100);
    connect(colorButton_, &QPushButton::clicked, this, &DeviceCard::onColorButtonClicked);
    colorControlLayout->addWidget(colorButton_);

    colorValueLabel_ = new QLabel("RGB(255, 255, 255)", this);
    colorValueLabel_->setStyleSheet("font-family: monospace; font-size: 11px;");
    colorControlLayout->addWidget(colorValueLabel_);

    colorControlLayout->addStretch();
    mainLayout_->addLayout(colorControlLayout);

    // Brightness control
    brightnessLayout_ = new QHBoxLayout();
    brightnessLayout_->setSpacing(10);

    brightnessLabel_ = new QLabel("Brightness:", this);
    brightnessLayout_->addWidget(brightnessLabel_);

    brightnessSlider_ = new QSlider(Qt::Horizontal, this);
    brightnessSlider_->setRange(0, 100);
    brightnessSlider_->setValue(currentBrightness_);
    brightnessSlider_->setFixedWidth(150);
    connect(brightnessSlider_, &QSlider::valueChanged, this, &DeviceCard::onBrightnessSliderChanged);
    brightnessLayout_->addWidget(brightnessSlider_);

    brightnessValueLabel_ = new QLabel("100%", this);
    brightnessValueLabel_->setFixedWidth(40);
    brightnessLayout_->addWidget(brightnessValueLabel_);

    brightnessLayout_->addStretch();
    mainLayout_->addLayout(brightnessLayout);

    // Quick color buttons
    actionLayout_ = new QHBoxLayout();
    actionLayout_->setSpacing(5);

    redButton_ = new QPushButton("R", this);
    redButton_->setFixedSize(30, 30);
    redButton_->setStyleSheet("background-color: #FF0000; color: white; font-weight: bold; border: none; border-radius: 15px;");
    connect(redButton_, &QPushButton::clicked, [this]() {
        applyColorToDevice(RGBColor::Red());
    });
    actionLayout_->addWidget(redButton_);

    greenButton_ = new QPushButton("G", this);
    greenButton_->setFixedSize(30, 30);
    greenButton_->setStyleSheet("background-color: #00FF00; color: black; font-weight: bold; border: none; border-radius: 15px;");
    connect(greenButton_, &QPushButton::clicked, [this]() {
        applyColorToDevice(RGBColor::Green());
    });
    actionLayout_->addWidget(greenButton_);

    blueButton_ = new QPushButton("B", this);
    blueButton_->setFixedSize(30, 30);
    blueButton_->setStyleSheet("background-color: #0000FF; color: white; font-weight: bold; border: none; border-radius: 15px;");
    connect(blueButton_, &QPushButton::clicked, [this]() {
        applyColorToDevice(RGBColor::Blue());
    });
    actionLayout_->addWidget(blueButton_);

    whiteButton_ = new QPushButton("W", this);
    whiteButton_->setFixedSize(30, 30);
    whiteButton_->setStyleSheet("background-color: #FFFFFF; color: black; font-weight: bold; border: 1px solid #ccc; border-radius: 15px;");
    connect(whiteButton_, &QPushButton::clicked, [this]() {
        applyColorToDevice(RGBColor(255, 255, 255));
    });
    actionLayout_->addWidget(whiteButton_);

    actionLayout_->addSpacing(10);

    offButton_ = new QPushButton("OFF", this);
    offButton_->setFixedSize(40, 30);
    offButton_->setStyleSheet("background-color: #333333; color: white; font-weight: bold; border: none; border-radius: 5px;");
    connect(offButton_, &QPushButton::clicked, this, &DeviceCard::onOffButtonClicked);
    actionLayout_->addWidget(offButton_);

    actionLayout_->addSpacing(10);

    effectButton_ = new QPushButton("Effects", this);
    effectButton_->setFixedSize(60, 30);
    effectButton_->setStyleSheet("background-color: #9C27B0; color: white; font-weight: bold; border: none; border-radius: 5px;");
    connect(effectButton_, &QPushButton::clicked, this, &DeviceCard::onEffectButtonClicked);
    actionLayout_->addWidget(effectButton_);

    actionLayout_->addStretch();
    mainLayout_->addLayout(actionLayout_);

    // Set background and border
    setStyleSheet(
        "DeviceCard {"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 8px;"
        "}"
        "DeviceCard:hover {"
        "    border-color: #007bff;"
        "}"
    );
}

void DeviceCard::updateColorDisplay()
{
    colorValueLabel_->setText(QString("RGB(%1, %2, %3)")
        .arg(currentColor_.r)
        .arg(currentColor_.g)
        .arg(currentColor_.b));

    QString buttonStyle = QString(
        "background-color: rgb(%1, %2, %3); "
        "border: 2px solid #007bff; "
        "border-radius: 5px;"
    ).arg(currentColor_.r).arg(currentColor_.g).arg(currentColor_.b);

    colorButton_->setStyleSheet(buttonStyle);
}

void DeviceCard::applyColorToDevice(const RGBColor& color)
{
    currentColor_ = color;

    // For now, just update the UI and emit signal
    // Device control will be handled by the parent widget
    updateColorDisplay();
    emit colorChanged(color);

    statusLabel_->setText("Color selected");
    statusLabel_->setStyleSheet("color: #4CAF50; font-size: 11px;");
}

void DeviceCard::onColorButtonClicked()
{
    QColor initialColor(currentColor_.r, currentColor_.g, currentColor_.b);
    QColor selectedColor = QColorDialog::getColor(initialColor, this, "Choose Color");

    if (selectedColor.isValid()) {
        RGBColor rgbColor(selectedColor.red(), selectedColor.green(), selectedColor.blue());
        applyColorToDevice(rgbColor);
    }
}

void DeviceCard::onBrightnessSliderChanged(int value)
{
    currentBrightness_ = value;
    brightnessValueLabel_->setText(QString("%1%").arg(value));

    // For now, just emit signal
    // Device control will be handled by the parent widget
    emit brightnessChanged(value);
    statusLabel_->setText("Brightness set");
    statusLabel_->setStyleSheet("color: #4CAF50; font-size: 11px;");
}
}

void DeviceCard::onEffectButtonClicked()
{
    // TODO: Implement effect selection dialog
    QMessageBox::information(this, "Effects", "Effect selection will be implemented in a future update.");
}

void DeviceCard::onOffButtonClicked()
{
    applyColorToDevice(RGBColor(0, 0, 0));
}

void DeviceCard::onUpdateDevice()
{
    try {
        device_->Update();
        statusLabel_->setText("Device updated");
        statusLabel_->setStyleSheet("color: #4CAF50; font-size: 11px;");
    } catch (const std::exception& e) {
        statusLabel_->setText("Error: " + QString::fromStdString(e.what()));
        statusLabel_->setStyleSheet("color: #f44336; font-size: 11px;");
    }
}

} // namespace OneClickRGB