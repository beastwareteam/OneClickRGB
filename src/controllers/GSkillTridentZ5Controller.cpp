/*---------------------------------------------------------*\
| GSkillTridentZ5Controller.cpp                             |
|                                                           |
| Controller for G.Skill Trident Z5 RGB DDR5 RAM           |
|                                                           |
| Based on OpenRGB GSkillDRAMController implementation     |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "GSkillTridentZ5Controller.h"
#include <iostream>
#include <cstring>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Constructor                                               |
\*---------------------------------------------------------*/
GSkillTridentZ5Controller::GSkillTridentZ5Controller(
    std::shared_ptr<SMBusInterface> smbus,
    uint8_t spd_address)
    : m_smbus(smbus)
    , m_spd_address(spd_address)
    , m_rgb_page(SPD5Hub::PAGE_VENDOR_1)
{
    // Derive DIMM slot from address
    m_dimm_slot = spd_address - SPD5Hub::ADDR_DIMM0;

    // Set device info
    m_vendor = "G.Skill";
    m_name = "Trident Z5 RGB DDR5";
    m_type = DeviceType::RAM;
    m_location = "DIMM" + std::to_string(m_dimm_slot);

    // Initialize with default LED count
    m_colors.resize(DEFAULT_LED_COUNT);
}

/*---------------------------------------------------------*\
| Destructor                                                |
\*---------------------------------------------------------*/
GSkillTridentZ5Controller::~GSkillTridentZ5Controller()
{
    Close();
}

/*---------------------------------------------------------*\
| Connection Management                                     |
\*---------------------------------------------------------*/
bool GSkillTridentZ5Controller::Open(const std::string& device_path)
{
    // SMBus devices don't need traditional "opening"
    // The SMBus interface should already be initialized

    if (!m_smbus || !m_smbus->IsInitialized())
    {
        std::cerr << "[GSkill] SMBus not initialized\n";
        return false;
    }

    // Verify the device is present
    if (!m_smbus->ProbeAddress(m_spd_address))
    {
        std::cerr << "[GSkill] No device at address 0x"
                  << std::hex << (int)m_spd_address << std::dec << "\n";
        return false;
    }

    m_is_open = true;
    m_location = device_path.empty() ?
        ("DIMM" + std::to_string(m_dimm_slot)) : device_path;

    return true;
}

void GSkillTridentZ5Controller::Close()
{
    if (m_is_open)
    {
        // Return to SPD page before closing
        SelectSPDPage();
        m_is_open = false;
    }
}

/*---------------------------------------------------------*\
| Initialization                                            |
\*---------------------------------------------------------*/
bool GSkillTridentZ5Controller::Initialize()
{
    if (!m_is_open)
    {
        if (!Open(""))
        {
            return false;
        }
    }

    // Verify this is a G.Skill RGB controller
    if (!IsGSkillRGB(m_smbus.get(), m_spd_address))
    {
        std::cerr << "[GSkill] Not a G.Skill RGB controller\n";
        return false;
    }

    // Select RGB page
    if (!SelectRGBPage())
    {
        std::cerr << "[GSkill] Failed to select RGB page\n";
        return false;
    }

    // Read actual LED count
    m_led_count = ReadLEDCount();
    if (m_led_count <= 0 || m_led_count > 32)
    {
        m_led_count = DEFAULT_LED_COUNT;
    }
    m_colors.resize(m_led_count);

    // Read firmware version
    uint8_t fw_ver = ReadRegister(GSkillRegs::FIRMWARE_VERSION);
    m_version = "v" + std::to_string(fw_ver >> 4) + "." + std::to_string(fw_ver & 0x0F);

    // Initialize modes and zones
    InitializeModes();
    InitializeZones();

    // Read current state
    m_brightness = ReadRegister(GSkillRegs::BRIGHTNESS);
    m_speed = ReadRegister(GSkillRegs::SPEED);
    m_direction = ReadRegister(GSkillRegs::DIRECTION);
    m_current_mode = static_cast<GSkillDRAMMode>(ReadRegister(GSkillRegs::MODE));

    std::cout << "[GSkill] Initialized " << m_name << " at DIMM" << m_dimm_slot
              << " (" << m_led_count << " LEDs, " << m_version << ")\n";

    return true;
}

