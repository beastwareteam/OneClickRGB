#include "DeviceRegistry.h"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace OneClickRGB {

DeviceRegistry& DeviceRegistry::getInstance() {
    static DeviceRegistry instance;
    return instance;
}

bool DeviceRegistry::loadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open device registry file: " << filepath << std::endl;
            return false;
        }

        nlohmann::json jsonData;
        file >> jsonData;

        devices_.clear();
        deviceMap_.clear();

        if (jsonData.contains("devices") && jsonData["devices"].is_array()) {
            for (const auto& deviceJson : jsonData["devices"]) {
                DeviceInfo device = deviceFromJson(deviceJson);
                addDevice(device);
            }
        }

        loaded_ = true;
        std::cout << "Loaded " << devices_.size() << " devices from registry" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error loading device registry: " << e.what() << std::endl;
        return false;
    }
}

bool DeviceRegistry::saveToFile(const std::string& filepath) const {
    try {
        nlohmann::json jsonData;
        jsonData["version"] = "1.0";
        jsonData["description"] = "OneClickRGB Device Registry";
        jsonData["last_updated"] = "2024-01-01"; // TODO: Add timestamp

        nlohmann::json devicesJson = nlohmann::json::array();
        for (const auto& device : devices_) {
            devicesJson.push_back(deviceToJson(device));
        }
        jsonData["devices"] = devicesJson;

        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filepath << std::endl;
            return false;
        }

        file << jsonData.dump(2); // Pretty print with 2-space indentation
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error saving device registry: " << e.what() << std::endl;
        return false;
    }
}

const DeviceInfo* DeviceRegistry::findDevice(uint16_t vendorId, uint16_t productId) const {
    auto it = deviceMap_.find(makeKey(vendorId, productId));
    if (it != deviceMap_.end()) {
        return &devices_[it->second];
    }
    return nullptr;
}

std::vector<const DeviceInfo*> DeviceRegistry::findDevicesByManufacturer(const std::string& manufacturer) const {
    std::vector<const DeviceInfo*> result;
    for (const auto& device : devices_) {
        if (device.manufacturer == manufacturer) {
            result.push_back(&device);
        }
    }
    return result;
}

std::vector<const DeviceInfo*> DeviceRegistry::findDevicesByType(const std::string& deviceType) const {
    std::vector<const DeviceInfo*> result;
    for (const auto& device : devices_) {
        if (device.deviceType == deviceType) {
            result.push_back(&device);
        }
    }
    return result;
}

void DeviceRegistry::addDevice(const DeviceInfo& device) {
    uint32_t key = makeKey(device.vendorId, device.productId);

    // Check if device already exists
    if (deviceMap_.find(key) != deviceMap_.end()) {
        std::cout << "Device already exists: " << device.name << " (VID: " << device.vendorId
                  << ", PID: " << device.productId << ")" << std::endl;
        return;
    }

    devices_.push_back(device);
    deviceMap_[key] = devices_.size() - 1;
}

bool DeviceRegistry::removeDevice(uint16_t vendorId, uint16_t productId) {
    uint32_t key = makeKey(vendorId, productId);
    auto it = deviceMap_.find(key);

    if (it == deviceMap_.end()) {
        return false;
    }

    size_t index = it->second;
    devices_.erase(devices_.begin() + index);
    deviceMap_.erase(it);

    // Update indices for remaining devices
    for (auto& pair : deviceMap_) {
        if (pair.second > index) {
            pair.second--;
        }
    }

    return true;
}

DeviceInfo DeviceRegistry::deviceFromJson(const nlohmann::json& j) {
    std::vector<std::string> features;
    if (j.contains("supportedFeatures") && j["supportedFeatures"].is_array()) {
        for (const auto& feature : j["supportedFeatures"]) {
            features.push_back(feature);
        }
    }

    return DeviceInfo(
        j.value("name", "Unknown Device"),
        j.value("manufacturer", "Unknown"),
        j.value("vendorId", 0),
        j.value("productId", 0),
        j.value("deviceType", "unknown"),
        j.value("protocol", "unknown"),
        j.value("controllerClass", ""),
        features,
        j.value("requiresModule", false),
        j.value("moduleName", "")
    );
}

nlohmann::json DeviceRegistry::deviceToJson(const DeviceInfo& device) const {
    nlohmann::json j;
    j["name"] = device.name;
    j["manufacturer"] = device.manufacturer;
    j["vendorId"] = device.vendorId;
    j["productId"] = device.productId;
    j["deviceType"] = device.deviceType;
    j["protocol"] = device.protocol;
    j["controllerClass"] = device.controllerClass;
    j["supportedFeatures"] = device.supportedFeatures;
    j["requiresModule"] = device.requiresModule;
    j["moduleName"] = device.moduleName;
    return j;
}

} // namespace OneClickRGB