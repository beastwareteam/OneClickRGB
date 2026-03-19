/*---------------------------------------------------------*\
| DeviceManager.h                                           |
|                                                           |
| Central device management with lazy loading               |
| Only loads controllers for detected hardware              |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#pragma once

#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <atomic>
#include "../scanner/HardwareScanner.h"
#include "../devices/RGBDevice.h"

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Device Change Callback                                    |
\*---------------------------------------------------------*/
using DeviceChangeCallback = std::function<void()>;

/*---------------------------------------------------------*\
| DeviceManager Class                                       |
| Singleton for managing all RGB devices                    |
\*---------------------------------------------------------*/
class DeviceManager
{
public:
    static DeviceManager& Get();

    // Prevent copying
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

    /*-----------------------------------------------------*\
    | Initialization                                        |
    \*-----------------------------------------------------*/

    // Quick scan - only enumerate hardware, don't initialize
    void QuickScan();

    // Full initialization of all detected devices
    void InitializeAll();

    // Initialize specific device
    bool InitializeDevice(size_t index);
    bool InitializeDevice(const std::string& hardware_id);

    // Rescan for new devices
    void Rescan();

    /*-----------------------------------------------------*\
    | Device Access                                         |
    \*-----------------------------------------------------*/

    // Get all detected devices (may not be initialized)
    std::vector<RGBDevice*> GetDevices();

    // Get initialized devices only
    std::vector<RGBDevice*> GetInitializedDevices();

    // Get device by index
    RGBDevice* GetDevice(size_t index);

    // Get device by hardware ID
    RGBDevice* GetDeviceByHardwareId(const std::string& hardware_id);

    // Get device count
    size_t GetDeviceCount() const { return devices.size(); }

    /*-----------------------------------------------------*\
    | Bulk Operations - One Click Actions                   |
    \*-----------------------------------------------------*/

    // Set all devices to one color
    void SetAllDevicesColor(const RGBColor& color);

    // Turn off all devices
    void TurnOffAll();

    // Set brightness for all devices
    void SetAllBrightness(uint8_t brightness);

    // Apply changes to all devices
    void UpdateAll();

    /*-----------------------------------------------------*\
    | Quick Colors                                          |
    \*-----------------------------------------------------*/
    void QuickRed()     { SetAllDevicesColor(RGBColor::Red()); UpdateAll(); }
    void QuickGreen()   { SetAllDevicesColor(RGBColor::Green()); UpdateAll(); }
    void QuickBlue()    { SetAllDevicesColor(RGBColor::Blue()); UpdateAll(); }
    void QuickWhite()   { SetAllDevicesColor(RGBColor::White()); UpdateAll(); }
    void QuickCyan()    { SetAllDevicesColor(RGBColor::Cyan()); UpdateAll(); }
    void QuickMagenta() { SetAllDevicesColor(RGBColor::Magenta()); UpdateAll(); }
    void QuickYellow()  { SetAllDevicesColor(RGBColor::Yellow()); UpdateAll(); }
    void QuickOrange()  { SetAllDevicesColor(RGBColor::Orange()); UpdateAll(); }
    void QuickOff()     { TurnOffAll(); }

    /*-----------------------------------------------------*\
    | Callbacks                                             |
    \*-----------------------------------------------------*/
    void RegisterDeviceChangeCallback(DeviceChangeCallback callback);
    void UnregisterDeviceChangeCallback(DeviceChangeCallback callback);

    /*-----------------------------------------------------*\
    | Status                                                |
    \*-----------------------------------------------------*/
    bool IsScanning() const { return is_scanning; }
    bool IsInitializing() const { return is_initializing; }
    std::string GetStatusMessage() const { return status_message; }

    /*-----------------------------------------------------*\
    | Shutdown                                              |
    \*-----------------------------------------------------*/
    void Shutdown();

private:
    DeviceManager();
    ~DeviceManager();

    // Hardware scanner
    HardwareScanner scanner;

    // Device storage
    std::vector<std::unique_ptr<RGBDevice>> devices;
    std::mutex devices_mutex;

    // Status
    std::atomic<bool> is_scanning{false};
    std::atomic<bool> is_initializing{false};
    std::string status_message;

    // Callbacks
    std::vector<DeviceChangeCallback> change_callbacks;
    std::mutex callbacks_mutex;

    // Notify callbacks
    void NotifyDeviceChange();
};

} // namespace OneClickRGB
