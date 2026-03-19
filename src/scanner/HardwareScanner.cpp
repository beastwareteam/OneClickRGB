/*---------------------------------------------------------*\
| HardwareScanner.cpp                                       |
|                                                           |
| Hardware-First Detection with REAL HIDAPI                 |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "HardwareScanner.h"
#include <iostream>
#include <hidapi.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Helper: Convert wide string to UTF-8
static std::string WideToUTF8(const wchar_t* wstr)
{
    if (!wstr || !wstr[0]) return "";

#ifdef _WIN32
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], size, nullptr, nullptr);
    return result;
#else
    // Simple fallback for non-Windows
    std::string result;
    while (*wstr) {
        if (*wstr < 128) {
            result += static_cast<char>(*wstr);
        }
        wstr++;
    }
    return result;
#endif
}

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Static HIDAPI initialization                              |
\*---------------------------------------------------------*/
static bool hidapi_initialized = false;

static bool EnsureHIDAPI()
{
    if (!hidapi_initialized)
    {
        if (hid_init() == 0)
        {
            hidapi_initialized = true;
        }
        else
        {
            std::cerr << "[HIDAPI] Failed to initialize!\n";
            return false;
        }
    }
    return true;
}

/*---------------------------------------------------------*\
| DetectedHardware Implementation                           |
\*---------------------------------------------------------*/
std::string DetectedHardware::GetHardwareId() const
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%04X:%04X", vendor_id, product_id);
    return std::string(buffer);
}

/*---------------------------------------------------------*\
| HardwareScanner Implementation                            |
\*---------------------------------------------------------*/
HardwareScanner::HardwareScanner()
{
    InitializeBuiltinDatabase();
}

HardwareScanner::~HardwareScanner()
{
}

std::vector<DetectedHardware> HardwareScanner::ScanAll()
{
    std::vector<DetectedHardware> all_devices;

    std::cout << "\n[SCANNER] Scanning for RGB devices (REAL HARDWARE)...\n";

    // Scan USB HID devices
    auto hid_devices = ScanUSBHID();
    all_devices.insert(all_devices.end(), hid_devices.begin(), hid_devices.end());

    // Scan I2C/SMBus (future)
    auto i2c_devices = ScanI2C();
    all_devices.insert(all_devices.end(), i2c_devices.begin(), i2c_devices.end());

    std::cout << "[SCANNER] Found " << all_devices.size() << " RGB device(s)\n";

    return all_devices;
}

std::vector<DetectedHardware> HardwareScanner::ScanUSBHID()
{
#ifdef _WIN32
    return ScanUSBHID_Windows();
#else
    return ScanUSBHID_Linux();
#endif
}

std::vector<DetectedHardware> HardwareScanner::ScanUSBHID_Windows()
{
    std::vector<DetectedHardware> devices;

    if (!EnsureHIDAPI())
    {
        std::cerr << "[SCANNER] HIDAPI not available!\n";
        return devices;
    }

    std::cout << "[SCANNER] Enumerating USB HID devices...\n";

    // Enumerate ALL HID devices
    hid_device_info* device_list = hid_enumerate(0, 0);
    hid_device_info* current = device_list;

    int total_hid = 0;
    int matched = 0;

    while (current != nullptr)
    {
        total_hid++;

        // Check if this VID:PID is in our database
        uint32_t key = MakeKey(current->vendor_id, current->product_id);

        if (controller_database.find(key) != controller_database.end())
        {
            const ControllerEntry& entry = controller_database[key];

            // Check if this is the correct interface/usage_page
            bool interface_match = (entry.interface_number < 0) ||
                                   (current->interface_number == entry.interface_number);
            bool usage_match = (entry.usage_page < 0) ||
                               (static_cast<int>(current->usage_page) == entry.usage_page);

            if (interface_match && usage_match)
            {
                DetectedHardware hw;
                hw.vendor_id = current->vendor_id;
                hw.product_id = current->product_id;
                hw.device_path = current->path ? current->path : "";
                hw.bus_type = DetectedHardware::BusType::USB_HID;

                // Convert wide strings to UTF-8
                hw.manufacturer = WideToUTF8(current->manufacturer_string);
                hw.product_name = WideToUTF8(current->product_string);
                hw.serial_number = WideToUTF8(current->serial_number);

                // Check for duplicates - only keep one interface per VID:PID
                bool duplicate = false;
                for (const auto& existing : devices)
                {
                    if (existing.vendor_id == hw.vendor_id &&
                        existing.product_id == hw.product_id)
                    {
                        duplicate = true;
                        break;
                    }
                }

                if (!duplicate)
                {
                    std::cout << "  [+] Found: " << hw.product_name;
                    if (hw.product_name.empty())
                    {
                        std::cout << entry.display_name;
                    }
                    std::cout << " (" << std::hex << hw.vendor_id << ":"
                              << hw.product_id << std::dec << ")";
                    std::cout << " [Interface " << current->interface_number
                              << ", Usage 0x" << std::hex << current->usage_page << std::dec << "]\n";

                    devices.push_back(hw);
                    matched++;
                }
            }
        }

        current = current->next;
    }

    hid_free_enumeration(device_list);

    std::cout << "[SCANNER] Scanned " << total_hid << " HID devices, "
              << matched << " matched RGB database\n";

    return devices;
}

