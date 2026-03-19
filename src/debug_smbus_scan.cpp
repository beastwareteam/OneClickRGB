/*---------------------------------------------------------*\
| debug_smbus_scan.cpp                                      |
|                                                           |
| Test program for SMBus/I2C scanning and DDR5 RGB control |
| Run as Administrator for hardware access                  |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

#include "smbus/SMBusInterface.h"
#include "smbus/SMBusWindows.h"
#include "controllers/GSkillTridentZ5Controller.h"

using namespace OneClickRGB;

/*---------------------------------------------------------*\
| Print Hex Dump                                            |
\*---------------------------------------------------------*/
void HexDump(const char* label, uint8_t addr, uint8_t start_reg,
             const uint8_t* data, size_t len)
{
    std::cout << label << " (0x" << std::hex << (int)addr
              << ", regs 0x" << (int)start_reg << "-0x"
              << (int)(start_reg + len - 1) << std::dec << "):\n";

    for (size_t i = 0; i < len; i++)
    {
        if (i % 16 == 0)
        {
            std::cout << "  " << std::hex << std::setw(2)
                      << std::setfill('0') << (int)(start_reg + i) << ": ";
        }

        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << (int)data[i] << " ";

        if ((i + 1) % 16 == 0)
        {
            std::cout << "\n";
        }
    }

    if (len % 16 != 0)
    {
        std::cout << "\n";
    }
    std::cout << std::dec;
}

/*---------------------------------------------------------*\
| Scan SMBus for All Devices                                |
\*---------------------------------------------------------*/
void ScanSMBus(SMBusInterface* smbus)
{
    std::cout << "\n=== SMBus Device Scan ===\n\n";
    std::cout << "     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n";

    for (uint8_t row = 0; row < 8; row++)
    {
        std::cout << std::hex << row << "0: ";

        for (uint8_t col = 0; col < 16; col++)
        {
            uint8_t addr = (row << 4) | col;

            // Skip reserved addresses
            if (addr < 0x08 || addr > 0x77)
            {
                std::cout << "   ";
                continue;
            }

            if (smbus->ProbeAddress(addr))
            {
                std::cout << std::hex << std::setw(2)
                          << std::setfill('0') << (int)addr << " ";
            }
            else
            {
                std::cout << "-- ";
            }
        }

        std::cout << "\n";
    }

    std::cout << std::dec << "\n";
}

/*---------------------------------------------------------*\
| Scan DDR5 SPD5 Hubs                                       |
\*---------------------------------------------------------*/
void ScanDDR5(SMBusInterface* smbus)
{
    std::cout << "\n=== DDR5 SPD5 Hub Scan (0x50-0x57) ===\n\n";

    for (uint8_t addr = 0x50; addr <= 0x57; addr++)
    {
        std::cout << "DIMM" << (addr - 0x50) << " (0x"
                  << std::hex << (int)addr << std::dec << "): ";

        if (!smbus->ProbeAddress(addr))
        {
            std::cout << "Not present\n";
            continue;
        }

        std::cout << "Present";

        // Try to read SPD data (first 16 bytes)
        uint8_t spd_data[16];
        bool spd_readable = true;

        // Select SPD page
        smbus->WriteByte(addr, SPD5Hub::PAGE_SELECT_REG, SPD5Hub::PAGE_SPD);

        for (int i = 0; i < 16; i++)
        {
            spd_data[i] = smbus->ReadByte(addr, i);
            if (spd_data[i] == 0xFF)
            {
                spd_readable = false;
            }
        }

        if (spd_readable)
        {
            // DDR5 SPD byte 0x02 contains the key byte (should be 0x12 for DDR5)
            if (spd_data[2] == 0x12)
            {
                std::cout << " - DDR5 SDRAM detected";
            }
        }

        // Check for RGB controller
        smbus->WriteByte(addr, SPD5Hub::PAGE_SELECT_REG, SPD5Hub::PAGE_VENDOR_1);
        uint8_t rgb_sig = smbus->ReadByte(addr, 0x00);

        if (rgb_sig != 0xFF)
        {
            std::cout << " - RGB controller (sig: 0x"
                      << std::hex << (int)rgb_sig << std::dec << ")";
        }

        std::cout << "\n";

        // Return to SPD page
        smbus->WriteByte(addr, SPD5Hub::PAGE_SELECT_REG, SPD5Hub::PAGE_SPD);
    }

    std::cout << "\n";
}

