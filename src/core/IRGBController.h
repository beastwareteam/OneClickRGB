/*---------------------------------------------------------*\
| OneClickRGB - Standardized RGB Controller Interface       |
| Based on OpenRGB architecture patterns                    |
\*---------------------------------------------------------*/
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| RGB Color Structure                                       |
\*---------------------------------------------------------*/
struct RGBColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    RGBColor() = default;
    RGBColor(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}

    bool operator==(const RGBColor& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
};

/*---------------------------------------------------------*\
| Device Mode Structure                                     |
\*---------------------------------------------------------*/
struct DeviceMode {
    std::string name;           // Display name (e.g., "Static", "Breathing")
    uint8_t     value;          // Protocol value to send
    bool        has_color;      // Mode supports color setting
    bool        has_speed;      // Mode supports speed setting
    bool        has_brightness; // Mode supports brightness
    bool        has_direction;  // Mode supports direction
    uint8_t     color_count;    // Number of colors supported (0 = unlimited)
};

/*---------------------------------------------------------*\
| Device Zone Structure                                     |
\*---------------------------------------------------------*/
struct DeviceZone {
    std::string name;           // Zone name (e.g., "Logo", "Wheel", "Strip 1")
    uint16_t    led_count;      // Number of LEDs in this zone
    uint16_t    start_index;    // Starting LED index
};

/*---------------------------------------------------------*\
| Device Type Enumeration                                   |
\*---------------------------------------------------------*/
enum class DeviceType {
    Unknown,
    Motherboard,
    GPU,
    RAM,
    Keyboard,
    Mouse,
    Mousepad,
    Headset,
    LEDStrip,
    Fan,
    Cooler,
    Case,
    Light
};

/*---------------------------------------------------------*\
| HID Communication Parameters                              |
\*---------------------------------------------------------*/
struct HIDParams {
    uint16_t    vendor_id;
    uint16_t    product_id;
    int         interface_number;   // -1 = any
    uint16_t    usage_page;         // 0 = any
    uint16_t    usage;              // 0 = any
    size_t      packet_size;        // Typical: 65 for feature reports
    bool        use_feature_report; // true = feature report, false = write
    uint8_t     report_id;          // First byte of packet (0 = none)
};

/*---------------------------------------------------------*\
| IRGBController - Abstract Base Class                      |
| All device controllers must implement this interface      |
\*---------------------------------------------------------*/
class IRGBController {
public:
    virtual ~IRGBController() = default;

    //--- Device Information ---
    virtual std::string GetName() const = 0;
    virtual std::string GetVendor() const = 0;
    virtual std::string GetDescription() const = 0;
    virtual std::string GetVersion() const = 0;
    virtual std::string GetSerial() const = 0;
    virtual std::string GetLocation() const = 0;
    virtual DeviceType  GetType() const = 0;

    //--- HID Parameters ---
    virtual HIDParams   GetHIDParams() const = 0;

    //--- Connection ---
    virtual bool Open(const std::string& device_path) = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;

    //--- Initialization ---
    virtual bool Initialize() = 0;

    //--- Modes ---
    virtual std::vector<DeviceMode> GetModes() const = 0;
    virtual int  GetActiveMode() const = 0;
    virtual bool SetMode(int mode_index) = 0;

    //--- Zones ---
    virtual std::vector<DeviceZone> GetZones() const = 0;
    virtual int  GetLEDCount() const = 0;

    //--- Colors ---
    virtual RGBColor GetLEDColor(int led_index) const = 0;
    virtual bool SetLEDColor(int led_index, const RGBColor& color) = 0;
    virtual bool SetAllLEDs(const RGBColor& color) = 0;
    virtual bool SetZoneColor(int zone_index, const RGBColor& color) = 0;

    //--- Settings ---
    virtual uint8_t GetBrightness() const = 0;
    virtual bool SetBrightness(uint8_t brightness) = 0;  // 0-100
    virtual uint8_t GetSpeed() const = 0;
    virtual bool SetSpeed(uint8_t speed) = 0;            // 0-100

    //--- Apply Changes ---
    virtual bool Apply() = 0;

    //--- Save to Device Memory ---
    virtual bool SaveToDevice() { return false; }  // Optional

    //--- Direct Protocol Access (for debugging) ---
    virtual bool SendPacket(const uint8_t* data, size_t length) = 0;
    virtual int  ReceivePacket(uint8_t* buffer, size_t length, int timeout_ms = 100) = 0;
};

/*---------------------------------------------------------*\
| RGBControllerBase - Common Implementation                 |
| Provides default implementations for common functionality |
\*---------------------------------------------------------*/
class RGBControllerBase : public IRGBController {
protected:
    // Device state
    std::string             m_name;
    std::string             m_vendor;
    std::string             m_description;
    std::string             m_version;
    std::string             m_serial;
    std::string             m_location;
    DeviceType              m_type = DeviceType::Unknown;

    // HID state
    void*                   m_hid_device = nullptr;
    bool                    m_is_open = false;
    HIDParams               m_hid_params = {};

    // Modes and zones
    std::vector<DeviceMode> m_modes;
    std::vector<DeviceZone> m_zones;
    int                     m_active_mode = 0;

    // LED colors
    std::vector<RGBColor>   m_colors;

    // Settings
    uint8_t                 m_brightness = 100;
    uint8_t                 m_speed = 50;

public:
    virtual ~RGBControllerBase();

    // IRGBController implementation
    std::string GetName() const override { return m_name; }
    std::string GetVendor() const override { return m_vendor; }
    std::string GetDescription() const override { return m_description; }
    std::string GetVersion() const override { return m_version; }
    std::string GetSerial() const override { return m_serial; }
    std::string GetLocation() const override { return m_location; }
    DeviceType  GetType() const override { return m_type; }
    HIDParams   GetHIDParams() const override { return m_hid_params; }

    bool IsOpen() const override { return m_is_open; }

    std::vector<DeviceMode> GetModes() const override { return m_modes; }
    int  GetActiveMode() const override { return m_active_mode; }
    bool SetMode(int mode_index) override;

    std::vector<DeviceZone> GetZones() const override { return m_zones; }
    int  GetLEDCount() const override { return static_cast<int>(m_colors.size()); }

    RGBColor GetLEDColor(int led_index) const override;
    bool SetLEDColor(int led_index, const RGBColor& color) override;
    bool SetAllLEDs(const RGBColor& color) override;
    bool SetZoneColor(int zone_index, const RGBColor& color) override;

    uint8_t GetBrightness() const override { return m_brightness; }
    bool SetBrightness(uint8_t brightness) override;
    uint8_t GetSpeed() const override { return m_speed; }
    bool SetSpeed(uint8_t speed) override;

    // HID helpers
    bool Open(const std::string& device_path) override;
    void Close() override;
    bool SendPacket(const uint8_t* data, size_t length) override;
    int  ReceivePacket(uint8_t* buffer, size_t length, int timeout_ms = 100) override;

protected:
    // Helper for sending feature reports
    bool SendFeatureReport(const uint8_t* data, size_t length);
    int  GetFeatureReport(uint8_t report_id, uint8_t* buffer, size_t length);
};

} // namespace OneClickRGB
