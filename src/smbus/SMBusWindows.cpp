/*---------------------------------------------------------*\
| SMBusWindows.cpp                                          |
|                                                           |
| Windows SMBus implementations using WinRing0/inpoutx64    |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "SMBusWindows.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| SMBusInterface Base Implementation                        |
\*---------------------------------------------------------*/
bool SMBusInterface::ProbeAddress(uint8_t addr)
{
    // Try a quick read to detect presence
    return QuickCommand(addr, true);
}

std::vector<uint8_t> SMBusInterface::ScanBus(uint8_t start, uint8_t end)
{
    std::vector<uint8_t> found;

    for (uint8_t addr = start; addr <= end; addr++)
    {
        // Skip reserved addresses
        if (addr < 0x08 || addr > 0x77) continue;

        if (ProbeAddress(addr))
        {
            found.push_back(addr);
        }
    }

    return found;
}

std::unique_ptr<SMBusInterface> SMBusInterface::Create()
{
    // Try WinRing0 first
    auto winring0 = CreateWinRing0();
    if (winring0 && winring0->Initialize())
    {
        return winring0;
    }

    // Fall back to inpoutx64
    auto inpout = CreateInpOut();
    if (inpout && inpout->Initialize())
    {
        return inpout;
    }

    return nullptr;
}

std::unique_ptr<SMBusInterface> SMBusInterface::CreateWinRing0()
{
    return std::make_unique<WinRing0SMBus>();
}

std::unique_ptr<SMBusInterface> SMBusInterface::CreateInpOut()
{
    return std::make_unique<InpOutSMBus>();
}

/*---------------------------------------------------------*\
| WinRing0SMBus Implementation                              |
\*---------------------------------------------------------*/
WinRing0SMBus::WinRing0SMBus()
{
}

WinRing0SMBus::~WinRing0SMBus()
{
    Shutdown();
}

bool WinRing0SMBus::Initialize()
{
    if (m_initialized)
    {
        return true;
    }

    if (!LoadWinRing0())
    {
        std::cerr << "[WinRing0] Failed to load WinRing0 library\n";
        return false;
    }

    if (!DetectSMBusController())
    {
        std::cerr << "[WinRing0] Failed to detect SMBus controller\n";
        UnloadWinRing0();
        return false;
    }

    m_initialized = true;
    std::cout << "[WinRing0] Initialized SMBus at base 0x"
              << std::hex << m_controller_info.base_address << std::dec
              << " (" << m_controller_info.description << ")\n";

    return true;
}

void WinRing0SMBus::Shutdown()
{
    if (m_initialized)
    {
        UnloadWinRing0();
        m_initialized = false;
    }
}

bool WinRing0SMBus::LoadWinRing0()
{
    // Try different DLL names
    const char* dll_names[] = {
        "WinRing0x64.dll",
        "WinRing0.dll",
        "OlsApi.dll"
    };

    for (const char* name : dll_names)
    {
        m_dll_handle = LoadLibraryA(name);
        if (m_dll_handle) break;
    }

    if (!m_dll_handle)
    {
        return false;
    }

    // Load function pointers
    m_InitializeOls = (InitializeOlsType)GetProcAddress(m_dll_handle, "InitializeOls");
    m_DeinitializeOls = (DeinitializeOlsType)GetProcAddress(m_dll_handle, "DeinitializeOls");
    m_GetDllStatus = (GetDllStatusType)GetProcAddress(m_dll_handle, "GetDllStatus");
    m_ReadIoPortByte = (ReadIoPortByteType)GetProcAddress(m_dll_handle, "ReadIoPortByte");
    m_WriteIoPortByte = (WriteIoPortByteType)GetProcAddress(m_dll_handle, "WriteIoPortByte");
    m_ReadIoPortWord = (ReadIoPortWordType)GetProcAddress(m_dll_handle, "ReadIoPortWord");
    m_WriteIoPortWord = (WriteIoPortWordType)GetProcAddress(m_dll_handle, "WriteIoPortWord");
    m_ReadPciConfigDwordEx = (ReadPciConfigDwordExType)GetProcAddress(m_dll_handle, "ReadPciConfigDwordEx");
    m_FindPciDeviceById = (FindPciDeviceByIdType)GetProcAddress(m_dll_handle, "FindPciDeviceById");

    if (!m_InitializeOls || !m_DeinitializeOls ||
        !m_ReadIoPortByte || !m_WriteIoPortByte)
    {
        FreeLibrary(m_dll_handle);
        m_dll_handle = nullptr;
        return false;
    }

    // Initialize the driver
    if (!m_InitializeOls())
    {
        FreeLibrary(m_dll_handle);
        m_dll_handle = nullptr;
        return false;
    }

    // Check driver status
    if (m_GetDllStatus)
    {
        DWORD status = m_GetDllStatus();
        if (status != 0)
        {
            std::cerr << "[WinRing0] Driver status: " << status << "\n";
            // 0 = OK, 1 = DLL not found, 2 = driver not loaded, etc.
        }
    }

    return true;
}

