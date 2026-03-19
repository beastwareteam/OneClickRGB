/*---------------------------------------------------------*\
| OneClickRGB - Controller Factory                          |
| Automatic device detection and controller instantiation   |
\*---------------------------------------------------------*/
#pragma once

#include "IRGBController.h"
#include "DeviceDatabase.h"
#include <vector>
#include <memory>
#include <string>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Detected Device Info                                      |
\*---------------------------------------------------------*/
struct DetectedDevice {
    std::string             path;           // HID device path
    uint16_t                vendor_id;
    uint16_t                product_id;
    int                     interface_number;
    uint16_t                usage_page;
    uint16_t                usage;
    std::string             product_name;   // From HID enumeration
    std::string             manufacturer;
    const DeviceDefinition* definition;     // Matching database entry (nullptr if unknown)
};

/*---------------------------------------------------------*\
| ControllerFactory Class                                   |
\*---------------------------------------------------------*/
class ControllerFactory {
public:
    static ControllerFactory& Instance();

    // Initialize factory with device database
    void Initialize();

    // Scan for all connected RGB devices
    std::vector<DetectedDevice> ScanDevices();

    // Create controller for detected device
    std::unique_ptr<IRGBController> CreateController(const DetectedDevice& device);

    // Create and open controller
    std::unique_ptr<IRGBController> CreateAndOpen(const DetectedDevice& device);

    // Convenience: Scan and create all controllers
    std::vector<std::unique_ptr<IRGBController>> DetectAllControllers();

private:
    ControllerFactory() = default;
    bool m_initialized = false;
};

} // namespace OneClickRGB
