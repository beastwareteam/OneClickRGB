/*---------------------------------------------------------*\
| HIDController.h                                           |
|                                                           |
| Generic HID device communication using HIDAPI             |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <hidapi.h>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| HIDController Class                                       |
| Real HIDAPI implementation                                |
\*---------------------------------------------------------*/
class HIDController
{
public:
    HIDController();
    ~HIDController();

    bool Open(uint16_t vendor_id, uint16_t product_id, int interface_number = -1);
    bool Open(const std::string& device_path);
    void Close();

    bool IsOpen() const;

    // Write operations
    bool Write(const uint8_t* data, size_t length);
    bool Write(const std::vector<uint8_t>& data);
    bool SendFeatureReport(const uint8_t* data, size_t length);
    bool SendFeatureReport(const std::vector<uint8_t>& data);

    // Read operations
    int Read(uint8_t* data, size_t length, int timeout_ms = 100);
    std::vector<uint8_t> Read(size_t length, int timeout_ms = 100);
    int GetFeatureReport(uint8_t report_id, uint8_t* data, size_t length);

    // Device info
    std::string GetManufacturer() const;
    std::string GetProduct() const;
    std::string GetSerialNumber() const;
    std::string GetDevicePath() const { return device_path; }
    std::string GetLastError() const { return last_error; }

    // Direct access (for custom protocols)
    hid_device* GetHandle() { return dev; }

private:
    hid_device* dev = nullptr;
    std::string device_path;
    std::string last_error;
    bool is_open = false;

    std::string WideToString(const wchar_t* wstr) const;
};

/*---------------------------------------------------------*\
| RGBProtocol Base Class                                    |
\*---------------------------------------------------------*/
class RGBProtocol
{
public:
    virtual ~RGBProtocol() = default;

    virtual bool Initialize(HIDController& hid) { return true; }
    virtual bool SetAllColors(HIDController& hid, uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual bool SetLEDColor(HIDController& hid, int led_index, uint8_t r, uint8_t g, uint8_t b)
    {
        return SetAllColors(hid, r, g, b);
    }
    virtual bool SetMode(HIDController& hid, int mode_index) { return true; }
    virtual bool SetBrightness(HIDController& hid, uint8_t brightness) { return true; }
    virtual bool Apply(HIDController& hid) { return true; }

    virtual int GetLEDCount() const { return 1; }
    virtual bool SupportsPerLED() const { return false; }
    virtual bool SupportsBrightness() const { return true; }
};

/*---------------------------------------------------------*\
| Generic RGB Protocol (placeholder)                        |
\*---------------------------------------------------------*/
class GenericRGBProtocol : public RGBProtocol
{
public:
    bool SetAllColors(HIDController& hid, uint8_t r, uint8_t g, uint8_t b) override;
};

/*---------------------------------------------------------*\
| ASUS Aura Mainboard Protocol                              |
| Based on OpenRGB AsusAuraMainboardController              |
\*---------------------------------------------------------*/
class AsusAuraMainboardProtocol : public RGBProtocol
{
public:
    bool Initialize(HIDController& hid) override;
    bool SetAllColors(HIDController& hid, uint8_t r, uint8_t g, uint8_t b) override;
    bool SetLEDColor(HIDController& hid, int led_index, uint8_t r, uint8_t g, uint8_t b) override;
    bool SetMode(HIDController& hid, int mode_index) override;
    bool SetBrightness(HIDController& hid, uint8_t brightness) override;
    bool Apply(HIDController& hid) override;
    int GetLEDCount() const override { return num_leds; }
    bool SupportsPerLED() const override { return true; }

private:
    static constexpr uint8_t AURA_REQUEST_FIRMWARE_VERSION = 0x82;
    static constexpr uint8_t AURA_REQUEST_CONFIG_TABLE     = 0xB0;
    static constexpr uint8_t AURA_CONTROL_MODE_DIRECT      = 0x40;
    static constexpr uint8_t AURA_CONTROL_MODE_EFFECT      = 0x35;

    unsigned char config_table[60] = {0};
    char version[17] = {0};
    uint8_t num_leds = 0;
    uint8_t current_mode = 0;
    uint8_t current_brightness = 100;
    std::vector<uint8_t> led_colors; // R,G,B for each LED
};

/*---------------------------------------------------------*\
| SteelSeries Rival 600 Protocol                            |
| Based on OpenRGB SteelSeriesRivalController               |
\*---------------------------------------------------------*/
class SteelSeriesRival600Protocol : public RGBProtocol
{
public:
    bool Initialize(HIDController& hid) override;
    bool SetAllColors(HIDController& hid, uint8_t r, uint8_t g, uint8_t b) override;
    bool SetLEDColor(HIDController& hid, int led_index, uint8_t r, uint8_t g, uint8_t b) override;
    bool Apply(HIDController& hid) override;
    int GetLEDCount() const override { return 8; }
    bool SupportsPerLED() const override { return true; }

private:
    static constexpr int NUM_ZONES = 8;
    uint8_t zone_colors[NUM_ZONES][3] = {{0}};
};

/*---------------------------------------------------------*\
| EVision Keyboard Protocol                                 |
| Based on OpenRGB EVisionKeyboardController                |
\*---------------------------------------------------------*/
class EVisionKeyboardProtocol : public RGBProtocol
{
public:
    bool Initialize(HIDController& hid) override;
    bool SetAllColors(HIDController& hid, uint8_t r, uint8_t g, uint8_t b) override;
    bool SetMode(HIDController& hid, int mode_index) override;
    bool SetBrightness(HIDController& hid, uint8_t brightness) override;
    bool Apply(HIDController& hid) override;
    int GetLEDCount() const override { return 104; }
    bool SupportsBrightness() const override { return true; }

private:
    static constexpr uint8_t EVISION_CMD_BEGIN           = 0x01;
    static constexpr uint8_t EVISION_CMD_END             = 0x02;
    static constexpr uint8_t EVISION_CMD_SET_PARAMETER   = 0x06;
    static constexpr uint8_t EVISION_CMD_WRITE_COLOR     = 0x11;

    // EVision mode values (from OpenRGB)
    static constexpr uint8_t EVISION_MODE_STATIC         = 0x01;  // Static color
    static constexpr uint8_t EVISION_MODE_BREATHING      = 0x02;  // Breathing
    static constexpr uint8_t EVISION_MODE_SPECTRUM       = 0x03;  // Spectrum cycle
    static constexpr uint8_t EVISION_MODE_WAVE           = 0x04;  // Wave
    static constexpr uint8_t EVISION_MODE_REACTIVE       = 0x05;  // Reactive

    uint8_t current_mode = EVISION_MODE_STATIC;
    uint8_t current_brightness = 100;
    uint8_t current_r = 255, current_g = 0, current_b = 0;

    bool SendCommand(HIDController& hid, uint8_t cmd, const uint8_t* data, size_t len);
    void ComputeChecksum(unsigned char* usb_buf);
    uint8_t CalculateChecksum(const uint8_t* data, size_t len);
};

/*---------------------------------------------------------*\
| Protocol Factory                                          |
\*---------------------------------------------------------*/
class ProtocolFactory
{
public:
    static std::unique_ptr<RGBProtocol> Create(const std::string& controller_name);
};

} // namespace OneClickRGB