void WinRing0SMBus::UnloadWinRing0()
{
    if (m_dll_handle)
    {
        if (m_DeinitializeOls)
        {
            m_DeinitializeOls();
        }
        FreeLibrary(m_dll_handle);
        m_dll_handle = nullptr;
    }

    m_InitializeOls = nullptr;
    m_DeinitializeOls = nullptr;
    m_GetDllStatus = nullptr;
    m_ReadIoPortByte = nullptr;
    m_WriteIoPortByte = nullptr;
    m_ReadIoPortWord = nullptr;
    m_WriteIoPortWord = nullptr;
    m_ReadPciConfigDwordEx = nullptr;
    m_FindPciDeviceById = nullptr;
}

bool WinRing0SMBus::DetectSMBusController()
{
    // Try Intel first (most common)
    if (DetectIntelSMBus())
    {
        return true;
    }

    // Try AMD
    if (DetectAMDSMBus())
    {
        return true;
    }

    return false;
}

bool WinRing0SMBus::DetectIntelSMBus()
{
    // Intel SMBus is at Bus 0, Device 31, Function 4
    // PCI address format for WinRing0: (bus << 8) | (dev << 3) | func
    DWORD pci_addr = (0 << 8) | (31 << 3) | 4;

    // Read Vendor ID (offset 0x00)
    DWORD vendor_device = 0;
    if (!m_ReadPciConfigDwordEx || !m_ReadPciConfigDwordEx(pci_addr, 0x00, &vendor_device))
    {
        return false;
    }

    uint16_t vendor_id = vendor_device & 0xFFFF;
    uint16_t device_id = (vendor_device >> 16) & 0xFFFF;

    // Check for Intel vendor ID
    if (vendor_id != 0x8086)
    {
        return false;
    }

    // Read BAR0 (offset 0x20) for base address
    DWORD bar0 = 0;
    if (!m_ReadPciConfigDwordEx(pci_addr, 0x20, &bar0))
    {
        return false;
    }

    // BAR0 is I/O space, mask off bit 0
    uint16_t base_addr = static_cast<uint16_t>(bar0 & 0xFFFE);

    if (base_addr == 0 || base_addr == 0xFFFE)
    {
        return false;
    }

    m_controller_info.type = SMBusControllerType::IntelPCH;
    m_controller_info.base_address = base_addr;
    m_controller_info.pci_vendor = vendor_id;
    m_controller_info.pci_device = device_id;

    // Set description based on device ID
    switch (device_id)
    {
        case IntelSMBus::PCI_DEVICE_ADL_S:
            m_controller_info.description = "Intel Alder Lake-S SMBus";
            break;
        case IntelSMBus::PCI_DEVICE_RPL_S:
            m_controller_info.description = "Intel Raptor Lake-S SMBus";
            break;
        case IntelSMBus::PCI_DEVICE_TGL:
            m_controller_info.description = "Intel Tiger Lake SMBus";
            break;
        case IntelSMBus::PCI_DEVICE_CML:
            m_controller_info.description = "Intel Comet Lake SMBus";
            break;
        default:
            m_controller_info.description = "Intel PCH SMBus";
            break;
    }

    return true;
}

