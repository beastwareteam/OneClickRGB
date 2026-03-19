#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace OneClickRGB {

struct DeviceInfo {
    std::string name;
    std::string manufacturer;
    uint16_t vendorId;
    uint16_t productId;
    std::string deviceType; // "motherboard", "keyboard", "mouse", "gpu", etc.
    std::string protocol;   // "aura", "steelseries", "evision", etc.
    std::string controllerClass; // "AsusAuraMainboardController", etc.
    std::vector<std::string> supportedFeatures;
    bool requiresModule = false;
    std::string moduleName; // if requiresModule is true

    // Constructor for convenience
    DeviceInfo(std::string n, std::string m, uint16_t vid, uint16_t pid,
               std::string type, std::string prot, std::string ctrlClass,
               std::vector<std::string> features = {},
               bool reqModule = false, std::string modName = "")
        : name(std::move(n)), manufacturer(std::move(m)), vendorId(vid), productId(pid),
          deviceType(std::move(type)), protocol(std::move(prot)), controllerClass(std::move(ctrlClass)),
          supportedFeatures(std::move(features)), requiresModule(reqModule), moduleName(std::move(modName)) {}
};

class DeviceRegistry {
public:
    static DeviceRegistry& getInstance();

    // Load device database from JSON file
    bool loadFromFile(const std::string& filepath);

    // Save current database to JSON file
    bool saveToFile(const std::string& filepath) const;

    // Find device by VID/PID
    const DeviceInfo* findDevice(uint16_t vendorId, uint16_t productId) const;

    // Find devices by manufacturer
    std::vector<const DeviceInfo*> findDevicesByManufacturer(const std::string& manufacturer) const;

    // Find devices by type
    std::vector<const DeviceInfo*> findDevicesByType(const std::string& deviceType) const;

    // Get all devices
    const std::vector<DeviceInfo>& getAllDevices() const { return devices_; }

    // Add a device (for dynamic registration)
    void addDevice(const DeviceInfo& device);

    // Remove a device
    bool removeDevice(uint16_t vendorId, uint16_t productId);

    // Get supported device count
    size_t getDeviceCount() const { return devices_.size(); }

    // Check if registry is loaded
    bool isLoaded() const { return loaded_; }

private:
    DeviceRegistry() = default;
    ~DeviceRegistry() = default;
    DeviceRegistry(const DeviceRegistry&) = delete;
    DeviceRegistry& operator=(const DeviceRegistry&) = delete;

    std::vector<DeviceInfo> devices_;
    std::unordered_map<uint32_t, size_t> deviceMap_; // (vendorId << 16 | productId) -> index
    bool loaded_ = false;

    // Helper to create composite key
    static uint32_t makeKey(uint16_t vendorId, uint16_t productId) {
        return (static_cast<uint32_t>(vendorId) << 16) | productId;
    }

    // JSON serialization helpers
    DeviceInfo deviceFromJson(const nlohmann::json& j);
    nlohmann::json deviceToJson(const DeviceInfo& device) const;
};

} // namespace OneClickRGB