/*---------------------------------------------------------*\
| Mode Initialization                                       |
\*---------------------------------------------------------*/
void GSkillTridentZ5Controller::InitializeModes()
{
    m_modes.clear();

    // Off
    m_modes.push_back({ "Off", static_cast<uint8_t>(GSkillDRAMMode::Off),
                        false, false, false, false, 0 });

    // Static
    m_modes.push_back({ "Static", static_cast<uint8_t>(GSkillDRAMMode::Static),
                        true, false, true, false, 1 });

    // Breathing
    m_modes.push_back({ "Breathing", static_cast<uint8_t>(GSkillDRAMMode::Breathing),
                        true, true, true, false, 1 });

    // Color Cycle
    m_modes.push_back({ "Color Cycle", static_cast<uint8_t>(GSkillDRAMMode::ColorCycle),
                        false, true, true, false, 0 });

    // Wave
    m_modes.push_back({ "Wave", static_cast<uint8_t>(GSkillDRAMMode::Wave),
                        true, true, true, true, 1 });

    // Comet
    m_modes.push_back({ "Comet", static_cast<uint8_t>(GSkillDRAMMode::Comet),
                        true, true, true, true, 1 });

    // Flash & Dash
    m_modes.push_back({ "Flash & Dash", static_cast<uint8_t>(GSkillDRAMMode::FlashDash),
                        true, true, true, true, 1 });

    // Rainbow
    m_modes.push_back({ "Rainbow", static_cast<uint8_t>(GSkillDRAMMode::Rainbow),
                        false, true, true, true, 0 });

    // Dual Rainbow
    m_modes.push_back({ "Dual Rainbow", static_cast<uint8_t>(GSkillDRAMMode::DualRainbow),
                        false, true, true, true, 0 });

    // Starry Night
    m_modes.push_back({ "Starry Night", static_cast<uint8_t>(GSkillDRAMMode::Starry),
                        true, true, true, false, 1 });

    // Color Shift
    m_modes.push_back({ "Color Shift", static_cast<uint8_t>(GSkillDRAMMode::ColorShift),
                        false, true, true, false, 0 });

    // Rain
    m_modes.push_back({ "Rain", static_cast<uint8_t>(GSkillDRAMMode::Rain),
                        true, true, true, false, 1 });
}

void GSkillTridentZ5Controller::InitializeZones()
{
    m_zones.clear();

    // Single zone for all LEDs
    DeviceZone zone;
    zone.name = "RAM Module";
    zone.led_count = static_cast<uint16_t>(m_led_count);
    zone.start_index = 0;
    m_zones.push_back(zone);
}

int GSkillTridentZ5Controller::ReadLEDCount()
{
    uint8_t count = ReadRegister(GSkillRegs::LED_COUNT);
    return (count > 0 && count <= 32) ? count : DEFAULT_LED_COUNT;
}

/*---------------------------------------------------------*\
| LED Control                                               |
\*---------------------------------------------------------*/
bool GSkillTridentZ5Controller::SetAllLEDs(const RGBColor& color)
{
    for (auto& c : m_colors)
    {
        c = color;
    }
    return true;
}

bool GSkillTridentZ5Controller::SetLEDColor(int led_index, const RGBColor& color)
{
    if (led_index < 0 || led_index >= static_cast<int>(m_colors.size()))
    {
        return false;
    }

    m_colors[led_index] = color;
    return true;
}

bool GSkillTridentZ5Controller::SetZoneColor(int zone_index, const RGBColor& color)
{
    if (zone_index != 0 || m_zones.empty())
    {
        return false;
    }

    return SetAllLEDs(color);
}

/*---------------------------------------------------------*\
| Mode Control                                              |
\*---------------------------------------------------------*/
bool GSkillTridentZ5Controller::SetMode(int mode_index)
{
    if (mode_index < 0 || mode_index >= static_cast<int>(m_modes.size()))
    {
        return false;
    }

    m_active_mode = mode_index;
    m_current_mode = static_cast<GSkillDRAMMode>(m_modes[mode_index].value);
    return true;
}

bool GSkillTridentZ5Controller::SetBrightness(uint8_t brightness)
{
    // Clamp to 0-100
    m_brightness = (brightness > 100) ? 100 : brightness;
    return true;
}

bool GSkillTridentZ5Controller::SetSpeed(uint8_t speed)
{
    // Clamp to 0-100
    m_speed = (speed > 100) ? 100 : speed;
    return true;
}

void GSkillTridentZ5Controller::SetDirection(Direction dir)
{
    m_direction = static_cast<uint8_t>(dir);
}