/*---------------------------------------------------------*\
| Dump RGB Controller Registers                             |
\*---------------------------------------------------------*/
void DumpRGBRegisters(SMBusInterface* smbus, uint8_t addr)
{
    std::cout << "\n=== RGB Controller Registers (0x"
              << std::hex << (int)addr << std::dec << ") ===\n\n";

    // Select RGB page
    smbus->WriteByte(addr, SPD5Hub::PAGE_SELECT_REG, SPD5Hub::PAGE_VENDOR_1);

    // Read first 128 bytes
    uint8_t regs[128];
    for (int i = 0; i < 128; i++)
    {
        regs[i] = smbus->ReadByte(addr, i);
    }

    HexDump("RGB Registers", addr, 0x00, regs, 128);

    // Decode known registers
    std::cout << "\nDecoded Values:\n";
    std::cout << "  Mode:       0x" << std::hex << (int)regs[GSkillRegs::MODE] << std::dec << "\n";
    std::cout << "  Speed:      " << (int)regs[GSkillRegs::SPEED] << "\n";
    std::cout << "  Direction:  " << (int)regs[GSkillRegs::DIRECTION] << "\n";
    std::cout << "  Brightness: " << (int)regs[GSkillRegs::BRIGHTNESS] << "\n";
    std::cout << "  LED Count:  " << (int)regs[GSkillRegs::LED_COUNT] << "\n";
    std::cout << "  FW Version: " << (int)(regs[GSkillRegs::FIRMWARE_VERSION] >> 4)
              << "." << (int)(regs[GSkillRegs::FIRMWARE_VERSION] & 0x0F) << "\n";

    // Show color values
    std::cout << "\n  LED Colors:\n";
    for (int i = 0; i < 10; i++)
    {
        int base = GSkillRegs::COLOR_START + (i * 3);
        std::cout << "    LED" << i << ": RGB("
                  << (int)regs[base] << ", "
                  << (int)regs[base + 1] << ", "
                  << (int)regs[base + 2] << ")\n";
    }

    // Return to SPD page
    smbus->WriteByte(addr, SPD5Hub::PAGE_SELECT_REG, SPD5Hub::PAGE_SPD);
}

/*---------------------------------------------------------*\
| Test RGB Control                                          |
\*---------------------------------------------------------*/
void TestRGBControl(SMBusInterface* smbus, uint8_t addr)
{
    std::cout << "\n=== Testing RGB Control ===\n\n";

    auto controller = std::make_shared<SMBusInterface*>(smbus);

    // Create controller instance
    auto smbus_ptr = std::shared_ptr<SMBusInterface>(smbus, [](SMBusInterface*){});
    GSkillTridentZ5Controller rgb(smbus_ptr, addr);

    if (!rgb.Initialize())
    {
        std::cerr << "Failed to initialize RGB controller\n";
        return;
    }

    std::cout << "Controller: " << rgb.GetName() << "\n";
    std::cout << "Location:   " << rgb.GetLocation() << "\n";
    std::cout << "Version:    " << rgb.GetVersion() << "\n";
    std::cout << "LED Count:  " << rgb.GetLEDCount() << "\n";

    // Set a test color
    std::cout << "\nSetting all LEDs to RED...\n";
    rgb.SetAllLEDs(RGBColor{255, 0, 0});
    rgb.SetMode(1);  // Static mode
    rgb.SetBrightness(100);

    if (rgb.Apply())
    {
        std::cout << "SUCCESS: Color applied!\n";
    }
    else
    {
        std::cout << "FAILED to apply color\n";
    }

    // Wait for user input
    std::cout << "\nPress Enter to test next color (GREEN)...";
    std::cin.get();

    rgb.SetAllLEDs(RGBColor{0, 255, 0});
    rgb.Apply();
    std::cout << "GREEN applied\n";

    std::cout << "\nPress Enter to test next color (BLUE)...";
    std::cin.get();

    rgb.SetAllLEDs(RGBColor{0, 0, 255});
    rgb.Apply();
    std::cout << "BLUE applied\n";

    std::cout << "\nPress Enter to test rainbow effect...";
    std::cin.get();

    // Set rainbow colors on each LED
    for (int i = 0; i < rgb.GetLEDCount(); i++)
    {
        float hue = (float)i / rgb.GetLEDCount() * 360.0f;

        // HSV to RGB (simplified)
        int h = (int)(hue / 60) % 6;
        float f = hue / 60.0f - h;
        uint8_t v = 255;
        uint8_t p = 0;
        uint8_t q = (uint8_t)(255 * (1 - f));
        uint8_t t = (uint8_t)(255 * f);

        RGBColor color;
        switch (h)
        {
            case 0: color = RGBColor{v, t, p}; break;
            case 1: color = RGBColor{q, v, p}; break;
            case 2: color = RGBColor{p, v, t}; break;
            case 3: color = RGBColor{p, q, v}; break;
            case 4: color = RGBColor{t, p, v}; break;
            case 5: color = RGBColor{v, p, q}; break;
        }

        rgb.SetLEDColor(i, color);
    }

    rgb.Apply();
    std::cout << "Rainbow applied\n";

    std::cout << "\nPress Enter to restore default (breathing mode)...";
    std::cin.get();

    rgb.SetMode(2);  // Breathing
    rgb.SetAllLEDs(RGBColor{0, 128, 255});  // Cyan-ish
    rgb.Apply();

    std::cout << "Done!\n";
}

