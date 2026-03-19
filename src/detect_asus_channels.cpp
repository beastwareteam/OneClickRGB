/**
 * ASUS Aura Channel Detection Tool
 * Detects all ASUS Aura devices and their channel configuration
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <hidapi/hidapi.h>

// ASUS Aura USB Protocol Constants (from OpenRGB)
namespace AuraUSB {
    // Vendor/Product IDs
    constexpr uint16_t ASUS_VID = 0x0B05;
    
    // Known ASUS Aura Controller PIDs
    constexpr uint16_t AURA_TERMINAL_PID       = 0x1889;
    constexpr uint16_t AURA_ADDRESSABLE_1_PID  = 0x1867;
    constexpr uint16_t AURA_ADDRESSABLE_2_PID  = 0x1872;
    constexpr uint16_t AURA_ADDRESSABLE_3_PID  = 0x18A3;
    constexpr uint16_t AURA_MOTHERBOARD_1_PID  = 0x18F3;
    constexpr uint16_t AURA_MOTHERBOARD_2_PID  = 0x1939;
    constexpr uint16_t AURA_MOTHERBOARD_3_PID  = 0x19AF;  // Your device!
    constexpr uint16_t AURA_ROG_STRIX_LC_PID   = 0x879F;
    
    // Commands
    constexpr uint8_t CMD_GET_CONFIG      = 0x30;  // Get device configuration
    constexpr uint8_t CMD_SET_DIRECT      = 0x35;  // Direct LED control
    constexpr uint8_t CMD_SET_EFFECT      = 0x35;  // Effect mode
    constexpr uint8_t CMD_FIRMWARE        = 0x82;  // Firmware info
    constexpr uint8_t CMD_GET_LED_COUNT   = 0xB0;  // Get LED count per channel
    constexpr uint8_t CMD_SET_LED_COUNT   = 0xB0;  // Set LED count per channel
    
    // Channel Types
    constexpr uint8_t CHANNEL_FIXED       = 0x00;  // Fixed/Static RGB (12V)
    constexpr uint8_t CHANNEL_ADDRESSABLE = 0x01;  // Addressable RGB (5V ARGB)
    
    // Direct Mode Sub-commands
    constexpr uint8_t DIRECT_APPLY        = 0x80;
    
    // Magic byte
    constexpr uint8_t MAGIC               = 0xEC;
}

struct AuraDevice {
    uint16_t vid;
    uint16_t pid;
    std::string name;
    int interface_number;
    uint16_t usage_page;
    hid_device* handle;
};

struct AuraChannel {
    uint8_t channel_id;
    uint8_t channel_type;  // 0=Fixed, 1=Addressable
    uint16_t led_count;
    std::string type_name;
};

std::vector<AuraDevice> detected_devices;

void PrintHex(const uint8_t* data, size_t len, const std::string& label) {
    std::cout << label << ": ";
    for (size_t i = 0; i < len; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
    }
    std::cout << std::dec << std::endl;
}

bool SendCommand(hid_device* dev, uint8_t cmd, uint8_t param1 = 0, uint8_t param2 = 0) {
    uint8_t buf[65] = {0};
    buf[0] = 0x00;  // Report ID
    buf[1] = AuraUSB::MAGIC;  // 0xEC
    buf[2] = cmd;
    buf[3] = param1;
    buf[4] = param2;
    
    PrintHex(buf, 8, "TX");
    int res = hid_write(dev, buf, 65);
    return res > 0;
}

bool ReadResponse(hid_device* dev, uint8_t* buf, size_t len) {
    memset(buf, 0, len);
    int res = hid_read_timeout(dev, buf, len, 500);
    if (res > 0) {
        PrintHex(buf, res > 16 ? 16 : res, "RX");
        return true;
    }
    return false;
}

void DetectChannels(hid_device* dev) {
    std::cout << "\n=== Detecting Channels ===" << std::endl;
    
    uint8_t response[65] = {0};
    
    // Try to get device configuration
    std::cout << "\n1. Getting device config (0x30)..." << std::endl;
    if (SendCommand(dev, AuraUSB::CMD_GET_CONFIG)) {
        if (ReadResponse(dev, response, 65)) {
            std::cout << "Config response received" << std::endl;
            
            // Parse channels from response
            // Format varies by device, but typically:
            // response[0] = number of channels
            // response[1+] = channel info
            int num_channels = response[0];
            if (num_channels > 0 && num_channels < 10) {
                std::cout << "Number of channels: " << num_channels << std::endl;
            }
        }
    }
    
    // Try firmware command for device info
    std::cout << "\n2. Getting firmware info (0x82)..." << std::endl;
    if (SendCommand(dev, AuraUSB::CMD_FIRMWARE)) {
        if (ReadResponse(dev, response, 65)) {
            // Firmware string starts at offset 2
            std::cout << "Firmware: ";
            for (int i = 2; i < 20 && response[i] != 0; i++) {
                std::cout << (char)response[i];
            }
            std::cout << std::endl;
        }
    }
    
    // Try to get LED count for each possible channel (0-7)
    std::cout << "\n3. Getting LED counts per channel (0xB0)..." << std::endl;
    for (int ch = 0; ch < 8; ch++) {
        if (SendCommand(dev, AuraUSB::CMD_GET_LED_COUNT, ch, 0x00)) {
            if (ReadResponse(dev, response, 65)) {
                uint16_t led_count = response[0] | (response[1] << 8);
                uint8_t ch_type = response[2];
                
                if (led_count > 0 && led_count < 500) {
                    std::cout << "  Channel " << ch << ": " 
                              << led_count << " LEDs, Type: " 
                              << (ch_type == 0 ? "Fixed(12V)" : "Addressable(5V)") 
                              << std::endl;
                }
            }
        }
    }
    
    // Try alternative config commands
    std::cout << "\n4. Trying alternative commands..." << std::endl;
    
    // 0xB0 with different params
    uint8_t test_cmds[] = {0x30, 0x31, 0x32, 0xB0, 0xB1, 0xB2, 0xB3};
    for (uint8_t cmd : test_cmds) {
        std::cout << "\nCommand 0x" << std::hex << (int)cmd << std::dec << ":" << std::endl;
        if (SendCommand(dev, cmd, 0x00, 0x00)) {
            ReadResponse(dev, response, 65);
        }
    }
}

int main() {
    std::cout << "=== ASUS Aura Channel Detection ===" << std::endl;
    std::cout << "Scanning for ASUS devices..." << std::endl << std::endl;
    
    if (hid_init() != 0) {
        std::cerr << "Failed to initialize HIDAPI" << std::endl;
        return 1;
    }
    
    // Enumerate all ASUS devices
    struct hid_device_info* devs = hid_enumerate(AuraUSB::ASUS_VID, 0);
    struct hid_device_info* cur = devs;
    
    while (cur) {
        std::cout << "Found ASUS device:" << std::endl;
        std::cout << "  PID: 0x" << std::hex << cur->product_id << std::dec << std::endl;
        std::cout << "  Interface: " << cur->interface_number << std::endl;
        std::cout << "  Usage Page: 0x" << std::hex << cur->usage_page << std::dec << std::endl;
        std::cout << "  Usage: 0x" << std::hex << cur->usage << std::dec << std::endl;
        
        if (cur->product_string) {
            std::wcout << L"  Product: " << cur->product_string << std::endl;
        }
        std::cout << std::endl;
        
        // Open devices with vendor-specific usage pages
        if (cur->usage_page >= 0xFF00) {
            std::cout << "  -> Opening for analysis..." << std::endl;
            
            hid_device* dev = hid_open_path(cur->path);
            if (dev) {
                std::cout << "  -> Device opened successfully!" << std::endl;
                hid_set_nonblocking(dev, 0);
                
                DetectChannels(dev);
                
                hid_close(dev);
            } else {
                std::cerr << "  -> Failed to open device" << std::endl;
            }
        }
        
        cur = cur->next;
    }
    
    hid_free_enumeration(devs);
    hid_exit();
    
    std::cout << "\n=== Detection Complete ===" << std::endl;
    return 0;
}