/*---------------------------------------------------------*\
| Apply Changes to Hardware                                 |
\*---------------------------------------------------------*/
bool GSkillTridentZ5Controller::Apply()
{
    if (!m_is_open || !m_smbus)
    {
        return false;
    }

    // Select RGB page
    if (!SelectRGBPage())
    {
        return false;
    }

    // Write mode settings
    if (!WriteRegister(GSkillRegs::MODE, static_cast<uint8_t>(m_current_mode)))
    {
        std::cerr << "[GSkill] Failed to write mode\n";
        return false;
    }

    if (!WriteRegister(GSkillRegs::BRIGHTNESS, m_brightness))
    {
        std::cerr << "[GSkill] Failed to write brightness\n";
        return false;
    }

    if (!WriteRegister(GSkillRegs::SPEED, m_speed))
    {
        std::cerr << "[GSkill] Failed to write speed\n";
        return false;
    }

    if (!WriteRegister(GSkillRegs::DIRECTION, m_direction))
    {
        std::cerr << "[GSkill] Failed to write direction\n";
        return false;
    }

    // Write color data
    if (!WriteColorBlock(m_colors.data(), static_cast<int>(m_colors.size())))
    {
        std::cerr << "[GSkill] Failed to write colors\n";
        return false;
    }

    // Commit changes
    if (!WriteRegister(GSkillRegs::APPLY, GSkillRegs::APPLY_ALL))
    {
        std::cerr << "[GSkill] Failed to apply changes\n";
        return false;
    }

    return true;
}

bool GSkillTridentZ5Controller::SaveToDevice()
{
    if (!m_is_open || !m_smbus)
    {
        return false;
    }

    // First apply current settings
    if (!Apply())
    {
        return false;
    }

    // Then save to EEPROM
    if (!WriteRegister(GSkillRegs::SAVE, GSkillRegs::SAVE_TO_EEPROM))
    {
        std::cerr << "[GSkill] Failed to save to EEPROM\n";
        return false;
    }

    std::cout << "[GSkill] Settings saved to device memory\n";
    return true;
}

/*---------------------------------------------------------*\
| Page Selection                                            |
\*---------------------------------------------------------*/
bool GSkillTridentZ5Controller::SelectRGBPage()
{
    return m_smbus->WriteByte(m_spd_address, SPD5Hub::PAGE_SELECT_REG, m_rgb_page);
}

bool GSkillTridentZ5Controller::SelectSPDPage()
{
    return m_smbus->WriteByte(m_spd_address, SPD5Hub::PAGE_SELECT_REG, SPD5Hub::PAGE_SPD);
}

/*---------------------------------------------------------*\
| Register Access                                           |
\*---------------------------------------------------------*/
bool GSkillTridentZ5Controller::WriteRegister(uint8_t reg, uint8_t value)
{
    return m_smbus->WriteByte(m_spd_address, reg, value);
}

uint8_t GSkillTridentZ5Controller::ReadRegister(uint8_t reg)
{
    return m_smbus->ReadByte(m_spd_address, reg);
}

bool GSkillTridentZ5Controller::WriteColorBlock(const RGBColor* colors, int count)
{
    // Write each LED's color (RGB format)
    for (int i = 0; i < count; i++)
    {
        uint8_t reg = GSkillRegs::COLOR_START + (i * 3);

        if (!WriteRegister(reg + 0, colors[i].r)) return false;
        if (!WriteRegister(reg + 1, colors[i].g)) return false;
        if (!WriteRegister(reg + 2, colors[i].b)) return false;
    }

    return true;
}

/*---------------------------------------------------------*\
| Temperature Reading                                       |
\*---------------------------------------------------------*/
float GSkillTridentZ5Controller::GetTemperature()
{
    // Temperature sensor is at a different address
    uint8_t temp_addr = SPD5Hub::GetTempAddress(m_dimm_slot);

    // Read temperature register (format depends on sensor)
    // Typical: high byte = integer, low byte = fraction
    uint16_t raw = m_smbus->ReadWord(temp_addr, 0x05);

    // Convert to Celsius (simplified)
    float temp = static_cast<float>(raw >> 8);
    temp += static_cast<float>(raw & 0xFF) / 256.0f;

    return temp;
}

/*---------------------------------------------------------*\
| Module Information                                        |
\*---------------------------------------------------------*/
std::string GSkillTridentZ5Controller::GetModulePartNumber()
{
    // Part number is in SPD EEPROM, bytes 329-348
    SelectSPDPage();

    std::string part_number;
    for (int i = 0; i < 20; i++)
    {
        uint8_t c = m_smbus->ReadByte(m_spd_address, 0x149 + i);  // 329 = 0x149
        if (c == 0 || c == 0xFF) break;
        if (c >= 0x20 && c <= 0x7E)
        {
            part_number += static_cast<char>(c);
        }
    }

    SelectRGBPage();
    return part_number;
}

