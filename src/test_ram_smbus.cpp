/**
 * RAM RGB SMBus Test
 * Scans for ENE-based DRAM controllers (G.Skill Trident Z, etc.)
 * Uses PawnIO for SMBus access
 */

#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <windows.h>

// PawnIO library
#include "PawnIOLib.h"

// ENE RAM addresses to scan
static const uint8_t ene_ram_addresses[] = {
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
    0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x4F, 0x66, 0x67,
    0x39, 0x3A, 0x3B, 0x3C, 0x3D
};

// ENE Registers
constexpr uint16_t ENE_REG_DEVICE_NAME    = 0x1000;
constexpr uint16_t ENE_REG_COLORS_DIRECT  = 0x8000;
constexpr uint16_t ENE_REG_DIRECT         = 0x8020;
constexpr uint16_t ENE_REG_MODE           = 0x8021;
constexpr uint16_t ENE_REG_APPLY          = 0x80A0;
constexpr uint8_t  ENE_APPLY_VAL          = 0x01;

// SMBus Base (Intel PCH - will be detected)
uint16_t smbus_base = 0;

// PawnIO handle
HANDLE pawnio_handle = nullptr;

// Load PawnIO
typedef HRESULT (*PawnIO_Start_t)(PHANDLE, LPCWSTR);
typedef HRESULT (*PawnIO_ReadIOPort8_t)(HANDLE, UINT16, UINT8*);
typedef HRESULT (*PawnIO_WriteIOPort8_t)(HANDLE, UINT16, UINT8);
typedef HRESULT (*PawnIO_ReadPciConfig_t)(HANDLE, UINT16, UINT16, UINT16, UINT8, UINT32*);

PawnIO_Start_t PawnIO_Start = nullptr;
PawnIO_ReadIOPort8_t PawnIO_ReadIOPort8 = nullptr;
PawnIO_WriteIOPort8_t PawnIO_WriteIOPort8 = nullptr;
PawnIO_ReadPciConfig_t PawnIO_ReadPciConfig = nullptr;

bool LoadPawnIO() {
    HMODULE hLib = LoadLibraryA("PawnIOLib.dll");
    if (!hLib) {
        std::cerr << "Failed to load PawnIOLib.dll" << std::endl;
        return false;
    }

    PawnIO_Start = (PawnIO_Start_t)GetProcAddress(hLib, "PawnIO_Start");
    PawnIO_ReadIOPort8 = (PawnIO_ReadIOPort8_t)GetProcAddress(hLib, "PawnIO_ReadIOPort8");
    PawnIO_WriteIOPort8 = (PawnIO_WriteIOPort8_t)GetProcAddress(hLib, "PawnIO_WriteIOPort8");
    PawnIO_ReadPciConfig = (PawnIO_ReadPciConfig_t)GetProcAddress(hLib, "PawnIO_ReadPciConfig");

    if (!PawnIO_Start || !PawnIO_ReadIOPort8 || !PawnIO_WriteIOPort8) {
        std::cerr << "Failed to get PawnIO functions" << std::endl;
        return false;
    }

    // Start PawnIO with the i801_smbus module
    HRESULT hr = PawnIO_Start(&pawnio_handle, L"modules\\i801_smbus.piom");
    if (FAILED(hr)) {
        std::cerr << "Failed to start PawnIO: 0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }

    std::cout << "PawnIO initialized successfully" << std::endl;
    return true;
}

// Detect Intel SMBus base address
bool DetectSMBusBase() {
    if (!PawnIO_ReadPciConfig) {
        std::cerr << "PawnIO_ReadPciConfig not available" << std::endl;
        return false;
    }

    // Intel SMBus: Bus 0, Device 31, Function 4, Offset 0x20 (BAR)
    uint32_t bar = 0;
    HRESULT hr = PawnIO_ReadPciConfig(pawnio_handle, 0, 31, 4, 0x20, &bar);
    if (FAILED(hr)) {
        std::cerr << "Failed to read SMBus BAR: 0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }

    smbus_base = (uint16_t)(bar & 0xFFFE);
    std::cout << "SMBus base address: 0x" << std::hex << smbus_base << std::dec << std::endl;
    return smbus_base != 0;
}

// SMBus low-level operations (Intel i801)
#define SMBUS_STS       0x00
#define SMBUS_CTL       0x02
#define SMBUS_CMD       0x03
#define SMBUS_ADDR      0x04
#define SMBUS_DATA0     0x05
#define SMBUS_DATA1     0x06
#define SMBUS_BLOCK     0x07

#define SMBUS_HOST_BUSY 0x01
#define SMBUS_INTR      0x02
#define SMBUS_DEV_ERR   0x04
#define SMBUS_BUS_ERR   0x08

void smbus_wait() {
    uint8_t status;
    int timeout = 1000;
    do {
        PawnIO_ReadIOPort8(pawnio_handle, smbus_base + SMBUS_STS, &status);
        if (!(status & SMBUS_HOST_BUSY)) break;
        Sleep(1);
    } while (--timeout > 0);
}

void smbus_clear_status() {
    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_STS, 0xFF);
}

int smbus_read_byte(uint8_t addr) {
    smbus_wait();
    smbus_clear_status();

    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_ADDR, (addr << 1) | 0x01);
    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_CTL, 0x44);  // Byte read + Start

    smbus_wait();

    uint8_t status;
    PawnIO_ReadIOPort8(pawnio_handle, smbus_base + SMBUS_STS, &status);
    if (status & (SMBUS_DEV_ERR | SMBUS_BUS_ERR)) {
        return -1;
    }

    uint8_t data;
    PawnIO_ReadIOPort8(pawnio_handle, smbus_base + SMBUS_DATA0, &data);
    return data;
}

