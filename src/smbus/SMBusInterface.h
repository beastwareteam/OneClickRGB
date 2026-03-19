/*---------------------------------------------------------*\
| SMBusInterface.h                                          |
|                                                           |
| Abstract interface for SMBus/I2C communication            |
| Supports Windows (WinRing0/inpoutx64) and Linux           |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| SMBus Controller Types                                    |
\*---------------------------------------------------------*/
enum class SMBusControllerType {
    Unknown,
    IntelPCH,           // Intel Platform Controller Hub
    AMDFCH,             // AMD Fusion Controller Hub
    NvidiaNForce,       // Older Nvidia chipsets
    VIA,                // VIA chipsets
};

/*---------------------------------------------------------*\
| SMBus Controller Info                                     |
\*---------------------------------------------------------*/
struct SMBusControllerInfo {
    SMBusControllerType type = SMBusControllerType::Unknown;
    uint16_t            base_address = 0;
    uint16_t            pci_vendor = 0;
    uint16_t            pci_device = 0;
    std::string         description;
};

/*---------------------------------------------------------*\
| SMBus Interface Abstract Class                            |
\*---------------------------------------------------------*/
class SMBusInterface {
public:
    virtual ~SMBusInterface() = default;

    /*-----------------------------------------------------*\
    | Initialization                                        |
    \*-----------------------------------------------------*/
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    /*-----------------------------------------------------*\
    | Controller Information                                |
    \*-----------------------------------------------------*/
    virtual SMBusControllerInfo GetControllerInfo() const = 0;

    /*-----------------------------------------------------*\
    | Basic SMBus Operations                                |
    \*-----------------------------------------------------*/

    // Quick Command (address-only, for probing)
    virtual bool QuickCommand(uint8_t addr, bool read) = 0;

    // Byte operations
    virtual bool SendByte(uint8_t addr, uint8_t data) = 0;
    virtual uint8_t ReceiveByte(uint8_t addr) = 0;

    // Byte Data operations (register read/write)
    virtual bool WriteByte(uint8_t addr, uint8_t reg, uint8_t data) = 0;
    virtual uint8_t ReadByte(uint8_t addr, uint8_t reg) = 0;

    // Word Data operations
    virtual bool WriteWord(uint8_t addr, uint8_t reg, uint16_t data) = 0;
    virtual uint16_t ReadWord(uint8_t addr, uint8_t reg) = 0;

    // Block operations
    virtual bool WriteBlock(uint8_t addr, uint8_t reg,
                            const uint8_t* data, uint8_t length) = 0;
    virtual int ReadBlock(uint8_t addr, uint8_t reg,
                          uint8_t* buffer, uint8_t max_length) = 0;

    /*-----------------------------------------------------*\
    | High-Level Operations                                 |
    \*-----------------------------------------------------*/

    // Probe for device presence at address
    virtual bool ProbeAddress(uint8_t addr);

    // Scan for all devices on the bus
    virtual std::vector<uint8_t> ScanBus(uint8_t start = 0x08, uint8_t end = 0x77);

    /*-----------------------------------------------------*\
    | Factory Method                                        |
    \*-----------------------------------------------------*/
    static std::unique_ptr<SMBusInterface> Create();
    static std::unique_ptr<SMBusInterface> CreateWinRing0();
    static std::unique_ptr<SMBusInterface> CreateInpOut();

protected:
    bool m_initialized = false;
    SMBusControllerInfo m_controller_info;

    // Wait for SMBus transaction to complete
    virtual bool WaitForNotBusy(int timeout_us = 10000) = 0;
    virtual bool WaitForComplete(int timeout_us = 10000) = 0;
};

/*---------------------------------------------------------*\
| Intel PCH SMBus Register Definitions                      |
\*---------------------------------------------------------*/
namespace IntelSMBus {
    // Register offsets from BAR
    constexpr uint16_t HOST_STATUS      = 0x00;
    constexpr uint16_t HOST_CONTROL     = 0x02;
    constexpr uint16_t HOST_COMMAND     = 0x03;
    constexpr uint16_t TRANSMIT_ADDR    = 0x04;
    constexpr uint16_t HOST_DATA0       = 0x05;
    constexpr uint16_t HOST_DATA1       = 0x06;
    constexpr uint16_t HOST_BLOCK_DB    = 0x07;
    constexpr uint16_t PEC              = 0x08;
    constexpr uint16_t RCV_SLVA         = 0x09;
    constexpr uint16_t SLV_DATA         = 0x0A;
    constexpr uint16_t AUX_STATUS       = 0x0C;
    constexpr uint16_t AUX_CONTROL      = 0x0D;
    constexpr uint16_t SMLINK_PIN_CTL   = 0x0E;
    constexpr uint16_t SMBUS_PIN_CTL    = 0x0F;
    constexpr uint16_t SLV_STATUS       = 0x10;
    constexpr uint16_t SLV_COMMAND      = 0x11;
    constexpr uint16_t NOTIFY_DADDR     = 0x14;
    constexpr uint16_t NOTIFY_DLOW      = 0x16;
    constexpr uint16_t NOTIFY_DHIGH     = 0x17;

    // Host Status bits
    constexpr uint8_t STS_HOST_BUSY     = 0x01;
    constexpr uint8_t STS_INTR          = 0x02;
    constexpr uint8_t STS_DEV_ERR       = 0x04;
    constexpr uint8_t STS_BUS_ERR       = 0x08;
    constexpr uint8_t STS_FAILED        = 0x10;
    constexpr uint8_t STS_SMBALERT      = 0x20;
    constexpr uint8_t STS_INUSE         = 0x40;
    constexpr uint8_t STS_BYTE_DONE     = 0x80;

