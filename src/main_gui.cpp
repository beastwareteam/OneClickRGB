/*---------------------------------------------------------*\
| main_gui.cpp                                              |
|                                                           |
| OneClickRGB GUI Application Entry Point                   |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QIcon>

#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application properties
    app.setApplicationName("OneClickRGB");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("OneClickRGB");
    app.setOrganizationDomain("oneclickrgb.com");

    // Set application icon
    app.setWindowIcon(QIcon(":/icons/app.png"));

    // Set a modern style
    app.setStyle(QStyleFactory::create("Fusion"));

    // Modern dark theme palette (optional)
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::black);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    // Uncomment to use dark theme
    // app.setPalette(darkPalette);

    try {
        // Create and show main window
        OneClickRGB::MainWindow window;
        window.initialize();
        window.show();

        return app.exec();

    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "Error",
            QString("Failed to start OneClickRGB GUI:\n%1").arg(e.what()));
        return 1;
    }
}
