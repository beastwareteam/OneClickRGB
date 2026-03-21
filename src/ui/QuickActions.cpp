/*---------------------------------------------------------*\
| QuickActions.cpp                                          |
|                                                           |
| Quick Actions Panel Implementation                        |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "QuickActions.h"
#include <QColorDialog>
#include <QApplication>

namespace OneClickRGB {

QuickActions::QuickActions(QWidget* parent)
    : QWidget(parent)
    , currentColor_(255, 255, 255)
    , currentBrightness_(100)
{
    setFixedWidth(280);
    setupUI();
}

void QuickActions::setupUI()
{
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(10, 10, 10, 10);
    mainLayout_->setSpacing(15);

    // Color section
    colorGroup_ = new QGroupBox("Color Control", this);
    colorLayout_ = new QVBoxLayout(colorGroup_);
    colorLayout_->setSpacing(8);

    customColorButton_ = new QPushButton("Choose Custom Color", this);
    customColorButton_->setMinimumHeight(35);
    connect(customColorButton_, &QPushButton::clicked, this, &QuickActions::onColorButtonClicked);
    colorLayout_->addWidget(customColorButton_);

    currentColorLabel_ = new QLabel("Current: RGB(255, 255, 255)", this);
    currentColorLabel_->setStyleSheet("font-family: monospace; font-size: 11px; color: #666;");
    colorLayout_->addWidget(currentColorLabel_);

    mainLayout_->addWidget(colorGroup_);

    // Quick colors
    QGroupBox* quickGroup = new QGroupBox("Quick Colors", this);
    quickColorsLayout_ = new QHBoxLayout(quickGroup);
    quickColorsLayout_->setSpacing(5);

    redButton_ = createColorButton("#FF0000", "Red");
    greenButton_ = createColorButton("#00FF00", "Green");
    blueButton_ = createColorButton("#0000FF", "Blue");
    whiteButton_ = createColorButton("#FFFFFF", "White");
    purpleButton_ = createColorButton("#9C27B0", "Purple");
    cyanButton_ = createColorButton("#00BCD4", "Cyan");
    yellowButton_ = createColorButton("#FFEB3B", "Yellow");
    orangeButton_ = createColorButton("#FF9800", "Orange");

    quickColorsLayout_->addWidget(redButton_);
    quickColorsLayout_->addWidget(greenButton_);
    quickColorsLayout_->addWidget(blueButton_);
    quickColorsLayout_->addWidget(whiteButton_);
    quickColorsLayout_->addWidget(purpleButton_);
    quickColorsLayout_->addWidget(cyanButton_);
    quickColorsLayout_->addWidget(yellowButton_);
    quickColorsLayout_->addWidget(orangeButton_);

    mainLayout_->addWidget(quickGroup);

    // Brightness section
    brightnessGroup_ = new QGroupBox("Brightness", this);
    brightnessLayout_ = new QVBoxLayout(brightnessGroup_);
    brightnessLayout_->setSpacing(8);

    brightnessSlider_ = new QSlider(Qt::Horizontal, this);
    brightnessSlider_->setRange(0, 100);
    brightnessSlider_->setValue(currentBrightness_);
    brightnessSlider_->setTickPosition(QSlider::TicksBelow);
    brightnessSlider_->setTickInterval(25);
    connect(brightnessSlider_, &QSlider::valueChanged, this, &QuickActions::onBrightnessChanged);
    brightnessLayout_->addWidget(brightnessSlider_);

    brightnessValueLabel_ = new QLabel("100%", this);
    brightnessValueLabel_->setAlignment(Qt::AlignCenter);
    brightnessValueLabel_->setStyleSheet("font-weight: bold;");
    brightnessLayout_->addWidget(brightnessValueLabel_);

    mainLayout_->addWidget(brightnessGroup_);

    // Effects section
    effectsGroup_ = new QGroupBox("Effects", this);
    effectsLayout_ = new QVBoxLayout(effectsGroup_);
    effectsLayout_->setSpacing(8);

    effectComboBox_ = new QComboBox(this);
    effectComboBox_->addItem("Static Color");
    effectComboBox_->addItem("Breathing");
    effectComboBox_->addItem("Rainbow Wave");
    effectComboBox_->addItem("Color Cycle");
    effectComboBox_->addItem("Strobe");
    effectComboBox_->addItem("Pulse");
    connect(effectComboBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QuickActions::onEffectSelected);
    effectsLayout_->addWidget(effectComboBox_);

    applyEffectButton_ = new QPushButton("Apply Effect", this);
    applyEffectButton_->setMinimumHeight(30);
    connect(applyEffectButton_, &QPushButton::clicked, [this]() {
        emit effectApplied(effectComboBox_->currentText());
    });
    effectsLayout_->addWidget(applyEffectButton_);

    mainLayout_->addWidget(effectsGroup_);

    // Global actions
    globalGroup_ = new QGroupBox("Global Actions", this);
    globalLayout_ = new QVBoxLayout(globalGroup_);
    globalLayout_->setSpacing(8);

    offButton_ = new QPushButton("Turn All OFF", this);
    offButton_->setMinimumHeight(35);
    offButton_->setStyleSheet("QPushButton { background-color: #333333; color: white; font-weight: bold; border: none; border-radius: 5px; }"
                              "QPushButton:hover { background-color: #555555; }");
    connect(offButton_, &QPushButton::clicked, [this]() {
        applyColorToAllDevices(RGBColor(0, 0, 0));
    });
    globalLayout_->addWidget(offButton_);

    applyToAllButton_ = new QPushButton("Apply Color to All", this);
    applyToAllButton_->setMinimumHeight(35);
    applyToAllButton_->setStyleSheet("QPushButton { background-color: #2196F3; color: white; font-weight: bold; border: none; border-radius: 5px; }"
                                    "QPushButton:hover { background-color: #1976D2; }");
    connect(applyToAllButton_, &QPushButton::clicked, this, &QuickActions::onApplyToAllClicked);
    globalLayout_->addWidget(applyToAllButton_);

    mainLayout_->addWidget(globalGroup_);

    mainLayout_->addStretch();

    // Set overall styling
    setStyleSheet(
        "QuickActions { background-color: #ffffff; }"
        "QGroupBox { font-weight: bold; border: 2px solid #e0e0e0; border-radius: 5px; margin-top: 1ex; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px 0 5px; }"
    );
}

QPushButton* QuickActions::createColorButton(const QString& color, const QString& tooltip)
{
    QPushButton* button = new QPushButton(this);
    button->setFixedSize(30, 30);
    button->setToolTip(tooltip);

    QString style = QString(
        "QPushButton { background-color: %1; border: 2px solid #ccc; border-radius: 15px; }"
        "QPushButton:hover { border-color: #007bff; }"
        "QPushButton:pressed { border-width: 3px; }"
    ).arg(color);

    button->setStyleSheet(style);
    connect(button, &QPushButton::clicked, [this, color]() {
        QColor qcolor(color);
        RGBColor rgbColor(qcolor.red(), qcolor.green(), qcolor.blue());
        applyColorToAllDevices(rgbColor);
    });

    return button;
}

void QuickActions::applyColorToAllDevices(const RGBColor& color)
{
    currentColor_ = color;
    currentColorLabel_->setText(QString("Current: RGB(%1, %2, %3)")
        .arg(color.r).arg(color.g).arg(color.b));

    // Update custom color button style
    QString buttonStyle = QString(
        "QPushButton { background-color: rgb(%1, %2, %3); color: %4; border: 2px solid #007bff; font-weight: bold; }"
    ).arg(color.r).arg(color.g).arg(color.b)
     .arg((color.r + color.g + color.b > 380) ? "black" : "white");

    customColorButton_->setStyleSheet(buttonStyle);

    emit colorChanged(color);
}

void QuickActions::onColorButtonClicked()
{
    QColor initialColor(currentColor_.r, currentColor_.g, currentColor_.b);
    QColor selectedColor = QColorDialog::getColor(initialColor, this, "Choose Color");

    if (selectedColor.isValid()) {
        RGBColor rgbColor(selectedColor.red(), selectedColor.green(), selectedColor.blue());
        applyColorToAllDevices(rgbColor);
    }
}

void QuickActions::onBrightnessChanged(int value)
{
    currentBrightness_ = value;
    brightnessValueLabel_->setText(QString("%1%").arg(value));
    emit brightnessChanged(value);
}

void QuickActions::onEffectSelected(int index)
{
    // Effect selection is handled when Apply Effect is clicked
    Q_UNUSED(index)
}

void QuickActions::onApplyToAllClicked()
{
    applyColorToAllDevices(currentColor_);
}

} // namespace OneClickRGB