bool WinRing0SMBus::DetectAMDSMBus()
{
    // AMD SMBus detection is more complex
    // Common location is via ACPI or fixed port

    // Try common AMD SMBus base addresses
    const uint16_t amd_bases[] = { 0x0B00, 0x0B20 };

    for (uint16_t base : amd_bases)
    {
        // Try to read status register
        uint8_t status = InPortByte(base + AMDSMBus::STATUS);

        // Check for valid status (not 0xFF which indicates no device)
        if (status != 0xFF)
        {
            m_controller_info.type = SMBusControllerType::AMDFCH;
            m_controller_info.base_address = base;
            m_controller_info.description = "AMD FCH SMBus";
            return true;
        }
    }

    return false;
}

uint8_t WinRing0SMBus::InPortByte(uint16_t port)
{
    if (m_ReadIoPortByte)
    {
        return m_ReadIoPortByte(port);
    }
    return 0xFF;
}

void WinRing0SMBus::OutPortByte(uint16_t port, uint8_t value)
{
    if (m_WriteIoPortByte)
    {
        m_WriteIoPortByte(port, value);
    }
}

uint16_t WinRing0SMBus::InPortWord(uint16_t port)
{
    if (m_ReadIoPortWord)
    {
        return m_ReadIoPortWord(port);
    }
    return 0xFFFF;
}

void WinRing0SMBus::OutPortWord(uint16_t port, uint16_t value)
{
    if (m_WriteIoPortWord)
    {
        m_WriteIoPortWord(port, value);
    }
}

uint32_t WinRing0SMBus::ReadPciConfig(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg)
{
    if (!m_ReadPciConfigDwordEx)
    {
        return 0xFFFFFFFF;
    }

    DWORD pci_addr = (bus << 8) | (dev << 3) | func;
    DWORD value = 0;
    m_ReadPciConfigDwordEx(pci_addr, reg, &value);
    return value;
}

void WinRing0SMBus::ClearStatus()
{
    uint16_t base = m_controller_info.base_address;

    if (m_controller_info.type == SMBusControllerType::IntelPCH)
    {
        // Clear all status bits by writing 1s
        OutPortByte(base + IntelSMBus::HOST_STATUS, 0xFF);
    }
    else if (m_controller_info.type == SMBusControllerType::AMDFCH)
    {
        OutPortByte(base + AMDSMBus::STATUS, 0xFF);
    }
}

uint8_t WinRing0SMBus::GetStatus()
{
    uint16_t base = m_controller_info.base_address;

    if (m_controller_info.type == SMBusControllerType::IntelPCH)
    {
        return InPortByte(base + IntelSMBus::HOST_STATUS);
    }
    else if (m_controller_info.type == SMBusControllerType::AMDFCH)
    {
        return InPortByte(base + AMDSMBus::STATUS);
    }

    return 0xFF;
}

bool WinRing0SMBus::WaitForNotBusy(int timeout_us)
{
    uint16_t base = m_controller_info.base_address;
    int elapsed = 0;

    while (elapsed < timeout_us)
    {
        uint8_t status = GetStatus();

        if (m_controller_info.type == SMBusControllerType::IntelPCH)
        {
            if (!(status & IntelSMBus::STS_HOST_BUSY))
            {
                return true;
            }
        }
        else if (m_controller_info.type == SMBusControllerType::AMDFCH)
        {
            if (!(status & AMDSMBus::STS_HOST_BUSY))
            {
                return true;
            }
        }

        std::this_thread::sleep_for(std::chrono::microseconds(10));
        elapsed += 10;
    }

    return false;
}

bool WinRing0SMBus::WaitForComplete(int timeout_us)
{
    uint16_t base = m_controller_info.base_address;
    int elapsed = 0;

    while (elapsed < timeout_us)
    {
        uint8_t status = GetStatus();

        if (m_controller_info.type == SMBusControllerType::IntelPCH)
        {
            // Check for completion or error
            if (status & (IntelSMBus::STS_DEV_ERR | IntelSMBus::STS_BUS_ERR |
                          IntelSMBus::STS_FAILED))
            {
                ClearStatus();
                return false;
            }
            if (status & IntelSMBus::STS_INTR)
            {
                ClearStatus();
                return true;
            }
        }
        else if (m_controller_info.type == SMBusControllerType::AMDFCH)
        {
            if (status & (AMDSMBus::STS_ABORT | AMDSMBus::STS_COLLISION |
                          AMDSMBus::STS_PROT_ERR))
            {
                ClearStatus();
                return false;
            }
            if (status & AMDSMBus::STS_DONE)
            {
                ClearStatus();
                return true;
            }
        }

        std::this_thread::sleep_for(std::chrono::microseconds(10));
        elapsed += 10;
    }

    return false;
}

