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
| Controller Database Entry                                 |
| Maps VID/PID to the controller module to load             |
\*---------------------------------------------------------*/
struct ControllerEntry
{
    uint16_t        vendor_id;
    uint16_t        product_id;
    std::string     controller_name;        // e.g., "CorsairPeripheral"
    std::string     display_name;           // e.g., "Corsair K70 RGB"
    std::string     device_type;            // keyboard, mouse, strip, etc.

    // Optional: interface/usage filtering for HID
    int             interface_number = -1;  // -1 = any
    int             usage_page = -1;        // -1 = any
    int             usage = -1;             // -1 = any
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

    // Get controller info for detected hardware
    const ControllerEntry* GetControllerEntry(const DetectedHardware& hw) const;

    // Get list of hardware that matches known RGB controllers
    std::vector<std::pair<DetectedHardware, ControllerEntry>>
        GetMatchedDevices();

    /*-----------------------------------------------------*\
    | Database Management                                   |
    \*-----------------------------------------------------*/

    // Load controller database from JSON
    bool LoadControllerDatabase(const std::string& path);

    // Get statistics
    size_t GetKnownDeviceCount() const { return controller_database.size(); }

private:
    // VID/PID -> ControllerEntry mapping
    std::unordered_map<uint32_t, ControllerEntry> controller_database;

    // Helper to create lookup key
    static uint32_t MakeKey(uint16_t vid, uint16_t pid) {
        return (static_cast<uint32_t>(vid) << 16) | pid;
    }

    // Initialize built-in controller database
    void InitializeBuiltinDatabase();

    // Platform-specific scanning
    std::vector<DetectedHardware> ScanUSBHID_Windows();
    std::vector<DetectedHardware> ScanUSBHID_Linux();
    std::vector<DetectedHardware> ScanI2C_Windows();
    std::vector<DetectedHardware> ScanI2C_Linux();
};

} // namespace OneClickRGB
