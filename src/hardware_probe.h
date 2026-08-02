/**
 * hardware_probe.h - Phase 0 diagnostic probe (--probe / --probe-interactive)
 *
 * Headless, read-mostly hardware capability dump. It is #included near the END
 * of oneclick_rgb_complete.cpp, AFTER the Devices namespace, the protocol
 * constants, EVisionQuery(), OpenAsusAura(), ReadAsusFirmware/ConfigTable(),
 * ParseAsusConfig() and the PawnIO typedefs are all defined - so it can reuse
 * the existing, verified low-level routines without duplicating protocol code.
 *
 * Output (FROZEN schema, see plan section 4b-3):
 *   - docs/probe_results_<ts>.json   (machine-readable, schemaVersion 1)
 *   - docs/Hardware_Capability_Report.md (human-readable, incl. LED-point list)
 *
 * Design rules:
 *   - NON-DESTRUCTIVE: the probe never writes colours or effect bytes. It only
 *     reads (HID enumerate, EVision 0x05 reads, Aura 0x82/0xB0 reads, SMBus
 *     read-byte / ENE-read). The only writes are the benign init handshakes
 *     that are required before a device will answer a read (SetGen1, EVision
 *     begin/end-configure). No 0x06 writes, no colour packets.
 *   - HONEST status: every addressing unit gets exactly one status of
 *     verified / partial / unknown / unavailable. We do not fake values we
 *     cannot read (e.g. Aura per-channel firmware effect mode has no confirmed
 *     GET command yet, so it is reported as "unknown" rather than guessed).
 */

#pragma once
#include <vector>
#include <string>
#include <cstdio>
#include <cstdint>
#include <cstdarg>