bool WinRing0SMBus::QuickCommand(uint8_t addr, bool read)
{
    if (!m_initialized) return false;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return false;
    ClearStatus();

    if (m_controller_info.type == SMBusControllerType::IntelPCH)
    {
        OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1) | (read ? 1 : 0));
        OutPortByte(base + IntelSMBus::HOST_CONTROL,
                    IntelSMBus::CMD_QUICK | IntelSMBus::CTL_START);
    }
    else if (m_controller_info.type == SMBusControllerType::AMDFCH)
    {
        OutPortByte(base + AMDSMBus::ADDRESS, (addr << 1) | (read ? 1 : 0));
        OutPortByte(base + AMDSMBus::CONTROL, AMDSMBus::PROTO_QUICK);
    }

    return WaitForComplete();
}

bool WinRing0SMBus::SendByte(uint8_t addr, uint8_t data)
{
    if (!m_initialized) return false;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return false;
    ClearStatus();

    if (m_controller_info.type == SMBusControllerType::IntelPCH)
    {
        OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1));
        OutPortByte(base + IntelSMBus::HOST_COMMAND, data);
        OutPortByte(base + IntelSMBus::HOST_CONTROL,
                    IntelSMBus::CMD_BYTE | IntelSMBus::CTL_START);
    }
    else if (m_controller_info.type == SMBusControllerType::AMDFCH)
    {
        OutPortByte(base + AMDSMBus::ADDRESS, (addr << 1));
        OutPortByte(base + AMDSMBus::HOST_CMD, data);
        OutPortByte(base + AMDSMBus::CONTROL, AMDSMBus::PROTO_BYTE);
    }

    return WaitForComplete();
}

uint8_t WinRing0SMBus::ReceiveByte(uint8_t addr)
{
    if (!m_initialized) return 0xFF;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return 0xFF;
    ClearStatus();

    if (m_controller_info.type == SMBusControllerType::IntelPCH)
    {
        OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1) | 1);
        OutPortByte(base + IntelSMBus::HOST_CONTROL,
                    IntelSMBus::CMD_BYTE | IntelSMBus::CTL_START);

        if (!WaitForComplete()) return 0xFF;

        return InPortByte(base + IntelSMBus::HOST_DATA0);
    }
    else if (m_controller_info.type == SMBusControllerType::AMDFCH)
    {
        OutPortByte(base + AMDSMBus::ADDRESS, (addr << 1) | 1);
        OutPortByte(base + AMDSMBus::CONTROL, AMDSMBus::PROTO_BYTE);

        if (!WaitForComplete()) return 0xFF;

        return InPortByte(base + AMDSMBus::DATA0);
    }

    return 0xFF;
}

bool WinRing0SMBus::WriteByte(uint8_t addr, uint8_t reg, uint8_t data)
{
    if (!m_initialized) return false;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return false;
    ClearStatus();

    if (m_controller_info.type == SMBusControllerType::IntelPCH)
    {
        OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1));
        OutPortByte(base + IntelSMBus::HOST_COMMAND, reg);
        OutPortByte(base + IntelSMBus::HOST_DATA0, data);
        OutPortByte(base + IntelSMBus::HOST_CONTROL,
                    IntelSMBus::CMD_BYTE_DATA | IntelSMBus::CTL_START);
    }
    else if (m_controller_info.type == SMBusControllerType::AMDFCH)
    {
        OutPortByte(base + AMDSMBus::ADDRESS, (addr << 1));
        OutPortByte(base + AMDSMBus::HOST_CMD, reg);
        OutPortByte(base + AMDSMBus::DATA0, data);
        OutPortByte(base + AMDSMBus::CONTROL, AMDSMBus::PROTO_BYTE_DATA);
    }

    return WaitForComplete();
}

