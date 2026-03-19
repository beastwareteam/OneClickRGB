/*---------------------------------------------------------*\
| HardwareScanner.h                                         |
|                                                           |
| Hardware-First Detection - Only scan for connected        |
| devices, no unnecessary module loading                    |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace OneClickRGB {

struct DeviceInfo; // Forward declaration

/*---------------------------------------------------------*\
| Detected Hardware Info (before loading any controller)    |
\*---------------------------------------------------------*/
struct DetectedHardware
{
    uint16_t        vendor_id;
    uint16_t        product_id;
    std::string     device_path;
    std::string     manufacturer;
    std::string     product_name;
    std::string     serial_number;

    enum class BusType {
        USB_HID,
        I2C_SMBUS,
        PCI,
        NETWORK
    } bus_type;

    // Unique identifier for matching with controller database
    std::string GetHardwareId() const;
};

/*---------------------------------------------------------*\
| HardwareScanner Class                                     |
| Scans system for RGB hardware WITHOUT loading controllers |
\*---------------------------------------------------------*/
class HardwareScanner
{
public:
    HardwareScanner();
    ~HardwareScanner();

    /*-----------------------------------------------------*\
    | Scan Methods - Fast hardware enumeration              |
    \*-----------------------------------------------------*/

    // Scan all supported bus types
    std::vector<DetectedHardware> ScanAll();

    // Scan specific bus types
    std::vector<DetectedHardware> ScanUSBHID();
    std::vector<DetectedHardware> ScanI2C();
    std::vector<DetectedHardware> ScanPCI();

    /*-----------------------------------------------------*\
    | Controller Matching                                   |
    \*-----------------------------------------------------*/

    // Check if hardware has a known RGB controller
    bool HasKnownController(const DetectedHardware& hw) const;

    // Get device info for detected hardware
    const DeviceInfo* GetDeviceInfo(const DetectedHardware& hw) const;

    // Get list of hardware that matches known RGB controllers
    std::vector<std::pair<DetectedHardware, const DeviceInfo*>>
        GetMatchedDevices();

    /*-----------------------------------------------------*\
    | Database Management                                   |
    \*-----------------------------------------------------*/

    // Load controller database from JSON
    bool LoadControllerDatabase(const std::string& path);

    // Get statistics
    size_t GetKnownDeviceCount() const;

private:
    // Platform-specific scanning
    std::vector<DetectedHardware> ScanUSBHID_Windows();
    std::vector<DetectedHardware> ScanUSBHID_Linux();
    std::vector<DetectedHardware> ScanI2C_Windows();
    std::vector<DetectedHardware> ScanI2C_Linux();
};

} // namespace OneClickRGB