namespace probe {

static const int PROBE_SCHEMA_VERSION = 1;

//---------------------------------------------------------------------------
// small formatting helpers
//---------------------------------------------------------------------------
static std::string Hex16(uint16_t v) { char b[8];  snprintf(b, sizeof(b), "0x%04X", v); return b; }
static std::string Hex8 (uint8_t  v) { char b[6];  snprintf(b, sizeof(b), "0x%02X", v); return b; }

static std::string HexDump(const uint8_t* d, int n) {
    std::string s; char b[4];
    for (int i = 0; i < n; i++) { snprintf(b, sizeof(b), "%02X", d[i]); s += b; if (i + 1 < n) s += ' '; }
    return s;
}

static std::string TimeStamp() {
    SYSTEMTIME st; GetLocalTime(&st);
    char b[32];
    snprintf(b, sizeof(b), "%04d%02d%02d-%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return b;
}

static std::string WToU8(const wchar_t* w) {
    if (!w) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return "";
    std::string s(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], len, nullptr, nullptr);
    return s;
}

static void Con(const char* fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(buf, stdout);
    fflush(stdout);
}

//---------------------------------------------------------------------------
// HID interface enumeration for one VID -> json array
//---------------------------------------------------------------------------
static json EnumInterfaces(uint16_t vid) {
    json arr = json::array();
    struct hid_device_info* devs = hid_enumerate(vid, 0);
    for (auto* c = devs; c; c = c->next) {
        json e;
        e["productId"]       = Hex16(c->product_id);
        e["interfaceNumber"] = c->interface_number;
        e["usagePage"]       = Hex16(c->usage_page);
        e["usage"]           = Hex16(c->usage);
        e["releaseNumber"]   = Hex16(c->release_number);
        e["manufacturer"]    = WToU8(c->manufacturer_string);
        e["product"]         = WToU8(c->product_string);
        e["path"]            = c->path ? c->path : "";
        arr.push_back(e);
    }
    hid_free_enumeration(devs);
    return arr;
}

//---------------------------------------------------------------------------
// ASUS Aura mainboard
//---------------------------------------------------------------------------

// Non-destructive raw query: write {0xEC, code} and capture the full 65-byte
// response as hex. Only READ-style codes (0x82 firmware, 0xB0 config) are used
// by the probe - no SET codes (0x35/0x40/0x52) are ever sent here.
static std::string AuraQueryRaw(hid_device* dev, uint8_t code) {
    uint8_t buf[65]; memset(buf, 0, sizeof(buf));
    buf[0] = 0xEC; buf[1] = code;
    if (hid_write(dev, buf, 65) < 0) return "[write-err]";
    int n = hid_read_timeout(dev, buf, 65, 600);
    if (n < 0) return "[read-err]";
    return HexDump(buf, 65);
}

static json ProbeAura() {
    Con("  [Aura]  enumerating HID interfaces...\n");
    json d;
    d["id"]        = "aura";
    d["name"]      = "ASUS Aura Mainboard";
    d["vendorId"]  = Hex16(Devices::ASUS_VID);
    d["productId"] = Hex16(Devices::ASUS_AURA_PID);
    d["interfaces"] = EnumInterfaces(Devices::ASUS_VID);

    json units = json::array();
    json risks = json::array();
    std::string status = "unavailable";

    hid_device* dev = OpenAsusAura();   // opens 0xFF72 + benign SetGen1 init
    if (!dev) {
        risks.push_back("Aura HID control interface (usage_page 0xFF72) not found");
        d["status"] = status; d["units"] = units; d["risks"] = risks;
        return d;
    }

    char fw[17] = {0};
    if (ReadAsusFirmware(dev, fw)) { d["firmware"] = fw; Con("  [Aura]  firmware: %s\n", fw); }
    else { d["firmware"] = ""; risks.push_back("Firmware version read (0x82) failed"); }

    // Capture full raw responses - the 60-byte config table holds per-channel
    // layout/state we otherwise discard; expose it verbatim for offline analysis.
    d["rawFirmware0x82"] = AuraQueryRaw(dev, 0x82);
    d["rawConfig0xB0"]   = AuraQueryRaw(dev, 0xB0);

    AsusHardwareConfig cfg;
    if (ReadAsusConfigTable(dev, cfg.configTable)) {
        ParseAsusConfig(cfg);
        d["configTable"] = HexDump(cfg.configTable, 60);
        d["numChannels"] = cfg.numChannels;
        d["numMainboardLEDs"]      = cfg.numMainboardLEDs;
        d["numAddressableHeaders"] = cfg.numAddressableHeaders;
        status = (cfg.numChannels > 0) ? "verified" : "partial";

        for (int i = 0; i < cfg.numChannels; i++) {
            json u;
            u["index"]         = i;
            u["name"]          = cfg.channels[i].name;
            u["ledCount"]      = cfg.channels[i].ledCount;
            u["directChannel"] = cfg.channels[i].directChannel;
            u["addressable"]   = cfg.channels[i].addressable;
            // No confirmed GET command for the live per-channel effect mode yet.
            // We refuse to guess -> "unknown" (to be resolved via interactive probe).
            u["currentFirmwareMode"] = "unknown";
            u["reportId"]      = "0xEC";
            u["identifyTest"]  = { {"executed", false}, {"result", "not-run"}, {"behavior", ""} };
            u["verified"]      = false;

            json pts = json::array();   // full LED-point list (plan requirement)
            for (int j = 0; j < cfg.channels[i].ledCount; j++)
                pts.push_back({ {"index", j},
                                {"channel", cfg.channels[i].directChannel},
                                {"offset", j} });
            u["ledPoints"] = pts;
            units.push_back(u);
        }
        Con("  [Aura]  %d channel(s) parsed from config table 0xB0\n", cfg.numChannels);
    } else {
        risks.push_back("Config table read (0xB0) failed - channel map unknown");
        status = "partial";
    }

    risks.push_back("Live per-channel effect mode has no confirmed GET command - "
                    "rainbow/firmware-mode detection deferred to interactive probe");

    hid_close(dev);
    d["status"] = status; d["units"] = units; d["risks"] = risks;
    return d;
}

//---------------------------------------------------------------------------
// Endorfy / EVision keyboard  -  the key discovery target
//---------------------------------------------------------------------------
static json ProbeKeyboard() {
    Con("  [Keyboard]  enumerating vendor HID collections...\n");
    json d;
    d["id"]        = "keyboard";
    d["name"]      = "Endorfy/EVision Keyboard";
    d["vendorId"]  = Hex16(Devices::EVISION_VID);
    d["productId"] = Hex16(Devices::EVISION_PID);
    d["interfaces"] = EnumInterfaces(Devices::EVISION_VID);

    json units   = json::array();
    json risks   = json::array();
    json special = json::object();
    std::string status = "unavailable";

    struct hid_device_info* devs = hid_enumerate(Devices::EVISION_VID, Devices::EVISION_PID);
    int collIdx = 0;
    for (auto* c = devs; c; c = c->next) {
        if (c->usage_page < 0xFF00) continue;   // only vendor-defined collections

        json u;
        u["index"]           = collIdx++;
        u["name"]            = "MI_" + std::to_string(c->interface_number) +
                               " UP=" + Hex16(c->usage_page);
        u["interfaceNumber"] = c->interface_number;
        u["usagePage"]       = Hex16(c->usage_page);
        u["usage"]           = Hex16(c->usage);
        u["path"]            = c->path ? c->path : "";

        hid_device* kd = c->path ? hid_open_path(c->path) : nullptr;
        if (!kd) { u["status"] = "unavailable"; u["note"] = "open failed"; units.push_back(u); continue; }

        // begin-configure handshake (required before reads answer); no writes follow
        EVisionQuery(kd, 0x01, 0, nullptr, 0, nullptr); Sleep(15);

        uint8_t prof = 0;
        int pr = EVisionQuery(kd, 0x05, 0x00, nullptr, 1, &prof);
        u["activeProfileReadResult"] = pr;
        u["activeProfile"] = (pr >= 0 ? (int)prof : -1);

        // Dump the 3 on-board profile slots byte-for-byte (0x40 bytes each).
        // This is the raw material to identify mode / win-lock / segment / edge
        // bytes by comparison - identification happens later, not here.
        json profiles = json::array();
        bool anyReadable = false;
        for (int p = 0; p < 3; p++) {
            uint16_t base = (uint16_t)(p * 0x40 + 0x01);
            std::string dump;
            bool any = false;
            for (int off = 0; off < 0x40; off += 16) {
                uint8_t buf[16] = {0};
                int rr = EVisionQuery(kd, 0x05, (uint16_t)(base + off), nullptr, 16, buf);
                if (rr >= 0) { any = true; anyReadable = true; dump += HexDump(buf, 16) + "  "; }
                else dump += "[err] ";
                Sleep(3);
            }
            profiles.push_back({ {"profile", p}, {"baseOffset", Hex16(base)},
                                 {"readable", any}, {"hex", dump} });
        }
        u["configDump"] = profiles;

        // Contiguous sweep of the config space. The 3 profile blocks cover
        // 0x01..0xC0; the key-remap table (where Win-Lock disables the GUI keys,
        // HID usage 0xE3/0xE7) lives further out. Sweep wide and flag any row
        // containing a GUI-key usage so the Win-Lock byte can be located.
        json full = json::array();
        json guiHits = json::array();
        for (uint16_t off = 0x00; off < 0x600; off += 16) {
            uint8_t buf[16] = {0};
            int rr = EVisionQuery(kd, 0x05, off, nullptr, 16, buf);
            bool ok = (rr >= 0);
            full.push_back({ {"offset", Hex16(off)}, {"readable", ok},
                             {"hex", ok ? HexDump(buf, 16) : std::string("[err]")} });
            if (ok) for (int k = 0; k < 16; k++)
                if (buf[k] == 0xE3 || buf[k] == 0xE7)
                    guiHits.push_back({ {"offset", Hex16((uint16_t)(off + k))},
                                        {"usage", Hex8(buf[k])} });
            Sleep(2);
        }
        u["fullAddressDump"] = full;
        u["guiKeyHits"] = guiHits;   // candidate Win-key remap positions

        u["status"]     = anyReadable ? "partial" : "unknown";
        if (anyReadable && status != "verified") status = "partial";

        EVisionQuery(kd, 0x02, 0, nullptr, 0, nullptr);  // end-configure
        hid_close(kd);
        units.push_back(u);
        Con("  [Keyboard]  collection MI_%d UP=%s dumped\n",
            c->interface_number, Hex16(c->usage_page).c_str());
    }
    hid_free_enumeration(devs);

    if (units.empty())
        risks.push_back("No vendor HID collections (usage_page >= 0xFF00) found");

    // Special features - all UNVERIFIED until interactive probe blesses them.
    special["winLock"]         = { {"detected", false}, {"byte", nullptr},
                                   {"bit", nullptr}, {"candidates", json::array()} };
    special["onBoardProfiles"] = { {"count", 3}, {"verified", false} };
    special["segments"]        = { {"detected", false},
                                   {"note", "Segment addressing not yet identified - analyze configDump"} };
    risks.push_back("Win-Lock / segment / effect bytes NOT yet identified - "
                    "no targeted write must occur until verified via --probe-interactive");

    d["status"] = status; d["units"] = units;
    d["specialFeatures"] = special; d["risks"] = risks;
    return d;
}

//---------------------------------------------------------------------------
// SteelSeries Rival 600 (write-only protocol -> enumerate + static unit map)
//---------------------------------------------------------------------------
static json ProbeMouse() {
    Con("  [Mouse]  enumerating HID interfaces...\n");
    json d;
    d["id"]        = "mouse";
    d["name"]      = "SteelSeries Rival 600";
    d["vendorId"]  = Hex16(Devices::STEELSERIES_VID);
    d["productId"] = Hex16(Devices::RIVAL_600_PID);
    json ifaces = EnumInterfaces(Devices::STEELSERIES_VID);
    d["interfaces"] = ifaces;

    // Non-destructive feature-report read on the vendor collection (0xFFC0).
    struct hid_device_info* md = hid_enumerate(Devices::STEELSERIES_VID, Devices::RIVAL_600_PID);
    for (auto* c = md; c; c = c->next) {
        if (c->usage_page < 0xFF00 || !c->path) continue;
        hid_device* mdev = hid_open_path(c->path);
        if (!mdev) continue;
        uint8_t fb[64] = {0}; fb[0] = 0x00;
        int n = hid_get_feature_report(mdev, fb, sizeof(fb));
        d["featureReport_" + Hex16(c->usage_page)] = (n > 0) ? HexDump(fb, n) : std::string("[no-data]");
        hid_close(mdev);
        break;
    }
    hid_free_enumeration(md);

    json units = json::array();
    json risks = json::array();

    // Authoritative 8-zone map derived offline from the SteelSeries GG engine DB
    // (devices.id=120 'rival_600' settings JSON + prism zone_cache) and the
    // rival_600.migration layout. Keys/effect_index are the SteelSeries zone
    // identifiers; uiX/uiY are the original software layout coordinates so the
    // visual editor can place hotspots like the vendor tool. See docs/Mouse_Protocol.md.
    struct MouseZone { const char* key; const char* name; int effectIndex; int uiX, uiY; };
    static const MouseZone kZones[8] = {
        { "wheel_lighting", "Scroll Wheel", 0,  85,  70 },
        { "logo_lighting",  "Logo",         1,  85, 270 },
        { "z2_lighting",    "Side Z2",      2,  50, 145 },
        { "z3_lighting",    "Side Z3",      3, 120, 145 },
        { "z4_lighting",    "Side Z4",      4,  45, 180 },
        { "z5_lighting",    "Side Z5",      5, 125, 180 },
        { "z6_lighting",    "Side Z6",      6,  25, 210 },
        { "z7_lighting",    "Side Z7",      7, 145, 210 },
    };
    for (int i = 0; i < 8; i++) {
        units.push_back({
            { "index", i },
            { "key", kZones[i].key },
            { "name", kZones[i].name },
            { "effectIndex", kZones[i].effectIndex },
            { "ledCount", 1 },
            { "uiX", kZones[i].uiX }, { "uiY", kZones[i].uiY },
            // Current write path addresses zones as bit 1<<i; that bit -> physical
            // zone mapping is asserted from effect_index order but stays unverified
            // until Identify lights exactly one zone and the user confirms it.
            { "zoneBit", 1 << i },
            { "verified", false },
            { "identifyTest", { {"executed", false}, {"result", "not-run"}, {"behavior", ""} } }
        });
    }

    std::string status = ifaces.empty() ? "unavailable" : "partial";
    if (ifaces.empty()) risks.push_back("SteelSeries Rival 600 not found");
    else {
        risks.push_back("Zone identity/layout taken from SteelSeries GG DB (8 zones: "
                        "wheel/logo/z2-z7); write protocol is write-only");
        risks.push_back("Bit (1<<i) -> physical-zone mapping NOT yet light-verified - "
                        "confirm via Identify before blessing as verified");
    }

    d["status"] = status; d["units"] = units; d["risks"] = risks;
    d["zoneSource"] = "SteelSeries GG engine DB (devices.id=120) + rival_600.migration";
    return d;
}

//---------------------------------------------------------------------------
// G.Skill RAM via PawnIO / SMBus (read-only scan)
//---------------------------------------------------------------------------
static json ProbeRAM() {
    Con("  [RAM]  loading PawnIO + scanning SMBus 0x70-0x77...\n");
    json d;
    d["id"]        = "ram";
    d["name"]      = "G.Skill Trident Z5 RGB (SMBus)";
    d["vendorId"]  = "SMBus";
    d["productId"] = "-";
    json units = json::array();
    json risks = json::array();
    std::string status = "unavailable";
    auto finish = [&](void* dll, void* handle,
                      pawnio_close_t close_fn) {
        if (handle && close_fn) close_fn((HANDLE)handle);
        if (dll) FreeLibrary((HMODULE)dll);
        d["status"] = status; d["units"] = units; d["risks"] = risks;
        return d;
    };

    std::string exeDir = GetExeDirA();
    HMODULE dll = NULL;
    std::string dllPaths[] = { exeDir + "\\PawnIOLib.dll",
                               exeDir + "\\dependencies\\PawnIO\\PawnIOLib.dll",
                               "PawnIOLib.dll" };
    for (const auto& p : dllPaths) { dll = LoadLibraryA(p.c_str()); if (dll) break; }
    if (!dll) { risks.push_back("PawnIOLib.dll not found"); return finish(nullptr, nullptr, nullptr); }

    auto p_open  = (pawnio_open_t)  GetProcAddress(dll, "pawnio_open");
    auto p_load  = (pawnio_load_t)  GetProcAddress(dll, "pawnio_load");
    auto p_exec  = (pawnio_execute_t)GetProcAddress(dll, "pawnio_execute");
    auto p_close = (pawnio_close_t) GetProcAddress(dll, "pawnio_close");
    if (!p_open || !p_load || !p_exec || !p_close) {
        risks.push_back("PawnIO exports missing"); return finish(dll, nullptr, nullptr);
    }

    HANDLE handle = NULL;
    if (p_open(&handle) != S_OK) {
        risks.push_back("PawnIO driver not running (requires admin + one-time PawnIO_setup)");
        return finish(dll, nullptr, p_close);
    }

    HANDLE hFile = INVALID_HANDLE_VALUE;
    std::string binPaths[] = { exeDir + "\\SmbusI801.bin",
                               exeDir + "\\modules\\SmbusI801.bin",
                               exeDir + "\\dependencies\\PawnIO\\modules\\SmbusI801.bin",
                               "SmbusI801.bin" };
    for (const auto& p : binPaths) {
        hFile = CreateFileA(p.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) break;
    }
    if (hFile == INVALID_HANDLE_VALUE) {
        risks.push_back("SmbusI801.bin module not found"); return finish(dll, handle, p_close);
    }
    DWORD sz = GetFileSize(hFile, NULL);
    std::vector<uint8_t> blob(sz);
    ReadFile(hFile, blob.data(), sz, &sz, NULL);
    CloseHandle(hFile);
    if (p_load(handle, blob.data(), blob.size()) != S_OK) {
        risks.push_back("Failed to load SMBus module"); return finish(dll, handle, p_close);
    }

    auto smbus_xfer = [&](uint8_t addr, char rw, uint8_t cmd, int s, i2c_smbus_data* data) -> int {
        ULONG64 in[9] = { addr, (ULONG64)rw, cmd, (ULONG64)s };
        if (data) memcpy(&in[4], data, sizeof(i2c_smbus_data));
        ULONG64 out[5] = {0}; SIZE_T ret_sz;
        HRESULT hr = p_exec(handle, "ioctl_smbus_xfer", in, 9, out, 5, &ret_sz);
        if (data) memcpy(data, &out[0], sizeof(i2c_smbus_data));
        return hr == S_OK ? 0 : -1;
    };
    auto read_byte = [&](uint8_t addr) -> int {
        i2c_smbus_data dt; return smbus_xfer(addr, 1, 0, 1, &dt) < 0 ? -1 : dt.byte;
    };
    auto write_word = [&](uint8_t addr, uint8_t cmd, uint16_t val) {
        i2c_smbus_data dt; dt.word = val; smbus_xfer(addr, 0, cmd, 3, &dt);
    };
    auto ene_read = [&](uint8_t addr, uint16_t reg) -> uint8_t {
        uint16_t sw = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF);
        write_word(addr, 0x00, sw); Sleep(1);
        i2c_smbus_data dt; smbus_xfer(addr, 1, 0x81, 2, &dt); return dt.byte;
    };

