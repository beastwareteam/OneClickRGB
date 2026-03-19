/*---------------------------------------------------------*\
| OneClickRGB - Controller Factory Implementation           |
\*---------------------------------------------------------*/
#include "ControllerFactory.h"
#include "../controllers/AsusAuraUSBController.h"
#include "../controllers/SteelSeriesRivalController.h"
#include "../controllers/EVisionKeyboardController.h"
#include <hidapi.h>
#include <iostream>
#include <algorithm>
#include <map>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Singleton Instance                                        |
\*---------------------------------------------------------*/
ControllerFactory& ControllerFactory::Instance()
{
    static ControllerFactory instance;
    return instance;
}

/*---------------------------------------------------------*\
| Initialize Factory                                        |
\*---------------------------------------------------------*/
void ControllerFactory::Initialize()
{
    if (m_initialized) return;

    // Initialize HIDAPI
    hid_init();

    // Initialize device database
    DeviceDatabase::Instance().InitializeKnownDevices();

    m_initialized = true;
    std::cout << "[FACTORY] Initialized\n";
}

/*---------------------------------------------------------*\
| Helper: Convert wide string to UTF-8                      |
\*---------------------------------------------------------*/
static std::string WideToUTF8(const wchar_t* wstr)
{
    if (!wstr) return "";

    std::string result;
    while (*wstr) {
        if (*wstr < 0x80) {
            result += static_cast<char>(*wstr);
        } else if (*wstr < 0x800) {
            result += static_cast<char>(0xC0 | (*wstr >> 6));
            result += static_cast<char>(0x80 | (*wstr & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (*wstr >> 12));
            result += static_cast<char>(0x80 | ((*wstr >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (*wstr & 0x3F));
        }
        wstr++;
    }
    return result;
}

/*---------------------------------------------------------*\
| Scan for RGB Devices                                      |
| IMPORTANT: Must match EXACT usage_page for RGB control    |
\*---------------------------------------------------------*/
std::vector<DetectedDevice> ControllerFactory::ScanDevices()
{
    if (!m_initialized) {
        Initialize();
    }

    std::vector<DetectedDevice> detected;
    std::map<std::pair<uint16_t, uint16_t>, int> best_match;  // VID:PID -> best match score

    std::cout << "[SCANNER] Scanning for RGB devices...\n";

    // Enumerate all HID devices
    hid_device_info* devs = hid_enumerate(0, 0);
    hid_device_info* cur = devs;

    while (cur) {
        // Check if this VID:PID is in our database
        const DeviceDefinition* def = DeviceDatabase::Instance().FindDevice(
            cur->vendor_id,
            cur->product_id,
            -1,  // Don't filter by interface yet
            0    // Don't filter by usage page yet
        );

        if (def) {
            auto key = std::make_pair(cur->vendor_id, cur->product_id);

            // Calculate match score - higher = better
            int score = 0;

            // Usage page match is MOST important for RGB control
            if (def->usage_page != 0 && cur->usage_page == def->usage_page) {
                score += 100;  // Exact usage page match - REQUIRED for RGB
            } else if (cur->usage_page >= 0xFF00) {
                score += 10;   // Vendor-specific page (might work)
            } else {
                // Standard HID pages (keyboard, mouse input) - skip these!
                cur = cur->next;
                continue;
            }

            // Interface match
            if (def->interface_number >= 0 && cur->interface_number == def->interface_number) {
                score += 50;
            }

            // Check if this is a better match than what we have
            auto it = best_match.find(key);
            if (it == best_match.end() || score > it->second) {
                // Remove old entry if exists
                detected.erase(
                    std::remove_if(detected.begin(), detected.end(),
                        [&](const DetectedDevice& d) {
                            return d.vendor_id == cur->vendor_id &&
                                   d.product_id == cur->product_id;
                        }),
                    detected.end()
                );

                // Add new entry
                DetectedDevice device;
                device.path = cur->path;
                device.vendor_id = cur->vendor_id;
                device.product_id = cur->product_id;
                device.interface_number = cur->interface_number;
                device.usage_page = cur->usage_page;
                device.usage = cur->usage;
                device.product_name = WideToUTF8(cur->product_string);
                device.manufacturer = WideToUTF8(cur->manufacturer_string);
                device.definition = def;

                detected.push_back(device);
                best_match[key] = score;

                std::cout << "  [+] " << def->name
                          << " (" << std::hex << cur->vendor_id << ":" << cur->product_id << std::dec
                          << ") Interface " << cur->interface_number
                          << ", Usage 0x" << std::hex << cur->usage_page << std::dec << "\n";
            }
        }

        cur = cur->next;
    }

    hid_free_enumeration(devs);

    std::cout << "[SCANNER] Found " << detected.size() << " RGB device(s)\n";
    return detected;
}

/*---------------------------------------------------------*\
| Create Controller for Device                              |
\*---------------------------------------------------------*/
std::unique_ptr<IRGBController> ControllerFactory::CreateController(const DetectedDevice& device)
{
    if (!device.definition) {
        std::cerr << "[FACTORY] No definition for device\n";
        return nullptr;
    }

    const std::string& controller_class = device.definition->controller_class;

    // Create appropriate controller based on class name
    if (controller_class == "AsusAuraUSBController") {
        return std::make_unique<AsusAuraUSBController>();
    }
    else if (controller_class == "SteelSeriesRivalController") {
        return std::make_unique<SteelSeriesRivalController>();
    }
    else if (controller_class == "EVisionKeyboardController") {
        return std::make_unique<EVisionKeyboardController>();
    }

    std::cerr << "[FACTORY] Unknown controller class: " << controller_class << "\n";
    return nullptr;
}

/*---------------------------------------------------------*\
| Create and Open Controller                                |
\*---------------------------------------------------------*/
std::unique_ptr<IRGBController> ControllerFactory::CreateAndOpen(const DetectedDevice& device)
{
    auto controller = CreateController(device);
    if (!controller) {
        return nullptr;
    }

    if (!controller->Open(device.path)) {
        std::cerr << "[FACTORY] Failed to open device: " << device.path << "\n";
        return nullptr;
    }

    if (!controller->Initialize()) {
        std::cerr << "[FACTORY] Failed to initialize controller\n";
        return nullptr;
    }

    return controller;
}

/*---------------------------------------------------------*\
| Detect and Create All Controllers                         |
\*---------------------------------------------------------*/
std::vector<std::unique_ptr<IRGBController>> ControllerFactory::DetectAllControllers()
{
    std::vector<std::unique_ptr<IRGBController>> controllers;

    auto devices = ScanDevices();
    for (const auto& device : devices) {
        auto controller = CreateAndOpen(device);
        if (controller) {
            controllers.push_back(std::move(controller));
        }
    }

    std::cout << "[FACTORY] Created " << controllers.size() << " controller(s)\n";
    return controllers;
}

} // namespace OneClickRGB
