/*---------------------------------------------------------*\
| RGBDevice.cpp                                             |
|                                                           |
| RGB Device implementation with real HID communication     |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "RGBDevice.h"
#include "../core/modules/ModuleManager.h"
#include "../core/DeviceRegistry.h"
#include <algorithm>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| RGBDevice Implementation                                  |
\*---------------------------------------------------------*/
RGBDevice::RGBDevice(const DetectedHardware& hw, const ControllerEntry& entry)
    : display_name(entry.display_name)
    , vendor_name(hw.manufacturer)
    , controller_name(entry.controller_name)
    , serial_number(hw.serial_number)
    , device_path(hw.device_path)
    , hardware_id(hw.GetHardwareId())
    , device_type(StringToDeviceType(entry.device_type))
    , vendor_id(hw.vendor_id)
    , product_id(hw.product_id)
    , is_connected(true)
{
    // Use product name from hardware if available
    if (!hw.product_name.empty())
    {
        display_name = hw.product_name;
    }

    // Initialize with a default zone
    DeviceZone default_zone;
    default_zone.name = "Main";
    default_zone.led_count = 1;
    zones.push_back(default_zone);

    // Initialize color buffer
    color_buffer.resize(1, RGBColor::Black());

    // Add default modes
    modes.push_back({"Direct", true, false, true, 0, 100, 0, 100});
    modes.push_back({"Static", true, false, true, 0, 100, 0, 100});
    modes.push_back({"Off", false, false, false, 0, 0, 0, 0});
}

RGBDevice::~RGBDevice()
{
    Shutdown();
}

bool RGBDevice::Initialize()
{
    if (is_initialized)
    {
        return true;
    }

    if (!InitializeController())
    {
        return false;
    }

    is_initialized = true;
    return true;
}

void RGBDevice::Shutdown()
{
    if (is_initialized)
    {
        ShutdownController();
        is_initialized = false;
    }
}

bool RGBDevice::InitializeController()
{
    // Create HID controller
    hid_controller = std::make_unique<HIDController>();

    // Open device by path (more reliable) or VID/PID
    bool opened = false;

    if (!device_path.empty())
    {
        opened = hid_controller->Open(device_path);
    }

    if (!opened)
    {
        opened = hid_controller->Open(vendor_id, product_id);
    }

    if (!opened)
    {
        hid_controller.reset();
        return false;
    }

    // Create protocol handler based on controller name
    protocol = ProtocolFactory::Create(controller_name);

    // Initialize protocol
    if (!protocol->Initialize(*hid_controller))
    {
        hid_controller->Close();
        hid_controller.reset();
        protocol.reset();
        return false;
    }

    return true;
}

void RGBDevice::ShutdownController()
{
    if (protocol)
    {
        protocol.reset();
    }

    if (hid_controller)
    {
        hid_controller->Close();
        hid_controller.reset();
    }
}

void RGBDevice::SetAllLEDs(const RGBColor& color)
{
    std::fill(color_buffer.begin(), color_buffer.end(), color);
}

void RGBDevice::SetLED(uint32_t led_index, const RGBColor& color)
{
    if (led_index < color_buffer.size())
    {
        color_buffer[led_index] = color;
    }
}

void RGBDevice::SetZoneColor(uint32_t zone_index, const RGBColor& color)
{
    if (zone_index >= zones.size())
    {
        return;
    }

    // Calculate LED range for this zone
    uint32_t start_led = 0;
    for (uint32_t i = 0; i < zone_index; i++)
    {
        start_led += zones[i].led_count;
    }

    uint32_t end_led = start_led + zones[zone_index].led_count;

    for (uint32_t i = start_led; i < end_led && i < color_buffer.size(); i++)
    {
        color_buffer[i] = color;
    }
}

RGBColor RGBDevice::GetLED(uint32_t led_index) const
{
    if (led_index < color_buffer.size())
    {
        return color_buffer[led_index];
    }
    return RGBColor::Black();
}

std::vector<RGBColor> RGBDevice::GetAllColors() const
{
    return color_buffer;
}