    int slot = 0;
    uint8_t addrs[] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};
    for (uint8_t addr : addrs) {
        if (read_byte(addr) < 0) continue;
        char name[17] = {0};
        for (int i = 0; i < 16; i++) name[i] = (char)ene_read(addr, 0x1000 + i);
        bool looksGSkill = strstr(name, "AUDA") || strstr(name, "DIMM") || strstr(name, "Trident");
        uint8_t led_count = ene_read(addr, 0x1C02);
        int lc = (led_count == 0 || led_count > 20) ? 8 : led_count;

        json u;
        u["index"]        = slot;
        u["name"]         = name;
        u["smbusAddress"] = Hex8(addr);
        u["ledCount"]     = lc;
        u["looksGSkill"]  = looksGSkill;
        u["verified"]     = false;
        json pts = json::array();
        for (int j = 0; j < lc; j++)
            pts.push_back({ {"index", j}, {"register", Hex16((uint16_t)(0x8100 + j * 3))} });
        u["ledPoints"] = pts;

        // Capture current device state (non-destructive ENE reads): the config
        // block (incl. mode/led-count) and the live per-LED colour registers.
        json regs = json::object();
        std::string cfgBlk;
        for (uint16_t reg = 0x1C00; reg < 0x1C10; reg++) {
            char b[4]; snprintf(b, sizeof(b), "%02X", ene_read(addr, reg)); cfgBlk += b; cfgBlk += ' ';
        }
        regs["configBlock_0x1C00"] = cfgBlk;
        std::string colBlk;
        int lcRead = (lc > 8) ? 8 : lc;
        for (int j = 0; j < lcRead * 3; j++) {
            char b[4]; snprintf(b, sizeof(b), "%02X", ene_read(addr, (uint16_t)(0x8100 + j))); colBlk += b; colBlk += ' ';
        }
        regs["currentColors_0x8100"] = colBlk;
        u["registers"] = regs;

        units.push_back(u);
        slot++;
        Con("  [RAM]  module @%s name='%s' leds=%d\n", Hex8(addr).c_str(), name, lc);
    }
    status = (slot > 0) ? "verified" : "partial";
    if (slot == 0) risks.push_back("No modules answered on SMBus 0x70-0x77");
    return finish(dll, handle, p_close);
}

