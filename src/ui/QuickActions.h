#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QGroupBox>
#include <QColorDialog>
#include <QComboBox>
#include <QSpinBox>

namespace OneClickRGB {

struct RGBColor; // Forward declare color type used by signals

class QuickActions : public QWidget {
    Q_OBJECT

public:
    explicit QuickActions(QWidget* parent = nullptr);
    ~QuickActions() override = default;

signals:
    void colorChanged(const RGBColor& color);
    void brightnessChanged(int brightness);
    void effectApplied(const QString& effect);

private slots:
    void onColorButtonClicked();
    void onQuickColorClicked();
    void onBrightnessChanged(int value);
    void onEffectSelected(int index);
    void onApplyToAllClicked();

private:
    void setupUI();
    void applyColorToAllDevices(const RGBColor& color);

    // UI components
    QVBoxLayout* mainLayout_;

    // Color section
    QGroupBox* colorGroup_;
    QVBoxLayout* colorLayout_;
    QPushButton* customColorButton_;
    QLabel* currentColorLabel_;

    // Quick colors
    QHBoxLayout* quickColorsLayout_;
    QPushButton* redButton_;
    QPushButton* greenButton_;
    QPushButton* blueButton_;
    QPushButton* whiteButton_;
    QPushButton* purpleButton_;
    QPushButton* cyanButton_;
    QPushButton* yellowButton_;
    QPushButton* orangeButton_;

    // Brightness section
    QGroupBox* brightnessGroup_;
    QVBoxLayout* brightnessLayout_;
    QSlider* brightnessSlider_;
    QLabel* brightnessValueLabel_;

    // Effects section
    QGroupBox* effectsGroup_;
    QVBoxLayout* effectsLayout_;
    QComboBox* effectComboBox_;
    QPushButton* applyEffectButton_;

    // Global actions
    QGroupBox* globalGroup_;
    QVBoxLayout* globalLayout_;
    QPushButton* offButton_;
    QPushButton* applyToAllButton_;

    // Current state
    RGBColor currentColor_;
    int currentBrightness_;
};

} // namespace OneClickRGB