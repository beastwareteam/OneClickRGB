#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QGroupBox>
#include <QDialogButtonBox>

namespace OneClickRGB {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    void setMinimizeToTray(bool minimize) { minimizeToTrayCheck_->setChecked(minimize); }
    bool getMinimizeToTray() const { return minimizeToTrayCheck_->isChecked(); }

    void setStartMinimized(bool startMinimized) { startMinimizedCheck_->setChecked(startMinimized); }
    bool getStartMinimized() const { return startMinimizedCheck_->isChecked(); }

    void setRefreshInterval(int seconds) { refreshIntervalSpin_->setValue(seconds); }
    int getRefreshInterval() const { return refreshIntervalSpin_->value(); }

private:
    void setupUI();

    // UI components
    QVBoxLayout* mainLayout_;

    // General settings
    QGroupBox* generalGroup_;
    QVBoxLayout* generalLayout_;
    QCheckBox* minimizeToTrayCheck_;
    QCheckBox* startMinimizedCheck_;

    // Device settings
    QGroupBox* deviceGroup_;
    QVBoxLayout* deviceLayout_;
    QHBoxLayout* refreshLayout_;
    QLabel* refreshLabel_;
    QSpinBox* refreshIntervalSpin_;
    QLabel* refreshUnitLabel_;

    // Dialog buttons
    QDialogButtonBox* buttonBox_;
};

} // namespace OneClickRGB