std::vector<DetectedHardware> HardwareScanner::ScanUSBHID_Linux()
{
    std::vector<DetectedHardware> devices;

    if (!EnsureHIDAPI())
    {
        return devices;
    }

    // Same logic as Windows - HIDAPI is cross-platform
    hid_device_info* device_list = hid_enumerate(0, 0);
    hid_device_info* current = device_list;

    while (current != nullptr)
    {
        uint32_t key = MakeKey(current->vendor_id, current->product_id);

        if (controller_database.find(key) != controller_database.end())
        {
            DetectedHardware hw;
            hw.vendor_id = current->vendor_id;
            hw.product_id = current->product_id;
            hw.device_path = current->path ? current->path : "";
            hw.bus_type = DetectedHardware::BusType::USB_HID;

            hw.manufacturer = WideToUTF8(current->manufacturer_string);
            hw.product_name = WideToUTF8(current->product_string);

            devices.push_back(hw);
        }

        current = current->next;
    }

    hid_free_enumeration(device_list);

    return devices;
}

std::vector<DetectedHardware> HardwareScanner::ScanI2C()
{
#ifdef _WIN32
    return ScanI2C_Windows();
#else
    return ScanI2C_Linux();
#endif
}

std::vector<DetectedHardware> HardwareScanner::ScanI2C_Windows()
{
    std::vector<DetectedHardware> devices;

    // I2C/SMBus scanning for DDR5 RGB RAM
    // Note: This requires Administrator privileges and WinRing0/inpoutx64

    // TODO: Uncomment when SMBus implementation is integrated
    /*
    #include "../smbus/SMBusInterface.h"
    #include "../smbus/SMBusWindows.h"
    #include "../controllers/GSkillTridentZ5Controller.h"

    auto smbus = SMBusInterface::Create();
    if (!smbus || !smbus->IsInitialized())
    {
        // SMBus not available - likely not running as admin
        // or WinRing0 driver not installed
        return devices;
    }

    std::cout << "[SCANNER] Scanning SMBus for DDR5 RGB modules...\n";

    // Detect G.Skill Trident Z5 RGB modules
    auto modules = GSkillTridentZ5Controller::DetectModules(smbus.get());

    for (uint8_t addr : modules)
    {
        DetectedHardware hw;
        hw.vendor_id = 0x0D62;  // G.Skill vendor ID
        hw.product_id = 0x0001;  // Trident Z5 RGB product ID
        hw.bus_type = DetectedHardware::BusType::I2C_SMBUS;
        hw.device_path = "SMBUS:" + std::to_string(addr);
        hw.product_name = "G.Skill Trident Z5 RGB DDR5";
        hw.manufacturer = "G.Skill";
        hw.serial_number = "";

        std::cout << "  [+] Found: " << hw.product_name
                  << " at address 0x" << std::hex << (int)addr << std::dec << "\n";

        devices.push_back(hw);
    }

    smbus->Shutdown();
    */

    return devices;
}

std::vector<DetectedHardware> HardwareScanner::ScanI2C_Linux()
{
    std::vector<DetectedHardware> devices;
    return devices;
}