//---------------------------------------------------------------------------
// Markdown report writer
//---------------------------------------------------------------------------
static void WriteMarkdown(const json& root, const std::string& path) {
    std::ofstream f(path);
    if (!f) return;
    f << "# OneClickRGB - Hardware Capability Report\n\n";
    f << "Generated: " << root.value("probeTimestamp", "") << "  \n";
    f << "Schema version: " << root.value("schemaVersion", 0) << "\n\n";

    const json& sum = root["summary"];
    f << "## Summary\n\n";
    f << "- Hardware detected: **" << sum.value("hardwareCount", 0) << "**\n";
    auto list = [&](const char* label, const char* key) {
        f << "- " << label << ": ";
        if (sum.contains(key) && !sum[key].empty()) {
            bool first = true;
            for (auto& s : sum[key]) { if (!first) f << ", "; f << s.get<std::string>(); first = false; }
        } else f << "_(none)_";
        f << "\n";
    };
    list("Ready to use", "readyToUse");
    list("Needs work",   "needsWork");
    list("Critical risks", "criticalRisks");
    f << "\n";

    for (const auto& dev : root["devices"]) {
        f << "## " << dev.value("name", "?") << "  (`" << dev.value("id", "") << "`)\n\n";
        f << "- VID:PID = `" << dev.value("vendorId", "") << ":" << dev.value("productId", "") << "`\n";
        f << "- Status: **" << dev.value("status", "") << "**\n";
        if (dev.contains("firmware") && !dev["firmware"].get<std::string>().empty())
            f << "- Firmware: `" << dev["firmware"].get<std::string>() << "`\n";

        if (dev.contains("interfaces") && !dev["interfaces"].empty()) {
            f << "\n### HID interfaces / collections\n\n";
            f << "| MI | UsagePage | Usage | Product |\n|---|---|---|---|\n";
            for (auto& i : dev["interfaces"])
                f << "| " << i.value("interfaceNumber", 0) << " | " << i.value("usagePage", "")
                  << " | " << i.value("usage", "") << " | " << i.value("product", "") << " |\n";
        }

        if (dev.contains("units") && !dev["units"].empty()) {
            f << "\n### Addressing units / LED points\n\n";
            for (auto& u : dev["units"]) {
                f << "- **" << u.value("name", "?") << "**";
                if (u.contains("ledCount")) f << " - " << u["ledCount"].get<int>() << " LED(s)";
                if (u.contains("smbusAddress")) f << " @ " << u["smbusAddress"].get<std::string>();
                if (u.contains("status")) f << " [" << u["status"].get<std::string>() << "]";
                f << (u.value("verified", false) ? " (verified)" : "") << "\n";
                if (u.contains("configDump")) {
                    for (auto& p : u["configDump"])
                        f << "    - profile " << p.value("profile", 0)
                          << " base " << p.value("baseOffset", "") << ": `"
                          << p.value("hex", "") << "`\n";
                }
            }
        }

        if (dev.contains("risks") && !dev["risks"].empty()) {
            f << "\n### Risks / open questions\n\n";
            for (auto& r : dev["risks"]) f << "- " << r.get<std::string>() << "\n";
        }
        f << "\n";
    }

    if (root.contains("nextSteps") && !root["nextSteps"].empty()) {
        f << "## Next steps\n\n";
        for (auto& s : root["nextSteps"]) f << "1. " << s.get<std::string>() << "\n";
    }
}

