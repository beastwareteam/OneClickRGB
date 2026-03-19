/*---------------------------------------------------------*\
| test_standalone.cpp                                       |
|                                                           |
| Standalone test - simulates the app without HIDAPI        |
| For testing the architecture and flow                     |
|                                                           |
| Compile with: g++ -std=c++17 -o test_rgb test_standalone.cpp
\*---------------------------------------------------------*/

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

/*---------------------------------------------------------*\
| Simulated RGB Color                                       |
\*---------------------------------------------------------*/
struct RGBColor
{
    uint8_t r = 0, g = 0, b = 0;

    RGBColor() = default;
    RGBColor(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}

    static RGBColor Red()   { return {255, 0, 0}; }
    static RGBColor Green() { return {0, 255, 0}; }
    static RGBColor Blue()  { return {0, 0, 255}; }
    static RGBColor White() { return {255, 255, 255}; }
    static RGBColor Black() { return {0, 0, 0}; }
};

/*---------------------------------------------------------*\
| Simulated Device                                          |
\*---------------------------------------------------------*/
class SimulatedDevice
{
public:
    std::string name;
    std::string hardware_id;
    RGBColor current_color;
    bool initialized = false;

    SimulatedDevice(const std::string& n, const std::string& id)
        : name(n), hardware_id(id) {}

    bool Initialize()
    {
        std::cout << "  [HID] Opening device: " << name << " (" << hardware_id << ")\n";
        std::cout << "  [HID] Device opened successfully\n";
        initialized = true;
        return true;
    }

    void SetColor(const RGBColor& color)
    {
        if (!initialized) Initialize();

        current_color = color;
        std::cout << "  [HID] Sending color packet: RGB("
                  << (int)color.r << ", "
                  << (int)color.g << ", "
                  << (int)color.b << ") to " << name << "\n";
    }

    void TurnOff()
    {
        SetColor(RGBColor::Black());
    }
};

/*---------------------------------------------------------*\
| Simulated Hardware Scanner                                |
\*---------------------------------------------------------*/
class HardwareScanner
{
public:
    struct DetectedHW
    {
        uint16_t vid;
        uint16_t pid;
        std::string name;
        std::string type;
    };

    std::vector<DetectedHW> ScanUSB()
    {
        std::cout << "\n[SCANNER] Enumerating USB HID devices...\n";

        // Simulate finding some devices
        std::vector<DetectedHW> found;

        // Simulate: Check if known VID/PIDs are present
        // In real app, this would use HIDAPI

        std::cout << "[SCANNER] Checking VID:PID against controller database...\n";

        // Simulated detected devices (would come from actual USB scan)
        found.push_back({0x1B1C, 0x1B2D, "Corsair K95 RGB Platinum", "keyboard"});
        found.push_back({0x1532, 0x0084, "Razer DeathAdder V2", "mouse"});

        std::cout << "[SCANNER] Found " << found.size() << " matching RGB device(s)\n";

        return found;
    }
};

/*---------------------------------------------------------*\
| Simulated Device Manager                                  |
\*---------------------------------------------------------*/
class DeviceManager
{
public:
    std::vector<std::unique_ptr<SimulatedDevice>> devices;

    void QuickScan()
    {
        std::cout << "\n========================================\n";
        std::cout << "DEVICE SCAN (Hardware-First Approach)\n";
        std::cout << "========================================\n";

        HardwareScanner scanner;
        auto found = scanner.ScanUSB();

        std::cout << "\n[MANAGER] Creating device wrappers (lazy-load)...\n";

        for (const auto& hw : found)
        {
            char id[16];
            snprintf(id, sizeof(id), "%04X:%04X", hw.vid, hw.pid);

            auto device = std::make_unique<SimulatedDevice>(hw.name, id);
            std::cout << "  + " << hw.name << " [" << id << "] - NOT initialized (lazy)\n";
            devices.push_back(std::move(device));
        }

        std::cout << "[MANAGER] " << devices.size() << " device(s) ready for use\n";
    }

