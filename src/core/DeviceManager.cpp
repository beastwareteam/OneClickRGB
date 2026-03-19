/*---------------------------------------------------------*\
| DeviceManager.cpp                                         |
|                                                           |
| Central device management implementation                  |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "DeviceManager.h"
#include <algorithm>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Singleton Instance                                        |
\*---------------------------------------------------------*/
DeviceManager& DeviceManager::Get()
{
    static DeviceManager instance;
    return instance;
}

DeviceManager::DeviceManager()
{
    status_message = "Ready";
}

DeviceManager::~DeviceManager()
{
    Shutdown();
}

/*---------------------------------------------------------*\
| Scanning and Initialization                               |
\*---------------------------------------------------------*/
void DeviceManager::QuickScan()
{
    if (is_scanning)
    {
        return;
    }

    is_scanning = true;
    status_message = "Scanning for RGB devices...";

    std::lock_guard<std::mutex> lock(devices_mutex);

    // Clear existing devices
    devices.clear();

    // Get matched hardware (only devices with known controllers)
    auto matched = scanner.GetMatchedDevices();

    status_message = "Found " + std::to_string(matched.size()) + " RGB devices";

    // Create device objects using module system
    for (const auto& [hw, deviceInfo] : matched)
    {
        auto device = DeviceFactory::Create(hw);
        if (device)
        {
            devices.push_back(std::move(device));
        }
    }

    is_scanning = false;
    NotifyDeviceChange();
}

void DeviceManager::InitializeAll()
{
    if (is_initializing)
    {
        return;
    }

    is_initializing = true;

    std::lock_guard<std::mutex> lock(devices_mutex);

    int initialized_count = 0;

    for (auto& device : devices)
    {
        status_message = "Initializing: " + device->GetName();

        if (device->Initialize())
        {
            initialized_count++;
        }
    }

    status_message = "Initialized " + std::to_string(initialized_count) +
                     " of " + std::to_string(devices.size()) + " devices";

    is_initializing = false;
    NotifyDeviceChange();
}

bool DeviceManager::InitializeDevice(size_t index)
{
    std::lock_guard<std::mutex> lock(devices_mutex);

    if (index >= devices.size())
    {
        return false;
    }

    return devices[index]->Initialize();
}

bool DeviceManager::InitializeDevice(const std::string& hardware_id)
{
    std::lock_guard<std::mutex> lock(devices_mutex);

    for (auto& device : devices)
    {
        if (device->GetHardwareId() == hardware_id)
        {
            return device->Initialize();
        }
    }

    return false;
}

void DeviceManager::Rescan()
{
    Shutdown();
    QuickScan();
}

/*---------------------------------------------------------*\
| Device Access                                             |
\*---------------------------------------------------------*/
std::vector<RGBDevice*> DeviceManager::GetDevices()
{
    std::lock_guard<std::mutex> lock(devices_mutex);

    std::vector<RGBDevice*> result;
    result.reserve(devices.size());

    for (auto& device : devices)
    {
        result.push_back(device.get());
    }

    return result;
}

std::vector<RGBDevice*> DeviceManager::GetInitializedDevices()
{
    std::lock_guard<std::mutex> lock(devices_mutex);

    std::vector<RGBDevice*> result;

    for (auto& device : devices)
    {
        if (device->IsInitialized())
        {
            result.push_back(device.get());
        }
    }

    return result;
}

RGBDevice* DeviceManager::GetDevice(size_t index)
{
    std::lock_guard<std::mutex> lock(devices_mutex);

    if (index >= devices.size())
    {
        return nullptr;
    }

    return devices[index].get();
}

RGBDevice* DeviceManager::GetDeviceByHardwareId(const std::string& hardware_id)
{
    std::lock_guard<std::mutex> lock(devices_mutex);

    for (auto& device : devices)
    {
        if (device->GetHardwareId() == hardware_id)
        {
            return device.get();
        }
    }

    return nullptr;
}

/*---------------------------------------------------------*\
| Bulk Operations                                           |
\*---------------------------------------------------------*/
void DeviceManager::SetAllDevicesColor(const RGBColor& color)
{
    std::lock_guard<std::mutex> lock(devices_mutex);

    for (auto& device : devices)
    {
        // Lazy initialize if needed
        if (!device->IsInitialized())
        {
            device->Initialize();
        }

        device->SetAllLEDs(color);
    }
}

void DeviceManager::TurnOffAll()
{
    std::lock_guard<std::mutex> lock(devices_mutex);

    for (auto& device : devices)
    {
        if (device->IsInitialized())
        {
            device->TurnOff();
        }
    }
}

void DeviceManager::SetAllBrightness(uint8_t brightness)
{
    std::lock_guard<std::mutex> lock(devices_mutex);

    for (auto& device : devices)
    {
        if (device->IsInitialized())
        {
            device->SetBrightness(brightness);
        }
    }
}

void DeviceManager::UpdateAll()
{
    std::lock_guard<std::mutex> lock(devices_mutex);

    for (auto& device : devices)
    {
        if (device->IsInitialized())
        {
            device->UpdateLEDs();
        }
    }
}

/*---------------------------------------------------------*\
| Callbacks                                                 |
\*---------------------------------------------------------*/
void DeviceManager::RegisterDeviceChangeCallback(DeviceChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(callbacks_mutex);
    change_callbacks.push_back(callback);
}

void DeviceManager::UnregisterDeviceChangeCallback(DeviceChangeCallback callback)
{
    // Note: This is a simplified implementation
    // In production, you'd want to use a more robust callback system
}

void DeviceManager::NotifyDeviceChange()
{
    std::lock_guard<std::mutex> lock(callbacks_mutex);

    for (auto& callback : change_callbacks)
    {
        if (callback)
        {
            callback();
        }
    }
}

/*---------------------------------------------------------*\
| Shutdown                                                  |
\*---------------------------------------------------------*/
void DeviceManager::Shutdown()
{
    std::lock_guard<std::mutex> lock(devices_mutex);

    for (auto& device : devices)
    {
        device->Shutdown();
    }

    devices.clear();
    status_message = "Shutdown complete";
}

} // namespace OneClickRGB