//---------------------------------------------------------------------------
// Entry point - called from WinMain on --probe
//---------------------------------------------------------------------------
static int RunHardwareProbe(bool /*interactive*/) {
    // Attach to the launching console if there is one, else allocate a new one.
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) AllocConsole();
    FILE* fdummy;
    freopen_s(&fdummy, "CONOUT$", "w", stdout);
    freopen_s(&fdummy, "CONOUT$", "w", stderr);

    DWORD t0 = GetTickCount();
    Con("\n=== OneClickRGB Hardware Probe (schema v%d) ===\n", PROBE_SCHEMA_VERSION);

    hid_init();

    json root;
    root["schemaVersion"]  = PROBE_SCHEMA_VERSION;
    root["probeTimestamp"] = TimeStamp();

    json devices = json::array();
    devices.push_back(ProbeAura());
    devices.push_back(ProbeKeyboard());
    devices.push_back(ProbeMouse());
    devices.push_back(ProbeRAM());
    root["devices"] = devices;

    hid_exit();

    // summary
    json readyToUse = json::array(), needsWork = json::array(), criticalRisks = json::array();
    int hwCount = 0;
    for (auto& dv : devices) {
        std::string st = dv.value("status", "unavailable");
        std::string nm = dv.value("name", "?");
        if (st != "unavailable") hwCount++;
        if (st == "verified")       readyToUse.push_back(nm);
        else if (st == "partial" || st == "unknown") needsWork.push_back(nm);
        for (auto& r : dv.value("risks", json::array()))
            if (r.get<std::string>().find("not yet identified") != std::string::npos ||
                r.get<std::string>().find("NOT yet identified") != std::string::npos)
                criticalRisks.push_back(nm + ": " + r.get<std::string>());
    }
    root["summary"] = { {"hardwareCount", hwCount},
                        {"readyToUse", readyToUse},
                        {"needsWork", needsWork},
                        {"criticalRisks", criticalRisks} };

    root["nextSteps"] = json::array({
        "Review keyboard configDump to locate Win-Lock / segment / effect bytes",
        "Run --probe-interactive to bless verified units (Identify) into config.json",
        "Confirm the single correct Edge-LED offset to replace the 15x brute-force write",
        "Determine a confirmed Aura per-channel effect-mode GET for rainbow detection"
    });

    // write outputs to ./docs (created if missing)
    SHCreateDirectoryExA(NULL, "docs", NULL);
    std::string jsonPath = "docs\\probe_results_" + root["probeTimestamp"].get<std::string>() + ".json";
    std::string mdPath   = "docs\\Hardware_Capability_Report.md";

    { std::ofstream jf(jsonPath); if (jf) jf << std::setw(2) << root; }
    WriteMarkdown(root, mdPath);

    DWORD dt = GetTickCount() - t0;
    Con("\nDevices detected: %d   (elapsed %lu ms)\n", hwCount, (unsigned long)dt);
    Con("JSON  -> %s\n", jsonPath.c_str());
    Con("Report-> %s\n", mdPath.c_str());
    Con("=== Probe complete ===\n\n");
    return 0;
}

} // namespace probe
