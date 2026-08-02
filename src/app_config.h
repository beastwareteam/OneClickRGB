/**
 * app_config.h - Unified settings and channel configuration
 *
 * Single source of truth for all persistent data.
 * One JSON file (config.json), one class, one Save(), one Load().
 *
 * Replaces the old AppSettings struct, ChannelManager, and the
 * fragmented app_settings.cfg / settings.json / channels.cfg files.
 */

#pragma once
#include "channel_config.h"
#include "devices/asus_aura.h"
#include "devices/evision.h"
#include <cstdint>
#include <string>
#include <map>
#include <fstream>
#include <iomanip>
#include <windows.h>
#include <shlobj.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

//=============================================================================
// RGBConfig - unified persistent settings
//=============================================================================

class RGBConfig {
public:
    //--- Color & effect ---
    uint8_t red        = 0;
    uint8_t green      = 34;
    uint8_t blue       = 255;
    uint8_t brightness = 4;   // 0-4 global brightness steps
    uint8_t speed      = 2;   // 0-5
    // Protocol values, not list indices - see devices/evision.h. Zero is not a
    // valid keyboard mode and means FREEZE for the edge zone, so these must not
    // be left zero-initialised.
    uint8_t kbMode     = devices::evision::KB_MODE_DEFAULT;
    uint8_t edgeMode   = devices::evision::EDGE_MODE_DEFAULT;

    //--- Device enables ---
    bool enableAura     = true;
    bool enableMouse    = true;
    bool enableKeyboard = true;
    bool enableRAM      = true;
    bool enableEdge     = true;

    //--- App behaviour ---
    bool autostart      = false;
    bool minimizeToTray = true;
    bool autoApply      = true;
    int  windowX        = -1;  // -1 = CW_USEDEFAULT
    int  windowY        = -1;
    int  themeId        = 0;   // 0=Dark, 1=Light, 2=Colorblind
    int  langId         = 0;   // 0=EN, 1=DE
    std::string lastProfile;

    //--- Channel correction (per device zone) ---

    // One entry per channel the Aura protocol can address. This used to be
    // eight while the protocol allows sixteen, so a board reporting more than
    // eight zones left the rest dark with no message. The correction dialog
    // still edits the first eight; the others apply the neutral default.
    static constexpr int AURA_CHANNELS = devices::aura::MAX_CHANNELS;
    static constexpr int RAM_SLOTS     = 4;

    ChannelConfig aura[AURA_CHANNELS];
    ChannelConfig ram[RAM_SLOTS];
    ChannelConfig steelseries;
    ChannelConfig keyboard;
    ChannelConfig edge;

    RGBConfig() { InitNames(); }

    //--- Persistence ---