uint32_t GSkillTridentZ5Controller::GetModuleCapacity()
{
    // Read density from SPD (simplified)
    SelectSPDPage();

    uint8_t density = m_smbus->ReadByte(m_spd_address, 0x04);

    // DDR5 density encoding
    uint32_t capacity_mb = 0;
    switch (density & 0x0F)
    {
        case 0x01: capacity_mb = 4096; break;   // 4 Gb
        case 0x02: capacity_mb = 8192; break;   // 8 Gb
        case 0x03: capacity_mb = 12288; break;  // 12 Gb
        case 0x04: capacity_mb = 16384; break;  // 16 Gb
        case 0x05: capacity_mb = 24576; break;  // 24 Gb
        case 0x06: capacity_mb = 32768; break;  // 32 Gb
        default: capacity_mb = 0;
    }

    SelectRGBPage();
    return capacity_mb;
}

/*---------------------------------------------------------*\
| Packet Interface (Not Used for SMBus)                     |
\*---------------------------------------------------------*/
bool GSkillTridentZ5Controller::SendPacket(const uint8_t* data, size_t length)
{
    // SMBus uses register-based access, not packets
    return false;
}

int GSkillTridentZ5Controller::ReceivePacket(uint8_t* buffer, size_t length, int timeout_ms)
{
    return -1;
}

/*---------------------------------------------------------*\
| Static Detection Methods                                  |
\*---------------------------------------------------------*/
std::vector<uint8_t> GSkillTridentZ5Controller::DetectModules(SMBusInterface* smbus)
{
    std::vector<uint8_t> found;

    if (!smbus || !smbus->IsInitialized())
    {
        return found;
    }

    std::cout << "[GSkill] Scanning for DDR5 RGB modules...\n";

    // Scan DDR5 SPD5 Hub addresses
    for (uint8_t addr = SPD5Hub::ADDR_DIMM0; addr <= SPD5Hub::ADDR_DIMM7; addr++)
    {
        // Check if there's a device at this address
        if (!smbus->ProbeAddress(addr))
        {
            continue;
        }

        // Check if it's a G.Skill RGB module
        if (IsGSkillRGB(smbus, addr))
        {
            std::cout << "  [+] Found G.Skill RGB at address 0x"
                      << std::hex << (int)addr << std::dec
                      << " (DIMM" << (addr - SPD5Hub::ADDR_DIMM0) << ")\n";
            found.push_back(addr);
        }
    }

    return found;
}

bool GSkillTridentZ5Controller::IsGSkillRGB(SMBusInterface* smbus, uint8_t spd_addr)
{
    // Read SPD manufacturer ID
    // DDR5 SPD bytes 512-513 contain the module manufacturer ID

    // First, select SPD page
    if (!smbus->WriteByte(spd_addr, SPD5Hub::PAGE_SELECT_REG, SPD5Hub::PAGE_SPD))
    {
        return false;
    }

    // Read manufacturer ID (bytes 512-513, continuation code + ID)
    // Note: Page 1 is needed for bytes 256-511, Page 0 for 0-255
    // For bytes 512+, we need to handle paging properly

    // Simplified check: try to select vendor RGB page and read signature
    if (!smbus->WriteByte(spd_addr, SPD5Hub::PAGE_SELECT_REG, SPD5Hub::PAGE_VENDOR_1))
    {
        return false;
    }

    // Read potential RGB controller signature
    uint8_t sig0 = smbus->ReadByte(spd_addr, 0x00);
    uint8_t sig1 = smbus->ReadByte(spd_addr, 0x01);

    // G.Skill RGB controllers typically have a recognizable signature
    // This needs to be validated with actual hardware

    // Known signatures for G.Skill/ENE RGB controllers:
    // - ENE-based controllers often have 0x5A as a signature
    // - G.Skill may use custom identifiers

    // Check for valid RGB controller response
    if (sig0 == 0xFF && sig1 == 0xFF)
    {
        // No RGB controller or not accessible
        return false;
    }

    // Additional checks:
    // - Try to read LED count register
    // - If we get a reasonable value (1-32), it's likely an RGB controller
    uint8_t led_count = smbus->ReadByte(spd_addr, GSkillRegs::LED_COUNT);
    if (led_count > 0 && led_count <= 32)
    {
        // Likely a valid RGB controller
        return true;
    }

    // Try reading mode register
    uint8_t mode = smbus->ReadByte(spd_addr, GSkillRegs::MODE);
    if (mode <= 0x0C || mode == 0xFF)  // Valid mode range
    {
        return true;
    }

    return false;
}

/*---------------------------------------------------------*\
| Factory Registration                                      |
\*---------------------------------------------------------*/
void RegisterGSkillTridentZ5()
{
    // This function would register the controller with the device database
    // For now, it's a placeholder for the factory pattern

    std::cout << "[GSkill] Trident Z5 RGB controller registered\n";
}

} // namespace OneClickRGB
