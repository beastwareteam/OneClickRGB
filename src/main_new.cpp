/*---------------------------------------------------------*\
| OneClickRGB - Main Application (New Architecture)         |
| Standardized RGB controller with proper device support    |
\*---------------------------------------------------------*/
#include "core/ControllerFactory.h"
#include "core/IRGBController.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <algorithm>

using namespace OneClickRGB;

void PrintUsage(const char* program)
{
    std::cout << "OneClickRGB - RGB Controller\n";
    std::cout << "Usage:\n";
    std::cout << "  " << program << " scan              - Scan for devices\n";
    std::cout << "  " << program << " color R G B       - Set all devices to color (0-255)\n";
    std::cout << "  " << program << " mode N            - Set mode (0=Direct, 1=Static, etc.)\n";
    std::cout << "  " << program << " brightness N      - Set brightness (0-100)\n";
    std::cout << "  " << program << " info              - Show device information\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string command = argv[1];
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    // Initialize factory
    ControllerFactory& factory = ControllerFactory::Instance();
    factory.Initialize();

    if (command == "scan") {
        // Just scan and list devices
        auto devices = factory.ScanDevices();
        std::cout << "\nDetected " << devices.size() << " RGB device(s):\n";
        for (size_t i = 0; i < devices.size(); i++) {
            const auto& dev = devices[i];
            std::cout << "  [" << i << "] " << (dev.definition ? dev.definition->name : dev.product_name) << "\n";
            std::cout << "       VID:PID = " << std::hex << dev.vendor_id << ":" << dev.product_id << std::dec << "\n";
            std::cout << "       Interface " << dev.interface_number << ", Usage Page 0x" << std::hex << dev.usage_page << std::dec << "\n";
        }
        return 0;
    }

    if (command == "color" && argc >= 5) {
        uint8_t r = static_cast<uint8_t>(std::atoi(argv[2]));
        uint8_t g = static_cast<uint8_t>(std::atoi(argv[3]));
        uint8_t b = static_cast<uint8_t>(std::atoi(argv[4]));

        std::cout << "Setting color: RGB(" << (int)r << ", " << (int)g << ", " << (int)b << ")\n";

        auto controllers = factory.DetectAllControllers();
        for (auto& ctrl : controllers) {
            ctrl->SetAllLEDs(RGBColor(r, g, b));
            ctrl->Apply();
        }

        std::cout << "Done!\n";
        return 0;
    }

    if (command == "mode" && argc >= 3) {
        int mode = std::atoi(argv[2]);

        std::cout << "Setting mode: " << mode << "\n";

        auto controllers = factory.DetectAllControllers();
        for (auto& ctrl : controllers) {
            ctrl->SetMode(mode);
            ctrl->Apply();
        }

        std::cout << "Done!\n";
        return 0;
    }

    if (command == "brightness" && argc >= 3) {
        uint8_t brightness = static_cast<uint8_t>(std::atoi(argv[2]));

        std::cout << "Setting brightness: " << (int)brightness << "%\n";

        auto controllers = factory.DetectAllControllers();
        for (auto& ctrl : controllers) {
            ctrl->SetBrightness(brightness);
            ctrl->Apply();
        }

        std::cout << "Done!\n";
        return 0;
    }

    if (command == "info") {
        auto controllers = factory.DetectAllControllers();

        std::cout << "\nDevice Information:\n";
        std::cout << "==================\n";

        for (size_t i = 0; i < controllers.size(); i++) {
            const auto& ctrl = controllers[i];
            std::cout << "\n[" << i << "] " << ctrl->GetName() << "\n";
            std::cout << "    Vendor:     " << ctrl->GetVendor() << "\n";
            std::cout << "    Version:    " << ctrl->GetVersion() << "\n";
            std::cout << "    Location:   " << ctrl->GetLocation() << "\n";
            std::cout << "    LED Count:  " << ctrl->GetLEDCount() << "\n";
            std::cout << "    Zones:      " << ctrl->GetZones().size() << "\n";

            std::cout << "    Modes:\n";
            auto modes = ctrl->GetModes();
            for (size_t m = 0; m < modes.size(); m++) {
                std::cout << "      [" << m << "] " << modes[m].name;
                if (modes[m].has_color) std::cout << " (color)";
                if (modes[m].has_speed) std::cout << " (speed)";
                std::cout << "\n";
            }

            std::cout << "    Zones:\n";
            auto zones = ctrl->GetZones();
            for (const auto& zone : zones) {
                std::cout << "      - " << zone.name << " (" << zone.led_count << " LEDs)\n";
            }
        }

        return 0;
    }

    PrintUsage(argv[0]);
    return 1;
}