uint8_t WinRing0SMBus::ReadByte(uint8_t addr, uint8_t reg)
{
    if (!m_initialized) return 0xFF;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return 0xFF;
    ClearStatus();

    if (m_controller_info.type == SMBusControllerType::IntelPCH)
    {
        OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1) | 1);
        OutPortByte(base + IntelSMBus::HOST_COMMAND, reg);
        OutPortByte(base + IntelSMBus::HOST_CONTROL,
                    IntelSMBus::CMD_BYTE_DATA | IntelSMBus::CTL_START);

        if (!WaitForComplete()) return 0xFF;

        return InPortByte(base + IntelSMBus::HOST_DATA0);
    }
    else if (m_controller_info.type == SMBusControllerType::AMDFCH)
    {
        OutPortByte(base + AMDSMBus::ADDRESS, (addr << 1) | 1);
        OutPortByte(base + AMDSMBus::HOST_CMD, reg);
        OutPortByte(base + AMDSMBus::CONTROL, AMDSMBus::PROTO_BYTE_DATA);

        if (!WaitForComplete()) return 0xFF;

        return InPortByte(base + AMDSMBus::DATA0);
    }

    return 0xFF;
}

bool WinRing0SMBus::WriteWord(uint8_t addr, uint8_t reg, uint16_t data)
{
    if (!m_initialized) return false;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return false;
    ClearStatus();

    if (m_controller_info.type == SMBusControllerType::IntelPCH)
    {
        OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1));
        OutPortByte(base + IntelSMBus::HOST_COMMAND, reg);
        OutPortByte(base + IntelSMBus::HOST_DATA0, data & 0xFF);
        OutPortByte(base + IntelSMBus::HOST_DATA1, (data >> 8) & 0xFF);
        OutPortByte(base + IntelSMBus::HOST_CONTROL,
                    IntelSMBus::CMD_WORD_DATA | IntelSMBus::CTL_START);
    }
    else if (m_controller_info.type == SMBusControllerType::AMDFCH)
    {
        OutPortByte(base + AMDSMBus::ADDRESS, (addr << 1));
        OutPortByte(base + AMDSMBus::HOST_CMD, reg);
        OutPortByte(base + AMDSMBus::DATA0, data & 0xFF);
        OutPortByte(base + AMDSMBus::DATA1, (data >> 8) & 0xFF);
        OutPortByte(base + AMDSMBus::CONTROL, AMDSMBus::PROTO_WORD_DATA);
    }

    return WaitForComplete();
}

uint16_t WinRing0SMBus::ReadWord(uint8_t addr, uint8_t reg)
{
    if (!m_initialized) return 0xFFFF;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return 0xFFFF;
    ClearStatus();

    if (m_controller_info.type == SMBusControllerType::IntelPCH)
    {
        OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1) | 1);
        OutPortByte(base + IntelSMBus::HOST_COMMAND, reg);
        OutPortByte(base + IntelSMBus::HOST_CONTROL,
                    IntelSMBus::CMD_WORD_DATA | IntelSMBus::CTL_START);

        if (!WaitForComplete()) return 0xFFFF;

        uint8_t low = InPortByte(base + IntelSMBus::HOST_DATA0);
        uint8_t high = InPortByte(base + IntelSMBus::HOST_DATA1);
        return (high << 8) | low;
    }
    else if (m_controller_info.type == SMBusControllerType::AMDFCH)
    {
        OutPortByte(base + AMDSMBus::ADDRESS, (addr << 1) | 1);
        OutPortByte(base + AMDSMBus::HOST_CMD, reg);
        OutPortByte(base + AMDSMBus::CONTROL, AMDSMBus::PROTO_WORD_DATA);

        if (!WaitForComplete()) return 0xFFFF;

        uint8_t low = InPortByte(base + AMDSMBus::DATA0);
        uint8_t high = InPortByte(base + AMDSMBus::DATA1);
        return (high << 8) | low;
    }

    return 0xFFFF;
}

