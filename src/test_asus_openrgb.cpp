/**
 * ASUS Aura Test - OpenRGB-konformes Protokoll
 * Basiert auf der exakten OpenRGB-Implementierung
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <hidapi/hidapi.h>
#define NOMINMAX
#include <windows.h>
#include <algorithm>

namespace AuraUSB {
    constexpr uint16_t ASUS_VID = 0x0B05;
    constexpr uint16_t AURA_CONTROLLER_PID = 0x19AF;
    constexpr uint8_t MAGIC = 0xEC;

    // Commands (from OpenRGB)
    constexpr uint8_t CMD_FIRMWARE       = 0x82;
    constexpr uint8_t CMD_CONFIG         = 0xB0;
    constexpr uint8_t CMD_DIRECT         = 0x40;
    constexpr uint8_t CMD_EFFECT         = 0x35;
    constexpr uint8_t CMD_EFFECT_COLOR   = 0x36;
    constexpr uint8_t CMD_ADDRESSABLE    = 0x3B;
    constexpr uint8_t CMD_COMMIT         = 0x3F;

    // Device Channels for Direct Control
    constexpr uint8_t DEVICE_MAINBOARD   = 0x04;
    constexpr uint8_t DEVICE_ARGB_1      = 0x00;
    constexpr uint8_t DEVICE_ARGB_2      = 0x01;
    constexpr uint8_t DEVICE_ARGB_3      = 0x02;

    // Apply flag
    constexpr uint8_t APPLY_FLAG         = 0x80;

    // Effect modes
    constexpr uint8_t MODE_OFF           = 0x00;
    constexpr uint8_t MODE_STATIC        = 0x01;
    constexpr uint8_t MODE_BREATHING     = 0x02;
    constexpr uint8_t MODE_COLOR_CYCLE   = 0x04;
    constexpr uint8_t MODE_RAINBOW       = 0x05;
    constexpr uint8_t MODE_DIRECT        = 0xFF;

    constexpr uint8_t MAX_LEDS_PER_PACKET = 20;
}

struct DeviceConfig {
    uint8_t num_argb_headers;
    uint8_t num_mainboard_leds;
    uint8_t num_12v_headers;
};

hid_device* g_dev = nullptr;

void PrintHex(const uint8_t* data, size_t len, const std::string& label) {
    std::cout << label << ": ";
    for (size_t i = 0; i < len && i < 32; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
    }
    std::cout << std::dec << std::endl;
}

// Send 64-byte packet (no report ID prefix needed for this device)
bool SendPacket(const uint8_t* data, const std::string& desc = "") {
    if (!desc.empty()) std::cout << desc << std::endl;
    PrintHex(data, 16, "TX");
    int res = hid_write(g_dev, data, 64);
    if (res < 0) {
        std::wcout << L"Error: " << hid_error(g_dev) << std::endl;
        return false;
    }
    return true;
}

// Read response
bool ReadResponse(uint8_t* buf, size_t len, int timeout_ms = 500) {
    memset(buf, 0, len);
    int res = hid_read_timeout(g_dev, buf, len, timeout_ms);
    if (res > 0) {
        PrintHex(buf, res > 32 ? 32 : res, "RX");
        return true;
    }
    return false;
}

// Get firmware version
std::string GetFirmware() {
    uint8_t buf[64] = {0};
    buf[0] = AuraUSB::MAGIC;
    buf[1] = AuraUSB::CMD_FIRMWARE;

    SendPacket(buf, "Getting firmware");

    uint8_t response[64] = {0};
    if (ReadResponse(response, 64)) {
        // Firmware string starts at offset 2
        std::string fw;
        for (int i = 2; i < 18 && response[i] != 0; i++) {
            fw += (char)response[i];
        }
        return fw;
    }
    return "Unknown";
}

// Get config table
DeviceConfig GetConfig() {
    DeviceConfig cfg = {0};

    uint8_t buf[64] = {0};
    buf[0] = AuraUSB::MAGIC;
    buf[1] = AuraUSB::CMD_CONFIG;

    SendPacket(buf, "Getting config table");

    uint8_t response[64] = {0};
    if (ReadResponse(response, 64)) {
        // Parse config table (offsets from OpenRGB)
        cfg.num_argb_headers = response[0x02];
        cfg.num_mainboard_leds = response[0x1B];
        cfg.num_12v_headers = response[0x1D];
    }

    return cfg;
}

// Set Direct Mode colors for a device
// device: DEVICE_MAINBOARD, DEVICE_ARGB_1, etc.
// leds: array of RGB values
// led_count: number of LEDs
void SetDirectColors(uint8_t device, uint8_t* colors, uint8_t led_count) {
    uint8_t offset = 0;

    while (offset < led_count) {
        uint8_t send_count = std::min((uint8_t)AuraUSB::MAX_LEDS_PER_PACKET, (uint8_t)(led_count - offset));
        bool is_last = (offset + send_count >= led_count);

        uint8_t buf[64] = {0};
        buf[0] = AuraUSB::MAGIC;
        buf[1] = AuraUSB::CMD_DIRECT;
        buf[2] = device | (is_last ? AuraUSB::APPLY_FLAG : 0x00);
        buf[3] = offset;       // Start LED index
        buf[4] = send_count;   // Number of LEDs in this packet

        // Copy RGB data
        for (int i = 0; i < send_count; i++) {
            buf[5 + i*3 + 0] = colors[(offset + i) * 3 + 0];  // R
            buf[5 + i*3 + 1] = colors[(offset + i) * 3 + 1];  // G
            buf[5 + i*3 + 2] = colors[(offset + i) * 3 + 2];  // B
        }

        std::stringstream ss;
        ss << "Direct: Device 0x" << std::hex << (int)device
           << ", LEDs " << std::dec << (int)offset << "-" << (int)(offset + send_count - 1)
           << (is_last ? " [APPLY]" : "");
        SendPacket(buf, ss.str());

        offset += send_count;
        Sleep(10);
    }
}

// Set all LEDs of a device to one color
void SetDeviceColor(uint8_t device, uint8_t r, uint8_t g, uint8_t b, uint8_t led_count) {
    uint8_t colors[120 * 3];  // Max 120 LEDs
    for (int i = 0; i < led_count; i++) {
        colors[i * 3 + 0] = r;
        colors[i * 3 + 1] = g;
        colors[i * 3 + 2] = b;
    }
    SetDirectColors(device, colors, led_count);
}

// Set effect mode (hardware effect)
void SetEffectMode(uint8_t channel, uint8_t mode, uint8_t r, uint8_t g, uint8_t b, uint8_t speed = 2) {
    uint8_t buf[64] = {0};
    buf[0] = AuraUSB::MAGIC;
    buf[1] = AuraUSB::CMD_EFFECT;
    buf[2] = channel;
    buf[3] = 0x00;
    buf[4] = 0x00;  // Normal (not shutdown effect)
    buf[5] = mode;

    std::stringstream ss;
    ss << "Effect: Channel " << (int)channel << ", Mode " << (int)mode;
    SendPacket(buf, ss.str());

    // Set effect color
    if (mode != AuraUSB::MODE_OFF && mode != AuraUSB::MODE_COLOR_CYCLE &&
        mode != AuraUSB::MODE_RAINBOW && mode != AuraUSB::MODE_DIRECT) {

        buf[0] = AuraUSB::MAGIC;
        buf[1] = AuraUSB::CMD_EFFECT_COLOR;
        buf[2] = 0xFF;  // Mask high byte (all LEDs)
        buf[3] = 0xFF;  // Mask low byte
        buf[4] = 0x00;  // Normal
        // Color for first LED (will be applied to all in static mode)
        buf[5] = r;
        buf[6] = g;
        buf[7] = b;

        ss.str("");
        ss << "Effect Color: RGB(" << (int)r << "," << (int)g << "," << (int)b << ")";
        SendPacket(buf, ss.str());
    }
}

// Commit changes (save to hardware)
void Commit() {
    uint8_t buf[64] = {0};
    buf[0] = AuraUSB::MAGIC;
    buf[1] = AuraUSB::CMD_COMMIT;
    buf[2] = 0x55;  // Magic value

    SendPacket(buf, "Committing changes");
}

int main() {
    std::cout << "=== ASUS Aura OpenRGB-konformer Test ===" << std::endl;
    std::cout << "Basiert auf exakter OpenRGB-Implementierung" << std::endl << std::endl;

    if (hid_init() != 0) {
        std::cerr << "Failed to initialize HIDAPI" << std::endl;
        return 1;
    }

    // Open device
    struct hid_device_info* devs = hid_enumerate(AuraUSB::ASUS_VID, AuraUSB::AURA_CONTROLLER_PID);
    struct hid_device_info* cur = devs;

    while (cur) {
        if (cur->usage_page == 0xFF72) {
            g_dev = hid_open_path(cur->path);
            if (g_dev) {
                std::wcout << L"Opened: " << cur->product_string << std::endl;
                break;
            }
        }
        cur = cur->next;
    }
    hid_free_enumeration(devs);

    if (!g_dev) {
        std::cerr << "Failed to open device" << std::endl;
        hid_exit();
        return 1;
    }

    hid_set_nonblocking(g_dev, 1);

    // Get device info
    std::cout << "\n=== Device Info ===" << std::endl;
    std::string fw = GetFirmware();
    std::cout << "Firmware: " << fw << std::endl;

    DeviceConfig cfg = GetConfig();
    std::cout << "\nConfig Table:" << std::endl;
    std::cout << "  ARGB Headers: " << (int)cfg.num_argb_headers << std::endl;
    std::cout << "  Mainboard LEDs: " << (int)cfg.num_mainboard_leds << std::endl;
    std::cout << "  12V RGB Headers: " << (int)cfg.num_12v_headers << std::endl;

    // Calculate total devices
    std::cout << "\n=== Testing Devices ===" << std::endl;

    // Test 1: Mainboard LEDs (if present)
    if (cfg.num_mainboard_leds > 0) {
        std::cout << "\n--- Mainboard LEDs (" << (int)cfg.num_mainboard_leds << " LEDs) ---" << std::endl;
        std::cout << "Setting to RED..." << std::endl;
        SetDeviceColor(AuraUSB::DEVICE_MAINBOARD, 255, 0, 0, cfg.num_mainboard_leds);
        Sleep(1000);

        std::cout << "Setting to GREEN..." << std::endl;
        SetDeviceColor(AuraUSB::DEVICE_MAINBOARD, 0, 255, 0, cfg.num_mainboard_leds);
        Sleep(1000);

        std::cout << "Setting to BLUE..." << std::endl;
        SetDeviceColor(AuraUSB::DEVICE_MAINBOARD, 0, 0, 255, cfg.num_mainboard_leds);
        Sleep(1000);
    }

    // Test 2: ARGB Headers (assuming 8 LEDs per fan/strip)
    const uint8_t LEDS_PER_ARGB = 8;  // Typical for fans

    for (int header = 0; header < cfg.num_argb_headers; header++) {
        std::cout << "\n--- ARGB Header " << header << " (" << (int)LEDS_PER_ARGB << " LEDs) ---" << std::endl;

        uint8_t device = AuraUSB::DEVICE_ARGB_1 + header;

        std::cout << "Setting to RED..." << std::endl;
        SetDeviceColor(device, 255, 0, 0, LEDS_PER_ARGB);
        Sleep(500);

        std::cout << "Setting to GREEN..." << std::endl;
        SetDeviceColor(device, 0, 255, 0, LEDS_PER_ARGB);
        Sleep(500);

        std::cout << "Setting to BLUE..." << std::endl;
        SetDeviceColor(device, 0, 0, 255, LEDS_PER_ARGB);
        Sleep(500);
    }

    // Test 3: Effect mode on all channels
    std::cout << "\n=== Testing Effect Mode ===" << std::endl;
    std::cout << "Setting STATIC PURPLE on all channels..." << std::endl;

    // Mainboard channel
    SetEffectMode(0, AuraUSB::MODE_STATIC, 128, 0, 255);

    // ARGB channels (channel 1, 2, 3, ...)
    for (int i = 0; i < cfg.num_argb_headers; i++) {
        SetEffectMode(i + 1, AuraUSB::MODE_STATIC, 128, 0, 255);
    }

    Sleep(2000);

    // Final: Set Direct Mode to user's preferred color (Blue #0022FF)
    std::cout << "\n=== Setting Final Color ===" << std::endl;
    std::cout << "Setting all devices to BLUE (#0022FF)..." << std::endl;

    // Switch to Direct mode
    SetEffectMode(0, AuraUSB::MODE_DIRECT, 0, 0, 0);
    for (int i = 0; i < cfg.num_argb_headers; i++) {
        SetEffectMode(i + 1, AuraUSB::MODE_DIRECT, 0, 0, 0);
    }
    Sleep(100);

    // Set Direct colors
    if (cfg.num_mainboard_leds > 0) {
        SetDeviceColor(AuraUSB::DEVICE_MAINBOARD, 0x00, 0x22, 0xFF, cfg.num_mainboard_leds);
    }

    // ARGB Headers - test with different LED counts
    uint8_t argb_led_counts[] = {8, 16, 8};  // Adjust based on your setup
    for (int i = 0; i < cfg.num_argb_headers && i < 3; i++) {
        SetDeviceColor(AuraUSB::DEVICE_ARGB_1 + i, 0x00, 0x22, 0xFF, argb_led_counts[i]);
    }

    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "All devices should now be BLUE (#0022FF)" << std::endl;

    hid_close(g_dev);
    hid_exit();

    return 0;
}