void RGBDevice::UpdateLEDs()
{
    // Auto-initialize if needed (lazy loading)
    if (!is_initialized)
    {
        if (!Initialize())
        {
            return;  // Failed to initialize
        }
    }

    // Send colors to hardware via protocol
    if (protocol && hid_controller && hid_controller->IsOpen())
    {
        // For now, use first color for all LEDs
        // TODO: Support per-LED colors for devices that support it
        if (!color_buffer.empty())
        {
            const RGBColor& color = color_buffer[0];
            protocol->SetAllColors(*hid_controller, color.r, color.g, color.b);
            protocol->Apply(*hid_controller);
        }
    }
}

void RGBDevice::TurnOff()
{
    SetAllLEDs(RGBColor::Black());
    UpdateLEDs();
}

std::vector<DeviceMode> RGBDevice::GetModes() const
{
    return modes;
}

void RGBDevice::SetMode(int mode_index)
{
    if (mode_index >= 0 && mode_index < static_cast<int>(modes.size()))
    {
        active_mode = mode_index;

        // Apply mode to hardware
        if (protocol && hid_controller && hid_controller->IsOpen())
        {
            protocol->SetMode(*hid_controller, mode_index);
        }
    }
}

void RGBDevice::SetModeByName(const std::string& name)
{
    for (size_t i = 0; i < modes.size(); i++)
    {
        if (modes[i].name == name)
        {
            SetMode(static_cast<int>(i));
            return;
        }
    }
}

void RGBDevice::SetBrightness(uint8_t brightness)
{
    current_brightness = brightness;

    // Apply brightness to hardware
    if (protocol && hid_controller && hid_controller->IsOpen())
    {
        protocol->SetBrightness(*hid_controller, brightness);
    }
}

std::vector<DeviceZone> RGBDevice::GetZones() const
{
    return zones;
}

uint32_t RGBDevice::GetTotalLEDCount() const
{
    uint32_t total = 0;
    for (const auto& zone : zones)
    {
        total += zone.led_count;
    }
    return total;
}

void RGBDevice::SaveToHardware()
{
    // Some devices support saving to onboard memory
    // This is device-specific and would need protocol support
    if (protocol && hid_controller && hid_controller->IsOpen())
    {
        // Protocol would implement SaveToHardware if supported
        UpdateLEDs();  // At minimum, ensure current state is applied
    }
}

DeviceType RGBDevice::StringToDeviceType(const std::string& type)
{
    if (type == "keyboard")     return DeviceType::Keyboard;
    if (type == "mouse")        return DeviceType::Mouse;
    if (type == "mousemat")     return DeviceType::Mousemat;
    if (type == "headset")      return DeviceType::Headset;
    if (type == "motherboard")  return DeviceType::Motherboard;
    if (type == "gpu")          return DeviceType::GPU;
    if (type == "ram")          return DeviceType::RAM;
    if (type == "ledstrip")     return DeviceType::LEDStrip;
    if (type == "fan")          return DeviceType::Fan;
    if (type == "cooler")       return DeviceType::Cooler;
    if (type == "case")         return DeviceType::Case;
    return DeviceType::Other;
}

/*---------------------------------------------------------*\
| DeviceFactory Implementation                              |
\*---------------------------------------------------------*/
std::unique_ptr<RGBDevice> DeviceFactory::Create(
    const DetectedHardware& hw)
{
    // Get device info from registry
    auto& registry = DeviceRegistry::getInstance();
    const DeviceInfo* deviceInfo = registry.findDevice(hw.vendor_id, hw.product_id);

    if (!deviceInfo) {
        return nullptr;
    }

    // Create controller using module system
    auto& moduleManager = ModuleManager::getInstance();
    auto controller = moduleManager.CreateController(*deviceInfo, hw.device_path);

    if (!controller) {
        return nullptr;
    }

    // For now, return the controller directly
    // TODO: Wrap in RGBDevice interface
    return controller;
}

} // namespace OneClickRGB