    static std::wstring GetPath() {
        wchar_t* ap = nullptr;
        if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &ap) != S_OK)
            return L"oneclickrgb_config.json";
        std::wstring p = std::wstring(ap) + L"\\OneClickRGB\\config.json";
        CoTaskMemFree(ap);
        return p;
    }

    void Save() const {
        std::wstring path = GetPath();
        // Ensure directory exists
        size_t sl = path.find_last_of(L"\\/");
        if (sl != std::wstring::npos)
            SHCreateDirectoryExW(NULL, path.substr(0, sl).c_str(), NULL);

        json j;
        j["red"]        = red;
        j["green"]      = green;
        j["blue"]       = blue;
        j["brightness"] = brightness;
        j["speed"]      = speed;
        j["kbMode"]     = kbMode;
        j["edgeMode"]   = edgeMode;

        j["enableAura"]     = enableAura;
        j["enableMouse"]    = enableMouse;
        j["enableKeyboard"] = enableKeyboard;
        j["enableRAM"]      = enableRAM;
        j["enableEdge"]     = enableEdge;

        j["autostart"]      = autostart;
        j["minimizeToTray"] = minimizeToTray;
        j["autoApply"]      = autoApply;
        j["windowX"]        = windowX;
        j["windowY"]        = windowY;
        j["themeId"]        = themeId;
        j["langId"]         = langId;
        j["lastProfile"]    = lastProfile;

        auto chOut = [](const ChannelConfig& c) -> json {
            return {{"name", c.name}, {"enabled", c.enabled},
                    {"r", c.red_adjust}, {"g", c.green_adjust},
                    {"b", c.blue_adjust}, {"br", c.brightness}};
        };
        for (int i = 0; i < AURA_CHANNELS; i++) j["aura"][i] = chOut(aura[i]);
        for (int i = 0; i < RAM_SLOTS; i++) j["ram"][i]  = chOut(ram[i]);
        j["steelseries"] = chOut(steelseries);
        j["keyboard"]    = chOut(keyboard);
        j["edge"]        = chOut(edge);

        std::ofstream f(path);
        if (f) f << std::setw(4) << j;
    }

    /// Set by Load() when it had to correct a value read from disk. The caller
    /// writes the file back, so the repair survives even if the process is
    /// terminated rather than closed - otherwise a broken config keeps being
    /// repaired in memory on every start and never actually gets fixed.
    bool repairedOnLoad = false;

    void Load() {
        repairedOnLoad = false;
        InitNames();  // ensure defaults before overwriting from disk

        std::ifstream f(GetPath());
        if (!f) { MigrateLegacy(); return; }

        try {
            json j; f >> j;
            red        = (uint8_t)j.value("red",        (int)red);
            green      = (uint8_t)j.value("green",      (int)green);
            blue       = (uint8_t)j.value("blue",       (int)blue);
            brightness = (uint8_t)j.value("brightness", (int)brightness);
            speed      = (uint8_t)j.value("speed",      (int)speed);
            kbMode     = (uint8_t)j.value("kbMode",     (int)kbMode);
            edgeMode   = (uint8_t)j.value("edgeMode",   (int)edgeMode);

            // Files written by earlier versions hold a zero here (and older
            // ones a combo index), which would be sent to the keyboard as a
            // mode byte. Anything the protocol does not implement falls back to
            // the default rather than going out on the wire.
            if (!devices::evision::IsValidKeyboardMode(kbMode)) {
                kbMode = devices::evision::KB_MODE_DEFAULT;
                repairedOnLoad = true;
            }
            // FREEZE is a protocol value the UI never offered, so a stored zero
            // can only be the old zero-initialised default rather than a
            // deliberate choice - migrate it instead of sending it.
            if (!devices::evision::IsValidEdgeMode(edgeMode) ||
                edgeMode == devices::evision::EDGE_MODE_FREEZE) {
                edgeMode = devices::evision::EDGE_MODE_DEFAULT;
                repairedOnLoad = true;
            }

            enableAura     = j.value("enableAura",     enableAura);
            enableMouse    = j.value("enableMouse",    enableMouse);
            enableKeyboard = j.value("enableKeyboard", enableKeyboard);
            enableRAM      = j.value("enableRAM",      enableRAM);
            enableEdge     = j.value("enableEdge",     enableEdge);

            autostart      = j.value("autostart",      autostart);
            minimizeToTray = j.value("minimizeToTray", minimizeToTray);
            autoApply      = j.value("autoApply",      autoApply);
            windowX        = j.value("windowX", windowX);
            windowY        = j.value("windowY", windowY);
            themeId        = j.value("themeId", themeId);
            langId         = j.value("langId",  langId);
            lastProfile    = j.value("lastProfile", lastProfile);

            auto loadCh = [](const json& src, ChannelConfig& dst) {
                if (src.contains("name"))    dst.name         = src["name"].get<std::string>();
                dst.enabled     = src.value("enabled", dst.enabled);
                dst.red_adjust  = src.value("r",  dst.red_adjust);
                dst.green_adjust= src.value("g",  dst.green_adjust);
                dst.blue_adjust = src.value("b",  dst.blue_adjust);
                dst.brightness  = src.value("br", dst.brightness);
            };
            if (j.contains("aura") && j["aura"].is_array())
                for (int i = 0; i < AURA_CHANNELS && i < (int)j["aura"].size(); i++)
                    loadCh(j["aura"][i], aura[i]);
            if (j.contains("ram") && j["ram"].is_array())
                for (int i = 0; i < RAM_SLOTS && i < (int)j["ram"].size(); i++)
                    loadCh(j["ram"][i], ram[i]);
            if (j.contains("steelseries")) loadCh(j["steelseries"], steelseries);
            if (j.contains("keyboard"))    loadCh(j["keyboard"],    keyboard);
            if (j.contains("edge"))        loadCh(j["edge"],        edge);
        } catch (...) {}
    }

