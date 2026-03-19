/*---------------------------------------------------------*\
| GSkillTridentZ5Controller.h                               |
|                                                           |
| Controller for G.Skill Trident Z5 RGB DDR5 RAM           |
| Communication via SMBus/I2C through SPD5 Hub             |
|                                                           |
| Based on OpenRGB GSkillDRAMController implementation     |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#pragma once

#include "../core/IRGBController.h"
#include "../smbus/SMBusInterface.h"
#include <memory>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| G.Skill DRAM Mode Definitions                             |
\*---------------------------------------------------------*/
enum class GSkillDRAMMode : uint8_t {
    Off             = 0x00,
    Static          = 0x01,
    Breathing       = 0x02,
    ColorCycle      = 0x03,
    Wave            = 0x04,
    Comet           = 0x05,
    FlashDash       = 0x06,
    Rainbow         = 0x07,
    DualRainbow     = 0x08,
    Starry          = 0x09,
    ColorShift      = 0x0A,
    Rain            = 0x0B,
    CustomPattern   = 0xFF,
};

/*---------------------------------------------------------*\
| G.Skill DRAM Register Definitions                         |
\*---------------------------------------------------------*/
namespace GSkillRegs {
    // Control registers
    constexpr uint8_t MODE              = 0x20;
    constexpr uint8_t SPEED             = 0x21;
    constexpr uint8_t DIRECTION         = 0x22;
    constexpr uint8_t BRIGHTNESS        = 0x23;

    // Color data (10 LEDs x 3 bytes = 30 bytes)
    constexpr uint8_t COLOR_START       = 0x30;
    constexpr uint8_t COLOR_END         = 0x4D;

    // Apply/Commit
    constexpr uint8_t APPLY             = 0x60;
    constexpr uint8_t SAVE              = 0x61;

    // Info registers
    constexpr uint8_t FIRMWARE_VERSION  = 0x70;
    constexpr uint8_t LED_COUNT         = 0x71;

    // Apply values
    constexpr uint8_t APPLY_COLORS      = 0x01;
    constexpr uint8_t APPLY_MODE        = 0x02;
    constexpr uint8_t APPLY_ALL         = 0x03;
    constexpr uint8_t SAVE_TO_EEPROM    = 0x01;
}

/*---------------------------------------------------------*\
| G.Skill Trident Z5 Controller Class                       |
\*---------------------------------------------------------*/
class GSkillTridentZ5Controller : public RGBControllerBase {
public:
    /*-----------------------------------------------------*\
    | Constructor / Destructor                              |
    \*-----------------------------------------------------*/
    GSkillTridentZ5Controller(std::shared_ptr<SMBusInterface> smbus, uint8_t spd_address);
    ~GSkillTridentZ5Controller() override;

    /*-----------------------------------------------------*\
    | IRGBController Interface                              |
    \*-----------------------------------------------------*/
    bool Open(const std::string& device_path) override;
    void Close() override;
    bool Initialize() override;
    bool Apply() override;
    bool SaveToDevice() override;

    // LED Control
    bool SetAllLEDs(const RGBColor& color) override;
    bool SetLEDColor(int led_index, const RGBColor& color) override;
    bool SetZoneColor(int zone_index, const RGBColor& color) override;

    // Mode Control
    bool SetMode(int mode_index) override;
    bool SetBrightness(uint8_t brightness) override;
    bool SetSpeed(uint8_t speed) override;

    // Packet interface (not used for SMBus)
    bool SendPacket(const uint8_t* data, size_t length) override;
    int ReceivePacket(uint8_t* buffer, size_t length, int timeout_ms = 100) override;

    /*-----------------------------------------------------*\
    | G.Skill Specific Methods                              |
    \*-----------------------------------------------------*/

    // Get the DIMM slot number (0-7)
    int GetDIMMSlot() const { return m_dimm_slot; }

    // Get the SPD5 Hub address
    uint8_t GetSPDAddress() const { return m_spd_address; }

    // Read module temperature (degrees C)
    float GetTemperature();

    // Read module information
    std::string GetModulePartNumber();
    uint32_t GetModuleCapacity();  // In MB

    // Direction control
    enum class Direction { Right = 0, Left = 1 };
    void SetDirection(Direction dir);

    /*-----------------------------------------------------*\
    | Static Detection Methods                              |
    \*-----------------------------------------------------*/

    // Detect G.Skill Trident Z5 RGB modules on the SMBus
    static std::vector<uint8_t> DetectModules(SMBusInterface* smbus);

    // Check if a specific address has a G.Skill RGB controller
    static bool IsGSkillRGB(SMBusInterface* smbus, uint8_t spd_addr);

private:
    std::shared_ptr<SMBusInterface> m_smbus;
    uint8_t m_spd_address;          // SPD5 Hub address (0x50-0x57)
    int m_dimm_slot;                // Derived from address
    uint8_t m_rgb_page;             // Vendor page for RGB control

    // LED configuration
    static constexpr int DEFAULT_LED_COUNT = 10;
    int m_led_count = DEFAULT_LED_COUNT;

    // Mode state
    GSkillDRAMMode m_current_mode = GSkillDRAMMode::Static;
    uint8_t m_direction = 0;

    /*-----------------------------------------------------*\
    | Internal Methods                                      |
    \*-----------------------------------------------------*/

    // Select the RGB vendor page on the SPD5 Hub
    bool SelectRGBPage();

    // Return to default SPD page
    bool SelectSPDPage();

    // Write a register value
    bool WriteRegister(uint8_t reg, uint8_t value);

    // Read a register value
    uint8_t ReadRegister(uint8_t reg);

    // Write a block of color data
    bool WriteColorBlock(const RGBColor* colors, int count);

    // Initialize mode list
    void InitializeModes();

    // Initialize zone list
    void InitializeZones();

    // Read LED count from controller
    int ReadLEDCount();
};

/*---------------------------------------------------------*\
| Factory Registration                                      |
\*---------------------------------------------------------*/

// Register this controller type with the device database
void RegisterGSkillTridentZ5();

} // namespace OneClickRGB