std::vector<DetectedHardware> HardwareScanner::ScanPCI()
{
    std::vector<DetectedHardware> devices;
    return devices;
}

bool HardwareScanner::HasKnownController(const DetectedHardware& hw) const
{
    uint32_t key = MakeKey(hw.vendor_id, hw.product_id);
    return controller_database.find(key) != controller_database.end();
}

const ControllerEntry* HardwareScanner::GetControllerEntry(const DetectedHardware& hw) const
{
    uint32_t key = MakeKey(hw.vendor_id, hw.product_id);
    auto it = controller_database.find(key);
    if (it != controller_database.end())
    {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::pair<DetectedHardware, ControllerEntry>>
HardwareScanner::GetMatchedDevices()
{
    std::vector<std::pair<DetectedHardware, ControllerEntry>> matched;

    auto all_hw = ScanAll();

    for (const auto& hw : all_hw)
    {
        const ControllerEntry* entry = GetControllerEntry(hw);
        if (entry != nullptr)
        {
            matched.push_back({hw, *entry});
        }
    }

    return matched;
}

bool HardwareScanner::LoadControllerDatabase(const std::string& path)
{
    return true;
}

/*---------------------------------------------------------*\
| Built-in Controller Database                              |
| Extended list of supported RGB devices                    |
\*---------------------------------------------------------*/
void HardwareScanner::InitializeBuiltinDatabase()
{
    // ==================== CORSAIR ====================
    // Keyboards
    controller_database[MakeKey(0x1B1C, 0x1B2D)] = {0x1B1C, 0x1B2D, "CorsairPeripheral", "Corsair K95 RGB Platinum", "keyboard"};
    controller_database[MakeKey(0x1B1C, 0x1B11)] = {0x1B1C, 0x1B11, "CorsairPeripheral", "Corsair K70 RGB", "keyboard"};
    controller_database[MakeKey(0x1B1C, 0x1B13)] = {0x1B1C, 0x1B13, "CorsairPeripheral", "Corsair K70 LUX RGB", "keyboard"};
    controller_database[MakeKey(0x1B1C, 0x1B49)] = {0x1B1C, 0x1B49, "CorsairPeripheral", "Corsair K70 RGB MK.2", "keyboard"};
    controller_database[MakeKey(0x1B1C, 0x1B6B)] = {0x1B1C, 0x1B6B, "CorsairPeripheral", "Corsair K70 RGB Pro", "keyboard"};
    controller_database[MakeKey(0x1B1C, 0x1B17)] = {0x1B1C, 0x1B17, "CorsairPeripheral", "Corsair K65 RGB", "keyboard"};
    controller_database[MakeKey(0x1B1C, 0x1B37)] = {0x1B1C, 0x1B37, "CorsairPeripheral", "Corsair K60 RGB Pro", "keyboard"};
    controller_database[MakeKey(0x1B1C, 0x1B4F)] = {0x1B1C, 0x1B4F, "CorsairPeripheral", "Corsair K100 RGB", "keyboard"};

    // Mice
    controller_database[MakeKey(0x1B1C, 0x1B2E)] = {0x1B1C, 0x1B2E, "CorsairPeripheral", "Corsair M65 RGB", "mouse"};
    controller_database[MakeKey(0x1B1C, 0x1B5A)] = {0x1B1C, 0x1B5A, "CorsairPeripheral", "Corsair Dark Core RGB", "mouse"};
    controller_database[MakeKey(0x1B1C, 0x1B34)] = {0x1B1C, 0x1B34, "CorsairPeripheral", "Corsair Scimitar RGB", "mouse"};
    controller_database[MakeKey(0x1B1C, 0x1B66)] = {0x1B1C, 0x1B66, "CorsairPeripheral", "Corsair Sabre RGB Pro", "mouse"};

    // Headsets
    controller_database[MakeKey(0x1B1C, 0x0A14)] = {0x1B1C, 0x0A14, "CorsairPeripheral", "Corsair Void RGB", "headset"};
    controller_database[MakeKey(0x1B1C, 0x0A55)] = {0x1B1C, 0x0A55, "CorsairPeripheral", "Corsair Virtuoso RGB", "headset"};

    // Lighting Controllers
    controller_database[MakeKey(0x1B1C, 0x0C0B)] = {0x1B1C, 0x0C0B, "CorsairLightingNode", "Corsair Lighting Node Pro", "ledstrip"};
    controller_database[MakeKey(0x1B1C, 0x0C1A)] = {0x1B1C, 0x0C1A, "CorsairCommanderCore", "Corsair Commander Core", "ledstrip"};
    controller_database[MakeKey(0x1B1C, 0x0C10)] = {0x1B1C, 0x0C10, "CorsairCommanderPro", "Corsair Commander Pro", "ledstrip"};
    controller_database[MakeKey(0x1B1C, 0x0C32)] = {0x1B1C, 0x0C32, "CorsairCommanderCore", "Corsair Commander Core XT", "ledstrip"};

    // ==================== RAZER ====================
    // Keyboards
    controller_database[MakeKey(0x1532, 0x0226)] = {0x1532, 0x0226, "RazerPeripheral", "Razer Huntsman Elite", "keyboard"};
    controller_database[MakeKey(0x1532, 0x0227)] = {0x1532, 0x0227, "RazerPeripheral", "Razer Huntsman", "keyboard"};
    controller_database[MakeKey(0x1532, 0x0246)] = {0x1532, 0x0246, "RazerPeripheral", "Razer Huntsman V2", "keyboard"};
    controller_database[MakeKey(0x1532, 0x026A)] = {0x1532, 0x026A, "RazerPeripheral", "Razer Huntsman V3 Pro", "keyboard"};
    controller_database[MakeKey(0x1532, 0x020F)] = {0x1532, 0x020F, "RazerPeripheral", "Razer BlackWidow Chroma", "keyboard"};
    controller_database[MakeKey(0x1532, 0x0221)] = {0x1532, 0x0221, "RazerPeripheral", "Razer BlackWidow Chroma V2", "keyboard"};
    controller_database[MakeKey(0x1532, 0x024E)] = {0x1532, 0x024E, "RazerPeripheral", "Razer BlackWidow V4", "keyboard"};
    controller_database[MakeKey(0x1532, 0x0282)] = {0x1532, 0x0282, "RazerPeripheral", "Razer Ornata V3", "keyboard"};

    // Mice
    controller_database[MakeKey(0x1532, 0x0084)] = {0x1532, 0x0084, "RazerPeripheral", "Razer DeathAdder V2", "mouse"};
    controller_database[MakeKey(0x1532, 0x007C)] = {0x1532, 0x007C, "RazerPeripheral", "Razer DeathAdder Elite", "mouse"};
    controller_database[MakeKey(0x1532, 0x0078)] = {0x1532, 0x0078, "RazerPeripheral", "Razer Basilisk", "mouse"};
    controller_database[MakeKey(0x1532, 0x0086)] = {0x1532, 0x0086, "RazerPeripheral", "Razer Basilisk V2", "mouse"};
    controller_database[MakeKey(0x1532, 0x0099)] = {0x1532, 0x0099, "RazerPeripheral", "Razer Basilisk V3", "mouse"};
    controller_database[MakeKey(0x1532, 0x0043)] = {0x1532, 0x0043, "RazerPeripheral", "Razer Mamba", "mouse"};
    controller_database[MakeKey(0x1532, 0x005B)] = {0x1532, 0x005B, "RazerPeripheral", "Razer Naga Trinity", "mouse"};
    controller_database[MakeKey(0x1532, 0x0083)] = {0x1532, 0x0083, "RazerPeripheral", "Razer Viper Ultimate", "mouse"};

    // Mousepads
    controller_database[MakeKey(0x1532, 0x0C00)] = {0x1532, 0x0C00, "RazerPeripheral", "Razer Firefly", "mousemat"};
    controller_database[MakeKey(0x1532, 0x0C04)] = {0x1532, 0x0C04, "RazerPeripheral", "Razer Firefly V2", "mousemat"};
    controller_database[MakeKey(0x1532, 0x0068)] = {0x1532, 0x0068, "RazerPeripheral", "Razer Goliathus Chroma", "mousemat"};

    // ==================== LOGITECH ====================
    controller_database[MakeKey(0x046D, 0xC336)] = {0x046D, 0xC336, "LogitechG", "Logitech G213", "keyboard"};
    controller_database[MakeKey(0x046D, 0xC339)] = {0x046D, 0xC339, "LogitechG", "Logitech G Pro Keyboard", "keyboard"};
    controller_database[MakeKey(0x046D, 0xC33C)] = {0x046D, 0xC33C, "LogitechG", "Logitech G513", "keyboard"};
    controller_database[MakeKey(0x046D, 0xC342)] = {0x046D, 0xC342, "LogitechG", "Logitech G512", "keyboard"};
    controller_database[MakeKey(0x046D, 0xC900)] = {0x046D, 0xC900, "LogitechG", "Logitech G915", "keyboard"};
    controller_database[MakeKey(0x046D, 0xC33F)] = {0x046D, 0xC33F, "LogitechG", "Logitech G815", "keyboard"};
    controller_database[MakeKey(0x046D, 0xC082)] = {0x046D, 0xC082, "LogitechG", "Logitech G502 Hero", "mouse"};
    controller_database[MakeKey(0x046D, 0xC08B)] = {0x046D, 0xC08B, "LogitechG", "Logitech G502 Lightspeed", "mouse"};
    controller_database[MakeKey(0x046D, 0xC084)] = {0x046D, 0xC084, "LogitechG", "Logitech G203", "mouse"};
    controller_database[MakeKey(0x046D, 0xC092)] = {0x046D, 0xC092, "LogitechG", "Logitech G Pro Wireless", "mouse"};
    controller_database[MakeKey(0x046D, 0xC088)] = {0x046D, 0xC088, "LogitechG", "Logitech G903", "mouse"};

    // ==================== STEELSERIES ====================
    controller_database[MakeKey(0x1038, 0x1610)] = {0x1038, 0x1610, "SteelSeriesApex", "SteelSeries Apex Pro", "keyboard"};
    controller_database[MakeKey(0x1038, 0x1612)] = {0x1038, 0x1612, "SteelSeriesApex", "SteelSeries Apex 7", "keyboard"};
    controller_database[MakeKey(0x1038, 0x161C)] = {0x1038, 0x161C, "SteelSeriesApex", "SteelSeries Apex 5", "keyboard"};
    controller_database[MakeKey(0x1038, 0x161A)] = {0x1038, 0x161A, "SteelSeriesApex", "SteelSeries Apex 3", "keyboard"};
    controller_database[MakeKey(0x1038, 0x184C)] = {0x1038, 0x184C, "SteelSeriesRival", "SteelSeries Rival 3", "mouse"};
    controller_database[MakeKey(0x1038, 0x1836)] = {0x1038, 0x1836, "SteelSeriesRival", "SteelSeries Aerox 3", "mouse"};
    controller_database[MakeKey(0x1038, 0x1724)] = {0x1038, 0x1724, "SteelSeriesRival", "SteelSeries Rival 600", "mouse", 0, 0xFFC0, -1};  // Interface 0, Usage Page 0xFFC0
    controller_database[MakeKey(0x1038, 0x1832)] = {0x1038, 0x1832, "SteelSeriesRival", "SteelSeries Sensei Ten", "mouse"};

    // ==================== ASUS ====================
    // Motherboards (AURA LED Controller)
    controller_database[MakeKey(0x0B05, 0x1866)] = {0x0B05, 0x1866, "AsusAuraMainboard", "ASUS Aura Motherboard", "motherboard"};
    controller_database[MakeKey(0x0B05, 0x18F3)] = {0x0B05, 0x18F3, "AsusAuraMainboard", "ASUS Aura Motherboard", "motherboard"};
    controller_database[MakeKey(0x0B05, 0x1939)] = {0x0B05, 0x1939, "AsusAuraMainboard", "ASUS Aura Motherboard", "motherboard"};
    controller_database[MakeKey(0x0B05, 0x19AF)] = {0x0B05, 0x19AF, "AsusAuraMainboard", "ASUS Aura Motherboard", "motherboard", 2, 0xFF72, -1};  // Interface 2, Usage Page 0xFF72
    controller_database[MakeKey(0x0B05, 0x1AA6)] = {0x0B05, 0x1AA6, "AsusAuraMainboard", "ASUS Aura Motherboard", "motherboard"};
    // Addressable LED Controllers
    controller_database[MakeKey(0x0B05, 0x1867)] = {0x0B05, 0x1867, "AsusAuraAddressable", "ASUS Aura Addressable Header", "ledstrip"};
    controller_database[MakeKey(0x0B05, 0x1872)] = {0x0B05, 0x1872, "AsusAuraAddressable", "ASUS Aura Addressable Header", "ledstrip"};
    controller_database[MakeKey(0x0B05, 0x1889)] = {0x0B05, 0x1889, "AsusAuraTerminal", "ASUS Aura Terminal", "ledstrip"};
    // Keyboards
    controller_database[MakeKey(0x0B05, 0x18F8)] = {0x0B05, 0x18F8, "AsusAuraKeyboard", "ASUS ROG Strix Scope", "keyboard"};
    controller_database[MakeKey(0x0B05, 0x190C)] = {0x0B05, 0x190C, "AsusAuraKeyboard", "ASUS ROG Strix Scope TKL", "keyboard"};
    controller_database[MakeKey(0x0B05, 0x1875)] = {0x0B05, 0x1875, "AsusAuraKeyboard", "ASUS ROG Strix Flare", "keyboard"};
    controller_database[MakeKey(0x0B05, 0x193C)] = {0x0B05, 0x193C, "AsusAuraKeyboard", "ASUS ROG Falchion", "keyboard"};
    // Mice
    controller_database[MakeKey(0x0B05, 0x1958)] = {0x0B05, 0x1958, "AsusAuraMouse", "ASUS ROG Gladius II", "mouse"};
    controller_database[MakeKey(0x0B05, 0x1A52)] = {0x0B05, 0x1A52, "AsusAuraMouse", "ASUS ROG Chakram", "mouse"};

    // ==================== EVISION KEYBOARDS ====================
    // (Used by many brands: Redragon, Tecware, GMMK, Womier, etc.)
    controller_database[MakeKey(0x3299, 0x4E9F)] = {0x3299, 0x4E9F, "EVisionKeyboard", "EVision RGB Keyboard", "keyboard", 1, 0xFF1C, -1};  // Interface 1, Usage Page 0xFF1C (Col04)
    controller_database[MakeKey(0x0C45, 0x5204)] = {0x0C45, 0x5204, "EVisionKeyboard", "Redragon K550", "keyboard"};
    controller_database[MakeKey(0x0C45, 0x5104)] = {0x0C45, 0x5104, "EVisionKeyboard", "Redragon K552", "keyboard"};
    controller_database[MakeKey(0x320F, 0x5064)] = {0x320F, 0x5064, "EVisionKeyboard", "GMMK TKL", "keyboard"};

    // ==================== MSI ====================
    controller_database[MakeKey(0x1462, 0x7C37)] = {0x1462, 0x7C37, "MSIMysticLight", "MSI Mystic Light Controller", "motherboard"};
    controller_database[MakeKey(0x1462, 0x7C91)] = {0x1462, 0x7C91, "MSIMysticLight", "MSI Mystic Light MS-7C91", "motherboard"};

    // ==================== NZXT ====================
    controller_database[MakeKey(0x1E71, 0x2006)] = {0x1E71, 0x2006, "NZXTHue2", "NZXT Hue 2", "ledstrip"};
    controller_database[MakeKey(0x1E71, 0x2001)] = {0x1E71, 0x2001, "NZXTHue2", "NZXT Smart Device V2", "ledstrip"};
    controller_database[MakeKey(0x1E71, 0x2007)] = {0x1E71, 0x2007, "NZXTHue2", "NZXT RGB & Fan Controller", "ledstrip"};
    controller_database[MakeKey(0x1E71, 0x2009)] = {0x1E71, 0x2009, "NZXTHue2", "NZXT RGB Controller", "ledstrip"};
    controller_database[MakeKey(0x1E71, 0x200D)] = {0x1E71, 0x200D, "NZXTKraken", "NZXT Kraken X", "cooler"};
    controller_database[MakeKey(0x1E71, 0x2010)] = {0x1E71, 0x2010, "NZXTKraken", "NZXT Kraken Z", "cooler"};

    // ==================== COOLER MASTER ====================
    controller_database[MakeKey(0x2516, 0x0051)] = {0x2516, 0x0051, "CoolerMasterARGB", "Cooler Master ARGB Controller", "ledstrip"};
    controller_database[MakeKey(0x2516, 0x004F)] = {0x2516, 0x004F, "CoolerMasterARGB", "Cooler Master Small ARGB", "ledstrip"};
    controller_database[MakeKey(0x2516, 0x0047)] = {0x2516, 0x0047, "CoolerMasterKeyboard", "Cooler Master SK650", "keyboard"};
    controller_database[MakeKey(0x2516, 0x0057)] = {0x2516, 0x0057, "CoolerMasterKeyboard", "Cooler Master MK730", "keyboard"};

    // ==================== GIGABYTE ====================
    controller_database[MakeKey(0x048D, 0x8297)] = {0x048D, 0x8297, "GigabyteRGBFusion", "Gigabyte RGB Fusion 2.0", "motherboard"};

    // ==================== HYPERX ====================
    controller_database[MakeKey(0x0951, 0x16CD)] = {0x0951, 0x16CD, "HyperXKeyboard", "HyperX Alloy Origins", "keyboard"};
    controller_database[MakeKey(0x0951, 0x16E5)] = {0x0951, 0x16E5, "HyperXKeyboard", "HyperX Alloy Elite 2", "keyboard"};
    controller_database[MakeKey(0x0951, 0x1727)] = {0x0951, 0x1727, "HyperXMouse", "HyperX Pulsefire Surge", "mouse"};
    controller_database[MakeKey(0x0951, 0x16B7)] = {0x0951, 0x16B7, "HyperXMouse", "HyperX Pulsefire FPS Pro", "mouse"};

    // ==================== ROCCAT ====================
    controller_database[MakeKey(0x1E7D, 0x2DD2)] = {0x1E7D, 0x2DD2, "RoccatKeyboard", "Roccat Vulcan 120", "keyboard"};
    controller_database[MakeKey(0x1E7D, 0x2FE6)] = {0x1E7D, 0x2FE6, "RoccatKeyboard", "Roccat Vulcan TKL", "keyboard"};
    controller_database[MakeKey(0x1E7D, 0x2E7C)] = {0x1E7D, 0x2E7C, "RoccatMouse", "Roccat Kone Pro", "mouse"};

    // ==================== G.SKILL (DDR5 RGB RAM) ====================
    // Note: These are SMBus devices, not USB HID
    // Detection happens via ScanI2C(), not ScanUSBHID()
    controller_database[MakeKey(0x0D62, 0x0001)] = {0x0D62, 0x0001, "GSkillTridentZ5", "G.Skill Trident Z5 RGB DDR5", "ram"};
    controller_database[MakeKey(0x0D62, 0x0002)] = {0x0D62, 0x0002, "GSkillTridentZ5", "G.Skill Trident Z5 Neo RGB DDR5", "ram"};
    controller_database[MakeKey(0x0D62, 0x0003)] = {0x0D62, 0x0003, "GSkillRipjaws", "G.Skill Ripjaws S5 RGB DDR5", "ram"};

    // ==================== TEAM GROUP (DDR5 RGB RAM) ====================
    controller_database[MakeKey(0x04D8, 0x0001)] = {0x04D8, 0x0001, "TeamGroupDelta", "TeamGroup T-Force Delta RGB DDR5", "ram"};

    // ==================== KINGSTON FURY (DDR5 RGB RAM) ====================
    controller_database[MakeKey(0x0951, 0xD001)] = {0x0951, 0xD001, "KingstonFury", "Kingston Fury Beast RGB DDR5", "ram"};
    controller_database[MakeKey(0x0951, 0xD002)] = {0x0951, 0xD002, "KingstonFury", "Kingston Fury Renegade RGB DDR5", "ram"};

    std::cout << "[DATABASE] Loaded " << controller_database.size() << " known RGB devices\n";
}

} // namespace OneClickRGB
