/*---------------------------------------------------------*\
| SettingsDialog.cpp                                        |
|                                                           |
| Settings Dialog Implementation                            |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "SettingsDialog.h"

namespace OneClickRGB {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Settings");
    setModal(true);
    setFixedSize(400, 300);
    setupUI();
}

void SettingsDialog::setupUI()
{
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(20, 20, 20, 20);
    mainLayout_->setSpacing(15);

    // General settings
    generalGroup_ = new QGroupBox("General", this);
    generalLayout_ = new QVBoxLayout(generalGroup_);
    generalLayout_->setSpacing(10);

    minimizeToTrayCheck_ = new QCheckBox("Minimize to system tray when closed", this);
    minimizeToTrayCheck_->setChecked(true);
    generalLayout_->addWidget(minimizeToTrayCheck_);

    startMinimizedCheck_ = new QCheckBox("Start minimized", this);
    startMinimizedCheck_->setChecked(false);
    generalLayout_->addWidget(startMinimizedCheck_);

    mainLayout_->addWidget(generalGroup_);

    // Device settings
    deviceGroup_ = new QGroupBox("Device Detection", this);
    deviceLayout_ = new QVBoxLayout(deviceGroup_);
    deviceLayout_->setSpacing(10);

    refreshLayout_ = new QHBoxLayout();
    refreshLayout_->setSpacing(10);

    refreshLabel_ = new QLabel("Auto-refresh interval:", this);
    refreshLayout_->addWidget(refreshLabel_);

    refreshIntervalSpin_ = new QSpinBox(this);
    refreshIntervalSpin_->setRange(5, 300);
    refreshIntervalSpin_->setValue(30);
    refreshIntervalSpin_->setSuffix(" seconds");
    refreshLayout_->addWidget(refreshIntervalSpin_);

    refreshLayout_->addStretch();
    deviceLayout_->addLayout(refreshLayout_);

    QLabel* refreshHelp = new QLabel("How often to automatically scan for new devices.\nSet to 0 to disable auto-refresh.", this);
    refreshHelp->setStyleSheet("color: #666; font-size: 11px;");
    refreshHelp->setWordWrap(true);
    deviceLayout_->addWidget(refreshHelp);

    mainLayout_->addWidget(deviceGroup_);

    mainLayout_->addStretch();

    // Dialog buttons
    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout_->addWidget(buttonBox_);

    // Set styling
    setStyleSheet(
        "QGroupBox { font-weight: bold; border: 2px solid #e0e0e0; border-radius: 5px; margin-top: 1ex; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px 0 5px; }"
        "QCheckBox { spacing: 8px; }"
        "QSpinBox { padding: 2px; }"
    );
}

} // namespace OneClickRGB