bool WinRing0SMBus::WriteBlock(uint8_t addr, uint8_t reg,
                                const uint8_t* data, uint8_t length)
{
    if (!m_initialized || length == 0 || length > 32) return false;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return false;
    ClearStatus();

    if (m_controller_info.type == SMBusControllerType::IntelPCH)
    {
        OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1));
        OutPortByte(base + IntelSMBus::HOST_COMMAND, reg);
        OutPortByte(base + IntelSMBus::HOST_DATA0, length);

        // Write data to block data buffer
        for (uint8_t i = 0; i < length; i++)
        {
            OutPortByte(base + IntelSMBus::HOST_BLOCK_DB, data[i]);
        }

        OutPortByte(base + IntelSMBus::HOST_CONTROL,
                    IntelSMBus::CMD_BLOCK | IntelSMBus::CTL_START);
    }
    else if (m_controller_info.type == SMBusControllerType::AMDFCH)
    {
        OutPortByte(base + AMDSMBus::ADDRESS, (addr << 1));
        OutPortByte(base + AMDSMBus::HOST_CMD, reg);
        OutPortByte(base + AMDSMBus::DATA0, length);

        for (uint8_t i = 0; i < length; i++)
        {
            OutPortByte(base + AMDSMBus::BLOCK_DATA, data[i]);
        }

        OutPortByte(base + AMDSMBus::CONTROL, AMDSMBus::PROTO_BLOCK_DATA);
    }

    return WaitForComplete();
}

int WinRing0SMBus::ReadBlock(uint8_t addr, uint8_t reg,
                              uint8_t* buffer, uint8_t max_length)
{
    if (!m_initialized || max_length == 0) return -1;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return -1;
    ClearStatus();

    if (m_controller_info.type == SMBusControllerType::IntelPCH)
    {
        OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1) | 1);
        OutPortByte(base + IntelSMBus::HOST_COMMAND, reg);
        OutPortByte(base + IntelSMBus::HOST_CONTROL,
                    IntelSMBus::CMD_BLOCK | IntelSMBus::CTL_START);

        if (!WaitForComplete()) return -1;

        uint8_t length = InPortByte(base + IntelSMBus::HOST_DATA0);
        if (length > max_length) length = max_length;

        for (uint8_t i = 0; i < length; i++)
        {
            buffer[i] = InPortByte(base + IntelSMBus::HOST_BLOCK_DB);
        }

        return length;
    }
    else if (m_controller_info.type == SMBusControllerType::AMDFCH)
    {
        OutPortByte(base + AMDSMBus::ADDRESS, (addr << 1) | 1);
        OutPortByte(base + AMDSMBus::HOST_CMD, reg);
        OutPortByte(base + AMDSMBus::CONTROL, AMDSMBus::PROTO_BLOCK_DATA);

        if (!WaitForComplete()) return -1;

        uint8_t length = InPortByte(base + AMDSMBus::DATA0);
        if (length > max_length) length = max_length;

        for (uint8_t i = 0; i < length; i++)
        {
            buffer[i] = InPortByte(base + AMDSMBus::BLOCK_DATA);
        }

        return length;
    }

    return -1;
}

/*---------------------------------------------------------*\
| InpOutSMBus Implementation (Stub)                         |
| Similar to WinRing0 but using inpoutx64 library           |
\*---------------------------------------------------------*/
InpOutSMBus::InpOutSMBus()
{
}

InpOutSMBus::~InpOutSMBus()
{
    Shutdown();
}

bool InpOutSMBus::Initialize()
{
    if (m_initialized)
    {
        return true;
    }

    if (!LoadInpOut())
    {
        std::cerr << "[InpOut] Failed to load inpoutx64 library\n";
        return false;
    }

    if (!DetectSMBusController())
    {
        std::cerr << "[InpOut] Failed to detect SMBus controller\n";
        UnloadInpOut();
        return false;
    }

    m_initialized = true;
    return true;
}

void InpOutSMBus::Shutdown()
{
    if (m_initialized)
    {
        UnloadInpOut();
        m_initialized = false;
    }
}

bool InpOutSMBus::LoadInpOut()
{
    m_dll_handle = LoadLibraryA("inpoutx64.dll");
    if (!m_dll_handle)
    {
        m_dll_handle = LoadLibraryA("inpout32.dll");
    }

    if (!m_dll_handle)
    {
        return false;
    }

    m_Out32 = (Out32Type)GetProcAddress(m_dll_handle, "Out32");
    m_Inp32 = (Inp32Type)GetProcAddress(m_dll_handle, "Inp32");
    m_IsDriverOpen = (IsInpOutDriverOpenType)GetProcAddress(m_dll_handle, "IsInpOutDriverOpen");

    if (!m_IsDriverOpen)
    {
        m_IsDriverOpen = (IsInpOutDriverOpenType)GetProcAddress(m_dll_handle, "IsDriverOpen64");
    }

    if (!m_Out32 || !m_Inp32)
    {
        FreeLibrary(m_dll_handle);
        m_dll_handle = nullptr;
        return false;
    }

    if (m_IsDriverOpen && !m_IsDriverOpen())
    {
        FreeLibrary(m_dll_handle);
        m_dll_handle = nullptr;
        return false;
    }

    return true;
}

