/*---------------------------------------------------------*\
| main_gui.cpp                                              |
|                                                           |
| OneClickRGB GUI Entry Point                               |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include <QApplication>
#include "ui/MainWindow.h"
#include "core/OneClickRGB.h"

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    int argc = 0;
    char** argv = nullptr;
#else
int main(int argc, char *argv[])
{
#endif

    QApplication app(argc, argv);
    app.setApplicationName("OneClickRGB");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("OneClickRGB");

    // Initialize OneClickRGB core
    OneClickRGB::AppConfig config;
    config.auto_scan_devices = true;
    config.lazy_load_controllers = true;

    auto& rgbApp = OneClickRGB::Application::Get();
    rgbApp.Initialize(config);

    // Create and show main window
    OneClickRGB::MainWindow window;
    window.show();

    return app.exec();
}
