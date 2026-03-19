/**
 * RAM RGB Test using PawnIO
 * Scans for ENE-based DRAM controllers (G.Skill Trident Z, etc.)
 * Based on OpenRGB's PawnIO implementation
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <windows.h>

// PawnIO API (from PawnIOLib.h)
extern "C" {
    __declspec(dllimport) HRESULT __stdcall pawnio_version(PULONG version);
    __declspec(dllimport) HRESULT __stdcall pawnio_open(PHANDLE handle);
    __declspec(dllimport) HRESULT __stdcall pawnio_load(HANDLE handle, const UCHAR* blob, SIZE_T size);
    __declspec(dllimport) HRESULT __stdcall pawnio_execute(HANDLE handle, PCSTR name, const ULONG64* in, SIZE_T in_size, PULONG64 out, SIZE_T out_size, PSIZE_T return_size);
    __declspec(dllimport) HRESULT __stdcall pawnio_close(HANDLE handle);
}

// ENE RAM addresses
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

HANDLE pawnio_handle = nullptr;

// SMBus transaction sizes
#define I2C_SMBUS_QUICK     0
#define I2C_SMBUS_BYTE      1
#define I2C_SMBUS_BYTE_DATA 2
#define I2C_SMBUS_WORD_DATA 3
#define I2C_SMBUS_BLOCK_DATA 5

#define I2C_SMBUS_READ  1
#define I2C_SMBUS_WRITE 0

// i2c_smbus_data union
union i2c_smbus_data {
    uint8_t  byte;
    uint16_t word;
    uint8_t  block[34];
};

// SMBus transfer via PawnIO
int smbus_xfer(uint8_t addr, char read_write, uint8_t command, int size, i2c_smbus_data* data) {
    const SIZE_T in_size = 9;
    ULONG64 in[in_size];
    const SIZE_T out_size = 5;
    ULONG64 out[out_size];
    SIZE_T return_size;

    in[0] = addr;
    in[1] = read_write;
    in[2] = command;
    in[3] = size;

    if (data != nullptr) {
        memcpy(&in[4], data, sizeof(i2c_smbus_data));
    }

    HRESULT status = pawnio_execute(pawnio_handle, "ioctl_smbus_xfer", in, in_size, out, out_size, &return_size);

    if (data != nullptr) {
        memcpy(data, &out[0], sizeof(i2c_smbus_data));
    }

    return status ? -1 : 0;
}

// High-level SMBus operations
int smbus_read_byte(uint8_t addr) {
    i2c_smbus_data data;
    if (smbus_xfer(addr, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data) < 0)
        return -1;
    return data.byte;
}

int smbus_read_byte_data(uint8_t addr, uint8_t cmd) {
    i2c_smbus_data data;
    if (smbus_xfer(addr, I2C_SMBUS_READ, cmd, I2C_SMBUS_BYTE_DATA, &data) < 0)
        return -1;
    return data.byte;
}

int smbus_write_byte_data(uint8_t addr, uint8_t cmd, uint8_t value) {
    i2c_smbus_data data;
    data.byte = value;
    return smbus_xfer(addr, I2C_SMBUS_WRITE, cmd, I2C_SMBUS_BYTE_DATA, &data);
}

int smbus_write_word_data(uint8_t addr, uint8_t cmd, uint16_t value) {
    i2c_smbus_data data;
    data.word = value;
    return smbus_xfer(addr, I2C_SMBUS_WRITE, cmd, I2C_SMBUS_WORD_DATA, &data);
}

// ENE register operations
uint8_t ene_read(uint8_t addr, uint16_t reg) {
    // Write register address (byte-swapped)
    smbus_write_word_data(addr, 0x00, ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF));
    // Read value
    return smbus_read_byte_data(addr, 0x81);
}

void ene_write(uint8_t addr, uint16_t reg, uint8_t value) {
    // Write register address (byte-swapped)
    smbus_write_word_data(addr, 0x00, ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF));
    // Write value
    smbus_write_byte_data(addr, 0x01, value);
}

// Load PawnIO SMBus module
bool LoadPawnIOModule(const std::string& filename) {
    ULONG version;
    if (pawnio_version(&version) != S_OK) {
        std::cerr << "PawnIO DLL not available" << std::endl;
        return false;
    }
    std::cout << "PawnIO version: " << version << std::endl;

    // Open PawnIO
    if (pawnio_open(&pawnio_handle) != S_OK) {
        std::cerr << "Failed to open PawnIO (need Administrator?)" << std::endl;
        return false;
    }

    // Load module file
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << filename << std::endl;
        pawnio_close(pawnio_handle);
        return false;
    }

    std::vector<char> blob((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    HRESULT status = pawnio_load(pawnio_handle, reinterpret_cast<const UCHAR*>(blob.data()), blob.size());
    if (status != S_OK) {
        std::cerr << "Failed to load PawnIO module: 0x" << std::hex << status << std::dec << std::endl;
        pawnio_close(pawnio_handle);
        return false;
    }

    std::cout << "PawnIO module loaded: " << filename << std::endl;
    return true;
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

            // Check for "Micron" - these are incompatible
            if (strstr(name, "Micron") != nullptr) {
                std::cout << "  (Skipping - Micron/Crucial uses different protocol)" << std::endl;
                continue;
            }

            // Check for DIMM_LED or AUDA (G.Skill Trident Z)
            if (strstr(name, "DIMM_LED") != nullptr || strstr(name, "AUDA") != nullptr) {
                std::cout << "  -> G.Skill Trident Z compatible!" << std::endl;
            }
        }
    }
}

// Set RAM color
void SetRAMColor(uint8_t addr, uint8_t r, uint8_t g, uint8_t b, int led_count = 5) {
    std::cout << "Setting RAM at 0x" << std::hex << (int)addr << std::dec
              << " to RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

    // Enable direct mode
    ene_write(addr, ENE_REG_DIRECT, 0x01);

    // Set colors for each LED (RBG order for ENE!)
    for (int i = 0; i < led_count; i++) {
        uint16_t color_reg = ENE_REG_COLORS_DIRECT + (i * 3);
        ene_write(addr, color_reg + 0, r);  // Red
        ene_write(addr, color_reg + 1, b);  // Blue (swapped!)
        ene_write(addr, color_reg + 2, g);  // Green
    }

    // Apply changes
    ene_write(addr, ENE_REG_APPLY, ENE_APPLY_VAL);
    std::cout << "Applied!" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== RAM RGB PawnIO Test ===" << std::endl;
    std::cout << "Requires Administrator privileges!" << std::endl << std::endl;

    // Try to load Intel i801 SMBus module
    if (!LoadPawnIOModule("dependencies/PawnIO/modules/SmbusI801.bin")) {
        // Try without path
        if (!LoadPawnIOModule("SmbusI801.bin")) {
            std::cerr << "Failed to load SMBus module" << std::endl;
            std::cerr << "Make sure SmbusI801.bin is available" << std::endl;
            return 1;
        }
    }

    // Scan for DRAM controllers
    ScanForDRAM();

    // If address and color provided, set it
    if (argc >= 5) {
        uint8_t addr = (uint8_t)strtol(argv[1], nullptr, 16);
        uint8_t r = (uint8_t)atoi(argv[2]);
        uint8_t g = (uint8_t)atoi(argv[3]);
        uint8_t b = (uint8_t)atoi(argv[4]);
        int led_count = (argc >= 6) ? atoi(argv[5]) : 5;

        SetRAMColor(addr, r, g, b, led_count);
    }

    pawnio_close(pawnio_handle);
    std::cout << "\n=== Test complete ===" << std::endl;
    return 0;
}
