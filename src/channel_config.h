/**
 * Channel Configuration - Per-channel color correction and settings
 */

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <shlobj.h>

struct ChannelConfig {
    std::string name;
    bool enabled = true;

    // Color correction (0-200, 100 = neutral)
    int red_adjust = 100;
    int green_adjust = 100;
    int blue_adjust = 100;

    // Brightness (0-100)
    int brightness = 100;

    // Apply color correction
    void ApplyCorrection(uint8_t& r, uint8_t& g, uint8_t& b) const {
        if (!enabled) {
            r = g = b = 0;
            return;
        }

        int nr = (r * red_adjust * brightness) / 10000;
        int ng = (g * green_adjust * brightness) / 10000;
        int nb = (b * blue_adjust * brightness) / 10000;

        r = (nr > 255) ? 255 : (nr < 0 ? 0 : nr);
        g = (ng > 255) ? 255 : (ng < 0 ? 0 : ng);
        b = (nb > 255) ? 255 : (nb < 0 ? 0 : nb);
    }
};

class ChannelManager {
public:
    // ASUS Aura channels (0-7)
    ChannelConfig aura_channels[8];

    // RAM modules (0-3)
    ChannelConfig ram_modules[4];

    // Other devices
    ChannelConfig steelseries;
    ChannelConfig keyboard;
    ChannelConfig edge;

    ChannelManager() {
        // Initialize with default names
        for (int i = 0; i < 8; i++) {
            aura_channels[i].name = "ASUS Channel " + std::to_string(i);
        }
        for (int i = 0; i < 4; i++) {
            ram_modules[i].name = "RAM Slot " + std::to_string(i);
        }
        steelseries.name = "SteelSeries Mouse";
        keyboard.name = "Keyboard";
        edge.name = "Keyboard Edge";
    }

    std::wstring GetConfigPath() {
        wchar_t path[MAX_PATH];
        SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path);
        std::wstring dir = std::wstring(path) + L"\\OneClickRGB";
        CreateDirectoryW(dir.c_str(), NULL);
        return dir + L"\\channels.cfg";
    }

    void Save() {
        std::wstring path = GetConfigPath();
        std::ofstream file(path);
        if (!file.is_open()) return;

        // Save ASUS channels
        for (int i = 0; i < 8; i++) {
            file << "aura" << i << "_enabled=" << aura_channels[i].enabled << "\n";
            file << "aura" << i << "_red=" << aura_channels[i].red_adjust << "\n";
            file << "aura" << i << "_green=" << aura_channels[i].green_adjust << "\n";
            file << "aura" << i << "_blue=" << aura_channels[i].blue_adjust << "\n";
            file << "aura" << i << "_brightness=" << aura_channels[i].brightness << "\n";
        }

        // Save RAM modules
        for (int i = 0; i < 4; i++) {
            file << "ram" << i << "_enabled=" << ram_modules[i].enabled << "\n";
            file << "ram" << i << "_red=" << ram_modules[i].red_adjust << "\n";
            file << "ram" << i << "_green=" << ram_modules[i].green_adjust << "\n";
            file << "ram" << i << "_blue=" << ram_modules[i].blue_adjust << "\n";
            file << "ram" << i << "_brightness=" << ram_modules[i].brightness << "\n";
        }

        // Save other devices
        auto saveDevice = [&](const std::string& prefix, const ChannelConfig& cfg) {
            file << prefix << "_enabled=" << cfg.enabled << "\n";
            file << prefix << "_red=" << cfg.red_adjust << "\n";
            file << prefix << "_green=" << cfg.green_adjust << "\n";
            file << prefix << "_blue=" << cfg.blue_adjust << "\n";
            file << prefix << "_brightness=" << cfg.brightness << "\n";
        };

        saveDevice("steelseries", steelseries);
        saveDevice("keyboard", keyboard);
        saveDevice("edge", edge);

        file.close();
    }

    void Load() {
        std::wstring path = GetConfigPath();
        std::ifstream file(path);
        if (!file.is_open()) return;

        std::map<std::string, int> values;
        std::string line;
        while (std::getline(file, line)) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                int val = std::stoi(line.substr(pos + 1));
                values[key] = val;
            }
        }
        file.close();

        // Load ASUS channels
        for (int i = 0; i < 8; i++) {
            std::string prefix = "aura" + std::to_string(i);
            if (values.count(prefix + "_enabled")) aura_channels[i].enabled = values[prefix + "_enabled"];
            if (values.count(prefix + "_red")) aura_channels[i].red_adjust = values[prefix + "_red"];
            if (values.count(prefix + "_green")) aura_channels[i].green_adjust = values[prefix + "_green"];
            if (values.count(prefix + "_blue")) aura_channels[i].blue_adjust = values[prefix + "_blue"];
            if (values.count(prefix + "_brightness")) aura_channels[i].brightness = values[prefix + "_brightness"];
        }

        // Load RAM modules
        for (int i = 0; i < 4; i++) {
            std::string prefix = "ram" + std::to_string(i);
            if (values.count(prefix + "_enabled")) ram_modules[i].enabled = values[prefix + "_enabled"];
            if (values.count(prefix + "_red")) ram_modules[i].red_adjust = values[prefix + "_red"];
            if (values.count(prefix + "_green")) ram_modules[i].green_adjust = values[prefix + "_green"];
            if (values.count(prefix + "_blue")) ram_modules[i].blue_adjust = values[prefix + "_blue"];
            if (values.count(prefix + "_brightness")) ram_modules[i].brightness = values[prefix + "_brightness"];
        }

        // Load other devices
        auto loadDevice = [&](const std::string& prefix, ChannelConfig& cfg) {
            if (values.count(prefix + "_enabled")) cfg.enabled = values[prefix + "_enabled"];
            if (values.count(prefix + "_red")) cfg.red_adjust = values[prefix + "_red"];
            if (values.count(prefix + "_green")) cfg.green_adjust = values[prefix + "_green"];
            if (values.count(prefix + "_blue")) cfg.blue_adjust = values[prefix + "_blue"];
            if (values.count(prefix + "_brightness")) cfg.brightness = values[prefix + "_brightness"];
        };

        loadDevice("steelseries", steelseries);
        loadDevice("keyboard", keyboard);
        loadDevice("edge", edge);
    }
};