int smbus_write_byte_data(uint8_t addr, uint8_t cmd, uint8_t value) {
    smbus_wait();
    smbus_clear_status();

    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_ADDR, addr << 1);
    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_CMD, cmd);
    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_DATA0, value);
    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_CTL, 0x48);  // Byte data + Start

    smbus_wait();

    uint8_t status;
    PawnIO_ReadIOPort8(pawnio_handle, smbus_base + SMBUS_STS, &status);
    return (status & (SMBUS_DEV_ERR | SMBUS_BUS_ERR)) ? -1 : 0;
}

int smbus_write_word_data(uint8_t addr, uint8_t cmd, uint16_t value) {
    smbus_wait();
    smbus_clear_status();

    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_ADDR, addr << 1);
    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_CMD, cmd);
    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_DATA0, value & 0xFF);
    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_DATA1, (value >> 8) & 0xFF);
    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_CTL, 0x4C);  // Word data + Start

    smbus_wait();

    uint8_t status;
    PawnIO_ReadIOPort8(pawnio_handle, smbus_base + SMBUS_STS, &status);
    return (status & (SMBUS_DEV_ERR | SMBUS_BUS_ERR)) ? -1 : 0;
}

int smbus_read_byte_data(uint8_t addr, uint8_t cmd) {
    smbus_wait();
    smbus_clear_status();

    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_ADDR, (addr << 1) | 0x01);
    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_CMD, cmd);
    PawnIO_WriteIOPort8(pawnio_handle, smbus_base + SMBUS_CTL, 0x48);  // Byte data + Start

    smbus_wait();

    uint8_t status;
    PawnIO_ReadIOPort8(pawnio_handle, smbus_base + SMBUS_STS, &status);
    if (status & (SMBUS_DEV_ERR | SMBUS_BUS_ERR)) {
        return -1;
    }

    uint8_t data;
    PawnIO_ReadIOPort8(pawnio_handle, smbus_base + SMBUS_DATA0, &data);
    return data;
}

// ENE register operations
uint8_t ene_read(uint8_t addr, uint16_t reg) {
    // Write register (swapped bytes)
    smbus_write_word_data(addr, 0x00, ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF));
    // Read value
    return smbus_read_byte_data(addr, 0x81);
}

void ene_write(uint8_t addr, uint16_t reg, uint8_t value) {
    // Write register (swapped bytes)
    smbus_write_word_data(addr, 0x00, ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF));
    // Write value
    smbus_write_byte_data(addr, 0x01, value);
}

// Scan for ENE DRAM controllers
void ScanForDRAM() {
    std::cout << "\n=== Scanning for ENE DRAM controllers ===" << std::endl;

    for (uint8_t addr : ene_ram_addresses) {
        int res = smbus_read_byte(addr);
        if (res >= 0) {
            std::cout << "\nDevice found at 0x" << std::hex << (int)addr << std::dec << std::endl;

            // Read device name
            char name[17] = {0};
            for (int i = 0; i < 16; i++) {
                name[i] = ene_read(addr, ENE_REG_DEVICE_NAME + i);
            }
            std::cout << "  Device name: " << name << std::endl;
        }
    }
}

// Set RAM color
void SetRAMColor(uint8_t addr, uint8_t r, uint8_t g, uint8_t b, int led_count = 5) {
    std::cout << "Setting RAM at 0x" << std::hex << (int)addr << std::dec
              << " to RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

    // Enable direct mode
    ene_write(addr, ENE_REG_DIRECT, 0x01);

    // Set colors (RBG order!)
    for (int i = 0; i < led_count; i++) {
        uint16_t color_reg = ENE_REG_COLORS_DIRECT + (i * 3);
        ene_write(addr, color_reg + 0, r);
        ene_write(addr, color_reg + 1, b);
        ene_write(addr, color_reg + 2, g);
    }

    // Apply
    ene_write(addr, ENE_REG_APPLY, ENE_APPLY_VAL);
}

int main() {
    std::cout << "=== RAM RGB SMBus Test ===" << std::endl;
    std::cout << "Requires Administrator privileges!" << std::endl << std::endl;

    if (!LoadPawnIO()) {
        std::cerr << "Failed to initialize PawnIO. Make sure:" << std::endl;
        std::cerr << "  1. Run as Administrator" << std::endl;
        std::cerr << "  2. PawnIOLib.dll is in the current directory" << std::endl;
        std::cerr << "  3. modules/i801_smbus.piom exists" << std::endl;
        return 1;
    }

    if (!DetectSMBusBase()) {
        std::cerr << "Failed to detect SMBus base address" << std::endl;
        return 1;
    }

    ScanForDRAM();

    std::cout << "\n=== Test complete ===" << std::endl;
    return 0;
}