    void SetAllColor(const RGBColor& color)
    {
        std::cout << "\n[MANAGER] Setting color on all devices...\n";

        for (auto& device : devices)
        {
            device->SetColor(color);
        }

        std::cout << "[MANAGER] All devices updated!\n";
    }

    void TurnOffAll()
    {
        std::cout << "\n[MANAGER] Turning off all devices...\n";

        for (auto& device : devices)
        {
            device->TurnOff();
        }
    }

    void ListDevices()
    {
        std::cout << "\n========================================\n";
        std::cout << "DETECTED RGB DEVICES\n";
        std::cout << "========================================\n\n";

        if (devices.empty())
        {
            std::cout << "No devices found.\n";
            return;
        }

        int i = 0;
        for (const auto& device : devices)
        {
            std::cout << "[" << i++ << "] " << device->name << "\n";
            std::cout << "    Hardware ID: " << device->hardware_id << "\n";
            std::cout << "    Status: " << (device->initialized ? "READY" : "Not initialized (lazy)") << "\n";
            std::cout << "    Current: RGB("
                      << (int)device->current_color.r << ", "
                      << (int)device->current_color.g << ", "
                      << (int)device->current_color.b << ")\n\n";
        }
    }
};

/*---------------------------------------------------------*\
| Simulated Profile                                         |
\*---------------------------------------------------------*/
struct Profile
{
    std::string name;
    RGBColor color;
};

/*---------------------------------------------------------*\
| Main Test                                                 |
\*---------------------------------------------------------*/
int main(int argc, char* argv[])
{
    std::cout << "\n";
    std::cout << "################################################\n";
    std::cout << "#                                              #\n";
    std::cout << "#   OneClickRGB - Architecture Test            #\n";
    std::cout << "#   (Simulated Hardware Communication)         #\n";
    std::cout << "#                                              #\n";
    std::cout << "################################################\n";

    DeviceManager manager;

    std::string command = (argc > 1) ? argv[1] : "demo";

    if (command == "demo")
    {
        std::cout << "\n>>> Running full demo sequence...\n";

        // Step 1: Scan
        manager.QuickScan();

        // Step 2: List (devices not yet initialized)
        manager.ListDevices();

        // Step 3: Set color (triggers lazy initialization)
        std::cout << "\n>>> Setting all devices to RED (One-Click!)...\n";
        manager.SetAllColor(RGBColor::Red());

        // Step 4: List again (now initialized)
        manager.ListDevices();

        // Step 5: Change color
        std::cout << "\n>>> Changing to BLUE...\n";
        manager.SetAllColor(RGBColor::Blue());

        // Step 6: Turn off
        std::cout << "\n>>> Turning off all devices...\n";
        manager.TurnOffAll();

        manager.ListDevices();
    }
    else if (command == "scan")
    {
        manager.QuickScan();
        manager.ListDevices();
    }
    else if (command == "red")
    {
        manager.QuickScan();
        std::cout << "\n>>> ONE-CLICK RED!\n";
        manager.SetAllColor(RGBColor::Red());
    }
    else if (command == "green")
    {
        manager.QuickScan();
        std::cout << "\n>>> ONE-CLICK GREEN!\n";
        manager.SetAllColor(RGBColor::Green());
    }
    else if (command == "blue")
    {
        manager.QuickScan();
        std::cout << "\n>>> ONE-CLICK BLUE!\n";
        manager.SetAllColor(RGBColor::Blue());
    }
    else if (command == "off")
    {
        manager.QuickScan();
        std::cout << "\n>>> ONE-CLICK OFF!\n";
        manager.TurnOffAll();
    }
    else
    {
        std::cout << "\nUsage: test_rgb [demo|scan|red|green|blue|off]\n";
    }

    std::cout << "\n========================================\n";
    std::cout << "TEST COMPLETE\n";
    std::cout << "========================================\n\n";

    return 0;
}
