/*---------------------------------------------------------*\
| RGBDevice.h                                               |
|                                                           |
| Simplified RGB Device abstraction                         |
| Lazy-loaded controller wrapper                            |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <functional>
#include "../scanner/HardwareScanner.h"
#include "HIDController.h"

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Simple RGB Color                                          |
\*---------------------------------------------------------*/
struct RGBColor
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    RGBColor() = default;
    RGBColor(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}

    // Conversion helpers
    uint32_t ToUInt32() const { return (b << 16) | (g << 8) | r; }
    static RGBColor FromUInt32(uint32_t color) {
        return RGBColor(color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);
    }

    // Common colors
    static RGBColor Red()     { return RGBColor(255, 0, 0); }
    static RGBColor Green()   { return RGBColor(0, 255, 0); }
    static RGBColor Blue()    { return RGBColor(0, 0, 255); }
    static RGBColor White()   { return RGBColor(255, 255, 255); }
    static RGBColor Black()   { return RGBColor(0, 0, 0); }
    static RGBColor Cyan()    { return RGBColor(0, 255, 255); }
    static RGBColor Magenta() { return RGBColor(255, 0, 255); }
    static RGBColor Yellow()  { return RGBColor(255, 255, 0); }
    static RGBColor Orange()  { return RGBColor(255, 128, 0); }
};

/*---------------------------------------------------------*\
| Device Type Enum                                          |
\*---------------------------------------------------------*/
enum class DeviceType
{
    Keyboard,
    Mouse,
    Mousemat,
    Headset,
    Motherboard,
    GPU,
    RAM,
    LEDStrip,
    Fan,
    Cooler,
    Case,
    Other
};

/*---------------------------------------------------------*\
| Device Mode (simplified)                                  |
\*---------------------------------------------------------*/
struct DeviceMode
{
    std::string     name;
    bool            supports_color      = true;
    bool            supports_speed      = false;
    bool            supports_brightness = false;
    uint32_t        speed_min           = 0;
    uint32_t        speed_max           = 100;
    uint32_t        brightness_min      = 0;
    uint32_t        brightness_max      = 100;
};

/*---------------------------------------------------------*\
| Device Zone                                               |
\*---------------------------------------------------------*/
struct DeviceZone
{
    std::string     name;
    uint32_t        led_count = 1;
    bool            resizable = false;
};

/*---------------------------------------------------------*\
| RGBDevice Class                                           |
| Unified interface for all RGB hardware                    |
\*---------------------------------------------------------*/
class RGBDevice
{
public:
    RGBDevice(const DetectedHardware& hw, const ControllerEntry& entry);
    ~RGBDevice();

    /*-----------------------------------------------------*\
    | Device Information                                    |
    \*-----------------------------------------------------*/
    std::string     GetName() const { return display_name; }
    std::string     GetVendor() const { return vendor_name; }
    std::string     GetSerial() const { return serial_number; }
    std::string     GetLocation() const { return device_path; }
    DeviceType      GetType() const { return device_type; }
    std::string     GetHardwareId() const { return hardware_id; }

    bool            IsConnected() const { return is_connected; }
    bool            IsInitialized() const { return is_initialized; }

    /*-----------------------------------------------------*\
    | Lazy Initialization                                   |
    | Controller is only loaded when first needed           |
    \*-----------------------------------------------------*/
    bool            Initialize();
    void            Shutdown();

    /*-----------------------------------------------------*\
    | Color Control                                         |
    \*-----------------------------------------------------*/

    // Set all LEDs to one color
    void            SetAllLEDs(const RGBColor& color);

    // Set specific LED
    void            SetLED(uint32_t led_index, const RGBColor& color);

    // Set zone color
    void            SetZoneColor(uint32_t zone_index, const RGBColor& color);

    // Get current colors
    RGBColor        GetLED(uint32_t led_index) const;
    std::vector<RGBColor> GetAllColors() const;

    // Apply changes to hardware
    void            UpdateLEDs();

    // Turn off all LEDs
    void            TurnOff();

    /*-----------------------------------------------------*\
    | Mode Control                                          |
    \*-----------------------------------------------------*/
    std::vector<DeviceMode> GetModes() const;
    int             GetActiveMode() const { return active_mode; }
    void            SetMode(int mode_index);
    void            SetModeByName(const std::string& name);

    /*-----------------------------------------------------*\
    | Brightness Control                                    |
    \*-----------------------------------------------------*/
    void            SetBrightness(uint8_t brightness);
    uint8_t         GetBrightness() const { return current_brightness; }

    /*-----------------------------------------------------*\
    | Zone Information                                      |
    \*-----------------------------------------------------*/
    std::vector<DeviceZone> GetZones() const;
    uint32_t        GetTotalLEDCount() const;

    /*-----------------------------------------------------*\
    | State Persistence                                     |
    \*-----------------------------------------------------*/
    void            SaveToHardware();   // Persist to device memory

private:
    // Device identification
    std::string     display_name;
    std::string     vendor_name;
    std::string     controller_name;
    std::string     serial_number;
    std::string     device_path;
    std::string     hardware_id;
    DeviceType      device_type;

    // Device state
    bool            is_connected = false;
    bool            is_initialized = false;
    int             active_mode = 0;
    uint8_t         current_brightness = 100;

    // Color buffer (before applying to hardware)
    std::vector<RGBColor> color_buffer;

    // Zones and modes
    std::vector<DeviceZone> zones;
    std::vector<DeviceMode> modes;

    // Hardware info (for reconnection)
    uint16_t        vendor_id;
    uint16_t        product_id;

    // HID controller for hardware communication
    std::unique_ptr<HIDController> hid_controller;
    std::unique_ptr<RGBProtocol> protocol;

    // Helper to convert string type to enum
    static DeviceType StringToDeviceType(const std::string& type);

    // Platform-specific initialization
    bool            InitializeController();
    void            ShutdownController();
};

/*---------------------------------------------------------*\
| Device Factory                                            |
| Creates RGBDevice instances from detected hardware        |
\*---------------------------------------------------------*/
class DeviceFactory
{
public:
    static std::unique_ptr<RGBDevice> Create(
        const DetectedHardware& hw);
};

} // namespace OneClickRGB