/*---------------------------------------------------------*\
| Main                                                      |
\*---------------------------------------------------------*/
int main(int argc, char* argv[])
{
    std::cout << "======================================\n";
    std::cout << " OneClickRGB - SMBus Scanner/Tester\n";
    std::cout << " DDR5 RGB Control via SMBus/I2C\n";
    std::cout << "======================================\n\n";

    std::cout << "NOTE: This program requires Administrator privileges\n";
    std::cout << "      for direct hardware access.\n\n";

    // Try to create SMBus interface
    std::cout << "Initializing SMBus interface...\n";

    auto smbus = SMBusInterface::Create();

    if (!smbus)
    {
        std::cerr << "\nERROR: Could not initialize SMBus interface.\n";
        std::cerr << "Possible causes:\n";
        std::cerr << "  - WinRing0x64.dll not found\n";
        std::cerr << "  - WinRing0x64.sys driver not installed\n";
        std::cerr << "  - Not running as Administrator\n";
        std::cerr << "  - SMBus controller not detected\n";
        return 1;
    }

    auto info = smbus->GetControllerInfo();
    std::cout << "SMBus Controller: " << info.description << "\n";
    std::cout << "Base Address:     0x" << std::hex << info.base_address << std::dec << "\n";
    std::cout << "PCI Device:       0x" << std::hex << info.pci_device << std::dec << "\n\n";

    // Parse command line
    std::string command = "scan";
    uint8_t target_addr = 0;

    if (argc > 1)
    {
        command = argv[1];
    }
    if (argc > 2)
    {
        target_addr = static_cast<uint8_t>(std::stoul(argv[2], nullptr, 16));
    }

    if (command == "scan")
    {
        // Full SMBus scan
        ScanSMBus(smbus.get());

        // DDR5 specific scan
        ScanDDR5(smbus.get());

        // Detect G.Skill modules
        auto modules = GSkillTridentZ5Controller::DetectModules(smbus.get());

        if (!modules.empty())
        {
            std::cout << "\nFound " << modules.size() << " G.Skill RGB module(s)\n";
            for (auto addr : modules)
            {
                DumpRGBRegisters(smbus.get(), addr);
            }
        }
    }
    else if (command == "dump" && target_addr != 0)
    {
        // Dump specific address
        DumpRGBRegisters(smbus.get(), target_addr);
    }
    else if (command == "test" && target_addr != 0)
    {
        // Test RGB control
        TestRGBControl(smbus.get(), target_addr);
    }
    else
    {
        std::cout << "Usage:\n";
        std::cout << "  " << argv[0] << " scan              - Scan SMBus for all devices\n";
        std::cout << "  " << argv[0] << " dump <addr>       - Dump registers at address (hex)\n";
        std::cout << "  " << argv[0] << " test <addr>       - Test RGB control at address (hex)\n";
        std::cout << "\nExamples:\n";
        std::cout << "  " << argv[0] << " dump 0x50         - Dump DIMM0\n";
        std::cout << "  " << argv[0] << " test 0x51         - Test DIMM1 RGB\n";
    }

    smbus->Shutdown();
    return 0;
}