    // Host Control bits
    constexpr uint8_t CTL_INTREN        = 0x01;
    constexpr uint8_t CTL_KILL          = 0x02;
    constexpr uint8_t CTL_CMD_MASK      = 0x1C;
    constexpr uint8_t CTL_LAST_BYTE     = 0x20;
    constexpr uint8_t CTL_START         = 0x40;
    constexpr uint8_t CTL_PEC_EN        = 0x80;

    // Command types (bits 2-4 of HOST_CONTROL)
    constexpr uint8_t CMD_QUICK         = 0x00;
    constexpr uint8_t CMD_BYTE          = 0x04;
    constexpr uint8_t CMD_BYTE_DATA     = 0x08;
    constexpr uint8_t CMD_WORD_DATA     = 0x0C;
    constexpr uint8_t CMD_PROCESS_CALL  = 0x10;
    constexpr uint8_t CMD_BLOCK         = 0x14;
    constexpr uint8_t CMD_I2C_READ      = 0x18;
    constexpr uint8_t CMD_BLOCK_PROCESS = 0x1C;

    // Common Intel SMBus PCI Device IDs
    constexpr uint16_t PCI_DEVICE_ICL   = 0x02A3;   // Ice Lake
    constexpr uint16_t PCI_DEVICE_TGL   = 0xA0A3;   // Tiger Lake
    constexpr uint16_t PCI_DEVICE_ADL_P = 0x51A3;   // Alder Lake-P
    constexpr uint16_t PCI_DEVICE_ADL_S = 0x7AA3;   // Alder Lake-S
    constexpr uint16_t PCI_DEVICE_RPL_S = 0x7A23;   // Raptor Lake-S
    constexpr uint16_t PCI_DEVICE_CML   = 0x06A3;   // Comet Lake
    constexpr uint16_t PCI_DEVICE_CNL   = 0xA323;   // Cannon Lake
}

/*---------------------------------------------------------*\
| AMD FCH SMBus Register Definitions                        |
\*---------------------------------------------------------*/
namespace AMDSMBus {
    // Register offsets
    constexpr uint16_t STATUS           = 0x00;
    constexpr uint16_t SLAVE_STATUS     = 0x01;
    constexpr uint16_t CONTROL          = 0x02;
    constexpr uint16_t HOST_CMD         = 0x03;
    constexpr uint16_t ADDRESS          = 0x04;
    constexpr uint16_t DATA0            = 0x05;
    constexpr uint16_t DATA1            = 0x06;
    constexpr uint16_t BLOCK_DATA       = 0x07;
    constexpr uint16_t SLAVE_CONTROL    = 0x08;
    constexpr uint16_t SHADOW_CMD       = 0x09;
    constexpr uint16_t SLAVE_EVENT      = 0x0A;
    constexpr uint16_t SLAVE_DATA       = 0x0C;

    // Status bits
    constexpr uint8_t STS_DONE          = 0x80;
    constexpr uint8_t STS_ABORT         = 0x20;
    constexpr uint8_t STS_COLLISION     = 0x10;
    constexpr uint8_t STS_PROT_ERR      = 0x08;
    constexpr uint8_t STS_HOST_BUSY     = 0x01;

    // Control/Protocol values
    constexpr uint8_t PROTO_QUICK       = 0x00;
    constexpr uint8_t PROTO_BYTE        = 0x04;
    constexpr uint8_t PROTO_BYTE_DATA   = 0x08;
    constexpr uint8_t PROTO_WORD_DATA   = 0x0C;
    constexpr uint8_t PROTO_BLOCK_DATA  = 0x14;

    // Default base address (may vary by system)
    constexpr uint16_t DEFAULT_BASE     = 0x0B00;
}

/*---------------------------------------------------------*\
| DDR5 SPD5 Hub Definitions                                 |
\*---------------------------------------------------------*/
namespace SPD5Hub {
    // Address range for DDR5 SPD5 Hubs (one per DIMM slot)
    constexpr uint8_t ADDR_DIMM0 = 0x50;
    constexpr uint8_t ADDR_DIMM1 = 0x51;
    constexpr uint8_t ADDR_DIMM2 = 0x52;
    constexpr uint8_t ADDR_DIMM3 = 0x53;
    constexpr uint8_t ADDR_DIMM4 = 0x54;
    constexpr uint8_t ADDR_DIMM5 = 0x55;
    constexpr uint8_t ADDR_DIMM6 = 0x56;
    constexpr uint8_t ADDR_DIMM7 = 0x57;

    // Temperature sensor addresses
    constexpr uint8_t TEMP_DIMM0 = 0x18;
    constexpr uint8_t TEMP_DIMM1 = 0x19;
    constexpr uint8_t TEMP_DIMM2 = 0x1A;
    constexpr uint8_t TEMP_DIMM3 = 0x1B;

    // Page selection register
    constexpr uint8_t PAGE_SELECT_REG = 0x0B;

    // Page values
    constexpr uint8_t PAGE_SPD        = 0x00;   // SPD EEPROM
    constexpr uint8_t PAGE_NVRAM      = 0x01;   // Non-volatile storage
    constexpr uint8_t PAGE_TEMP       = 0x02;   // Temperature sensor
    constexpr uint8_t PAGE_VENDOR_1   = 0x04;   // Vendor-specific (RGB)
    constexpr uint8_t PAGE_VENDOR_2   = 0x05;   // Vendor-specific

    // Helper functions
    inline uint8_t GetHubAddress(int dimm_slot) {
        return static_cast<uint8_t>(0x50 + (dimm_slot & 0x07));
    }

    inline uint8_t GetTempAddress(int dimm_slot) {
        return static_cast<uint8_t>(0x18 + (dimm_slot & 0x07));
    }
}

} // namespace OneClickRGB