void InpOutSMBus::UnloadInpOut()
{
    if (m_dll_handle)
    {
        FreeLibrary(m_dll_handle);
        m_dll_handle = nullptr;
    }

    m_Out32 = nullptr;
    m_Inp32 = nullptr;
    m_IsDriverOpen = nullptr;
}

bool InpOutSMBus::DetectSMBusController()
{
    // inpoutx64 doesn't have PCI config access, so we try known base addresses
    const uint16_t common_bases[] = { 0xF000, 0xF020, 0x0B00, 0x0B20 };

    for (uint16_t base : common_bases)
    {
        uint8_t status = InPortByte(base);
        if (status != 0xFF)
        {
            m_controller_info.type = SMBusControllerType::Unknown;
            m_controller_info.base_address = base;
            m_controller_info.description = "SMBus Controller";
            return true;
        }
    }

    return false;
}

uint8_t InpOutSMBus::InPortByte(uint16_t port)
{
    if (m_Inp32)
    {
        return static_cast<uint8_t>(m_Inp32(static_cast<short>(port)));
    }
    return 0xFF;
}

void InpOutSMBus::OutPortByte(uint16_t port, uint8_t value)
{
    if (m_Out32)
    {
        m_Out32(static_cast<short>(port), static_cast<short>(value));
    }
}

// Remaining methods are similar to WinRing0 implementation
// Using the same register offsets but different I/O functions

void InpOutSMBus::ClearStatus()
{
    uint16_t base = m_controller_info.base_address;
    OutPortByte(base + IntelSMBus::HOST_STATUS, 0xFF);
}

uint8_t InpOutSMBus::GetStatus()
{
    return InPortByte(m_controller_info.base_address + IntelSMBus::HOST_STATUS);
}

bool InpOutSMBus::WaitForNotBusy(int timeout_us)
{
    int elapsed = 0;
    while (elapsed < timeout_us)
    {
        if (!(GetStatus() & IntelSMBus::STS_HOST_BUSY))
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        elapsed += 10;
    }
    return false;
}

bool InpOutSMBus::WaitForComplete(int timeout_us)
{
    int elapsed = 0;
    while (elapsed < timeout_us)
    {
        uint8_t status = GetStatus();
        if (status & (IntelSMBus::STS_DEV_ERR | IntelSMBus::STS_BUS_ERR |
                      IntelSMBus::STS_FAILED))
        {
            ClearStatus();
            return false;
        }
        if (status & IntelSMBus::STS_INTR)
        {
            ClearStatus();
            return true;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        elapsed += 10;
    }
    return false;
}

bool InpOutSMBus::QuickCommand(uint8_t addr, bool read)
{
    if (!m_initialized) return false;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return false;
    ClearStatus();

    OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1) | (read ? 1 : 0));
    OutPortByte(base + IntelSMBus::HOST_CONTROL,
                IntelSMBus::CMD_QUICK | IntelSMBus::CTL_START);

    return WaitForComplete();
}

bool InpOutSMBus::SendByte(uint8_t addr, uint8_t data)
{
    if (!m_initialized) return false;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return false;
    ClearStatus();

    OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1));
    OutPortByte(base + IntelSMBus::HOST_COMMAND, data);
    OutPortByte(base + IntelSMBus::HOST_CONTROL,
                IntelSMBus::CMD_BYTE | IntelSMBus::CTL_START);

    return WaitForComplete();
}

uint8_t InpOutSMBus::ReceiveByte(uint8_t addr)
{
    if (!m_initialized) return 0xFF;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return 0xFF;
    ClearStatus();

    OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1) | 1);
    OutPortByte(base + IntelSMBus::HOST_CONTROL,
                IntelSMBus::CMD_BYTE | IntelSMBus::CTL_START);

    if (!WaitForComplete()) return 0xFF;

    return InPortByte(base + IntelSMBus::HOST_DATA0);
}