private:
    void InitNames() {
        for (int i = 0; i < AURA_CHANNELS; i++) aura[i].name = "ASUS Channel " + std::to_string(i);
        for (int i = 0; i < RAM_SLOTS; i++) ram[i].name  = "RAM Slot "     + std::to_string(i);
        steelseries.name = "SteelSeries Mouse";
        keyboard.name    = "Keyboard";
        edge.name        = "Keyboard Edge";
    }

    // One-time migration from old separate .cfg files on first run after upgrade
    void MigrateLegacy() {
        wchar_t oldBase[MAX_PATH];
        if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, oldBase) != S_OK) return;
        std::wstring dir = std::wstring(oldBase) + L"\\OneClickRGB";

        // --- app_settings.cfg (lang, theme, windowX/Y, lastProfile) ---
        std::ifstream appCfg(dir + L"\\app_settings.cfg");
        if (appCfg.is_open()) {
            std::string line;
            while (std::getline(appCfg, line)) {
                if (line.find("lang=de") != std::string::npos)
                    langId = 1;
                if (line.find("theme=") == 0)
                    try { themeId = std::stoi(line.substr(6)); } catch (...) {}
                if (line.find("windowX=") == 0)
                    try { windowX = std::stoi(line.substr(8)); } catch (...) {}
                if (line.find("windowY=") == 0)
                    try { windowY = std::stoi(line.substr(8)); } catch (...) {}
                if (line.find("lastProfile=") == 0)
                    lastProfile = line.substr(12);
            }
        }

        // --- channels.cfg (per-channel correction values) ---
        std::map<std::string, int> vals;
        std::ifstream chCfg(dir + L"\\channels.cfg");
        if (chCfg.is_open()) {
            std::string line;
            while (std::getline(chCfg, line)) {
                auto eq = line.find('=');
                if (eq != std::string::npos)
                    try { vals[line.substr(0, eq)] = std::stoi(line.substr(eq + 1)); } catch (...) {}
            }
            for (int i = 0; i < AURA_CHANNELS; i++) {
                std::string p = "aura" + std::to_string(i);
                if (vals.count(p + "_enabled"))    aura[i].enabled      = vals[p + "_enabled"] != 0;
                if (vals.count(p + "_red"))        aura[i].red_adjust   = vals[p + "_red"];
                if (vals.count(p + "_green"))      aura[i].green_adjust = vals[p + "_green"];
                if (vals.count(p + "_blue"))       aura[i].blue_adjust  = vals[p + "_blue"];
                if (vals.count(p + "_brightness")) aura[i].brightness   = vals[p + "_brightness"];
            }
            for (int i = 0; i < RAM_SLOTS; i++) {
                std::string p = "ram" + std::to_string(i);
                if (vals.count(p + "_enabled"))    ram[i].enabled      = vals[p + "_enabled"] != 0;
                if (vals.count(p + "_red"))        ram[i].red_adjust   = vals[p + "_red"];
                if (vals.count(p + "_green"))      ram[i].green_adjust = vals[p + "_green"];
                if (vals.count(p + "_blue"))       ram[i].blue_adjust  = vals[p + "_blue"];
                if (vals.count(p + "_brightness")) ram[i].brightness   = vals[p + "_brightness"];
            }
            auto loadOld = [&](const std::string& pref, ChannelConfig& cfg) {
                if (vals.count(pref + "_enabled"))    cfg.enabled      = vals[pref + "_enabled"] != 0;
                if (vals.count(pref + "_red"))        cfg.red_adjust   = vals[pref + "_red"];
                if (vals.count(pref + "_green"))      cfg.green_adjust = vals[pref + "_green"];
                if (vals.count(pref + "_blue"))       cfg.blue_adjust  = vals[pref + "_blue"];
                if (vals.count(pref + "_brightness")) cfg.brightness   = vals[pref + "_brightness"];
            };
            loadOld("steelseries", steelseries);
            loadOld("keyboard",    keyboard);
            loadOld("edge",        edge);
        }
    }
};

// Global instance - single source of truth for all persistent settings
inline RGBConfig g_config;