bool InpOutSMBus::WriteByte(uint8_t addr, uint8_t reg, uint8_t data)
{
    if (!m_initialized) return false;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return false;
    ClearStatus();

    OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1));
    OutPortByte(base + IntelSMBus::HOST_COMMAND, reg);
    OutPortByte(base + IntelSMBus::HOST_DATA0, data);
    OutPortByte(base + IntelSMBus::HOST_CONTROL,
                IntelSMBus::CMD_BYTE_DATA | IntelSMBus::CTL_START);

    return WaitForComplete();
}

uint8_t InpOutSMBus::ReadByte(uint8_t addr, uint8_t reg)
{
    if (!m_initialized) return 0xFF;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return 0xFF;
    ClearStatus();

    OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1) | 1);
    OutPortByte(base + IntelSMBus::HOST_COMMAND, reg);
    OutPortByte(base + IntelSMBus::HOST_CONTROL,
                IntelSMBus::CMD_BYTE_DATA | IntelSMBus::CTL_START);

    if (!WaitForComplete()) return 0xFF;

    return InPortByte(base + IntelSMBus::HOST_DATA0);
}

bool InpOutSMBus::WriteWord(uint8_t addr, uint8_t reg, uint16_t data)
{
    if (!m_initialized) return false;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return false;
    ClearStatus();

    OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1));
    OutPortByte(base + IntelSMBus::HOST_COMMAND, reg);
    OutPortByte(base + IntelSMBus::HOST_DATA0, data & 0xFF);
    OutPortByte(base + IntelSMBus::HOST_DATA1, (data >> 8) & 0xFF);
    OutPortByte(base + IntelSMBus::HOST_CONTROL,
                IntelSMBus::CMD_WORD_DATA | IntelSMBus::CTL_START);

    return WaitForComplete();
}

uint16_t InpOutSMBus::ReadWord(uint8_t addr, uint8_t reg)
{
    if (!m_initialized) return 0xFFFF;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return 0xFFFF;
    ClearStatus();

    OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1) | 1);
    OutPortByte(base + IntelSMBus::HOST_COMMAND, reg);
    OutPortByte(base + IntelSMBus::HOST_CONTROL,
                IntelSMBus::CMD_WORD_DATA | IntelSMBus::CTL_START);

    if (!WaitForComplete()) return 0xFFFF;

    uint8_t low = InPortByte(base + IntelSMBus::HOST_DATA0);
    uint8_t high = InPortByte(base + IntelSMBus::HOST_DATA1);
    return (high << 8) | low;
}

bool InpOutSMBus::WriteBlock(uint8_t addr, uint8_t reg,
                              const uint8_t* data, uint8_t length)
{
    // Block write implementation - similar to WinRing0
    if (!m_initialized || length == 0 || length > 32) return false;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return false;
    ClearStatus();

    OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1));
    OutPortByte(base + IntelSMBus::HOST_COMMAND, reg);
    OutPortByte(base + IntelSMBus::HOST_DATA0, length);

    for (uint8_t i = 0; i < length; i++)
    {
        OutPortByte(base + IntelSMBus::HOST_BLOCK_DB, data[i]);
    }

    OutPortByte(base + IntelSMBus::HOST_CONTROL,
                IntelSMBus::CMD_BLOCK | IntelSMBus::CTL_START);

    return WaitForComplete();
}

int InpOutSMBus::ReadBlock(uint8_t addr, uint8_t reg,
                            uint8_t* buffer, uint8_t max_length)
{
    if (!m_initialized || max_length == 0) return -1;

    uint16_t base = m_controller_info.base_address;

    if (!WaitForNotBusy()) return -1;
    ClearStatus();

    OutPortByte(base + IntelSMBus::TRANSMIT_ADDR, (addr << 1) | 1);
    OutPortByte(base + IntelSMBus::HOST_COMMAND, reg);
    OutPortByte(base + IntelSMBus::HOST_CONTROL,
                IntelSMBus::CMD_BLOCK | IntelSMBus::CTL_START);

    if (!WaitForComplete()) return -1;

    uint8_t length = InPortByte(base + IntelSMBus::HOST_DATA0);
    if (length > max_length) length = max_length;

    for (uint8_t i = 0; i < length; i++)
    {
        buffer[i] = InPortByte(base + IntelSMBus::HOST_BLOCK_DB);
    }

    return length;
}

} // namespace OneClickRGB
