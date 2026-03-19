/**
 * Detailed RAM RGB Debug - Test all addresses and settings
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdint>

typedef HRESULT (__stdcall *pawnio_open_t)(PHANDLE);
typedef HRESULT (__stdcall *pawnio_load_t)(HANDLE, const UCHAR*, SIZE_T);
typedef HRESULT (__stdcall *pawnio_execute_t)(HANDLE, PCSTR, const ULONG64*, SIZE_T, PULONG64, SIZE_T, PSIZE_T);
typedef HRESULT (__stdcall *pawnio_close_t)(HANDLE);

union i2c_smbus_data { uint8_t byte; uint16_t word; uint8_t block[34]; };

HANDLE g_pawnio = nullptr;
pawnio_execute_t g_exec = nullptr;

int smbus_xfer(uint8_t addr, char rw, uint8_t cmd, int sz, i2c_smbus_data* data) {
    ULONG64 in[9] = {addr, (ULONG64)rw, cmd, (ULONG64)sz};
    if (data) memcpy(&in[4], data, sizeof(i2c_smbus_data));
    ULONG64 out[5] = {0}; SIZE_T ret_sz;
    HRESULT hr = g_exec(g_pawnio, "ioctl_smbus_xfer", in, 9, out, 5, &ret_sz);
    if (data) memcpy(data, &out[0], sizeof(i2c_smbus_data));
    return hr == S_OK ? 0 : -1;
}

int read_byte(uint8_t addr) {
    i2c_smbus_data d; return smbus_xfer(addr, 1, 0, 1, &d) < 0 ? -1 : d.byte;
}

void write_word(uint8_t addr, uint8_t cmd, uint16_t val) {
    i2c_smbus_data d; d.word = val; smbus_xfer(addr, 0, cmd, 3, &d);
}

void write_byte(uint8_t addr, uint8_t cmd, uint8_t val) {
    i2c_smbus_data d; d.byte = val; smbus_xfer(addr, 0, cmd, 2, &d);
}

int read_byte_data(uint8_t addr, uint8_t cmd) {
    i2c_smbus_data d; return smbus_xfer(addr, 1, cmd, 2, &d) < 0 ? -1 : d.byte;
}

void ene_write(uint8_t addr, uint16_t reg, uint8_t val) {
    uint16_t sw = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF);
    write_word(addr, 0x00, sw); Sleep(1);
    write_byte(addr, 0x01, val); Sleep(1);
}

uint8_t ene_read(uint8_t addr, uint16_t reg) {
    uint16_t sw = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF);
    write_word(addr, 0x00, sw); Sleep(1);
    int res = read_byte_data(addr, 0x81);
    return res >= 0 ? (uint8_t)res : 0;
}

int main(int argc, char* argv[]) {
    uint8_t r = 0, g = 34, b = 255;
    if (argc >= 4) {
        r = atoi(argv[1]);
        g = atoi(argv[2]);
        b = atoi(argv[3]);
    }

    std::cout << "=== Detailed RAM RGB Debug ===" << std::endl;
    std::cout << "Target: RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;
    std::cout << std::endl;

    // Load PawnIO
    HMODULE dll = LoadLibraryA("PawnIOLib.dll");
    if (!dll) {
        std::cout << "ERROR: PawnIOLib.dll not found" << std::endl;
        return 1;
    }

    auto p_open = (pawnio_open_t)GetProcAddress(dll, "pawnio_open");
    auto p_load = (pawnio_load_t)GetProcAddress(dll, "pawnio_load");
    g_exec = (pawnio_execute_t)GetProcAddress(dll, "pawnio_execute");
    auto p_close = (pawnio_close_t)GetProcAddress(dll, "pawnio_close");

    if (p_open(&g_pawnio) != S_OK) {
        std::cout << "ERROR: PawnIO driver not running" << std::endl;
        FreeLibrary(dll);
        return 1;
    }

    // Load SMBus module
    HANDLE hFile = CreateFileA("SmbusI801.bin", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cout << "ERROR: SmbusI801.bin not found" << std::endl;
        p_close(g_pawnio);
        FreeLibrary(dll);
        return 1;
    }

    DWORD size = GetFileSize(hFile, NULL);
    std::vector<uint8_t> blob(size);
    ReadFile(hFile, blob.data(), size, &size, NULL);
    CloseHandle(hFile);

    if (p_load(g_pawnio, blob.data(), blob.size()) != S_OK) {
        std::cout << "ERROR: Failed to load SMBus module" << std::endl;
        p_close(g_pawnio);
        FreeLibrary(dll);
        return 1;
    }

    std::cout << "PawnIO initialized successfully" << std::endl << std::endl;

    // Scan all potential addresses
    std::cout << "=== Scanning I2C addresses ===" << std::endl;
    uint8_t all_addrs[] = {0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,  // SPD5 hubs
                           0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77}; // ENE controllers

    std::vector<uint8_t> found_ene;

    for (uint8_t addr : all_addrs) {
        int result = read_byte(addr);
        if (result >= 0) {
            std::cout << "Found device at 0x" << std::hex << (int)addr << std::dec;

            // Try to read device name (ENE format)
            char name[17] = {0};
            for (int i = 0; i < 16; i++) {
                name[i] = ene_read(addr, 0x1000 + i);
            }

            if (name[0] >= 32 && name[0] < 127) {
                std::cout << " - Name: \"" << name << "\"";
                found_ene.push_back(addr);
            }
            std::cout << std::endl;
        }
    }

    std::cout << std::endl;

    // For each found ENE device, read detailed info
    for (uint8_t addr : found_ene) {
        std::cout << "=== Device 0x" << std::hex << (int)addr << std::dec << " ===" << std::endl;

        // Read device info
        char name[17] = {0};
        for (int i = 0; i < 16; i++) name[i] = ene_read(addr, 0x1000 + i);
        std::cout << "Name: " << name << std::endl;

        // Read firmware info
        char fw[17] = {0};
        for (int i = 0; i < 16; i++) fw[i] = ene_read(addr, 0x1010 + i);
        std::cout << "Firmware: " << fw << std::endl;

        // Read config registers
        uint8_t led_count = ene_read(addr, 0x1C02);
        std::cout << "LED Count (0x1C02): " << (int)led_count << std::endl;

        uint8_t mode = ene_read(addr, 0x8021);
        std::cout << "Current Mode (0x8021): 0x" << std::hex << (int)mode << std::dec << std::endl;

        uint8_t direct = ene_read(addr, 0x8020);
        std::cout << "Direct Mode (0x8020): 0x" << std::hex << (int)direct << std::dec << std::endl;

        // Read current colors
        std::cout << "Current colors:" << std::endl;
        for (int i = 0; i < 8; i++) {
            uint16_t base = 0x8100 + (i * 3);
            uint8_t cr = ene_read(addr, base + 0);
            uint8_t cb = ene_read(addr, base + 1);  // ENE uses RBG
            uint8_t cg = ene_read(addr, base + 2);
            std::cout << "  LED " << i << ": RGB(" << (int)cr << "," << (int)cg << "," << (int)cb << ")" << std::endl;
        }

        std::cout << std::endl;

        // Now try to SET colors
        std::cout << "Setting colors to RGB(" << (int)r << "," << (int)g << "," << (int)b << ")..." << std::endl;

        // Enable direct mode
        ene_write(addr, 0x8020, 0x01);
        Sleep(10);

        // Verify direct mode is set
        direct = ene_read(addr, 0x8020);
        std::cout << "Direct mode after enable: 0x" << std::hex << (int)direct << std::dec << std::endl;

        // Set colors for all LEDs (use actual led_count or default 8)
        int leds = (led_count > 0 && led_count <= 20) ? led_count : 8;
        for (int i = 0; i < leds; i++) {
            uint16_t base = 0x8100 + (i * 3);
            ene_write(addr, base + 0, r);     // Red
            ene_write(addr, base + 1, b);     // Blue (ENE uses RBG order!)
            ene_write(addr, base + 2, g);     // Green
        }
        Sleep(10);

        // Apply changes
        ene_write(addr, 0x80A0, 0x01);
        Sleep(50);

        // Verify colors were set
        std::cout << "Colors after setting:" << std::endl;
        for (int i = 0; i < leds; i++) {
            uint16_t base = 0x8100 + (i * 3);
            uint8_t cr = ene_read(addr, base + 0);
            uint8_t cb = ene_read(addr, base + 1);
            uint8_t cg = ene_read(addr, base + 2);
            std::cout << "  LED " << i << ": RGB(" << (int)cr << "," << (int)cg << "," << (int)cb << ")" << std::endl;
        }

        std::cout << std::endl;
    }

    // Check if we found no ENE devices but found SPD5 hubs
    if (found_ene.empty()) {
        std::cout << "No ENE controllers found. Checking for DDR5 SPD5 hubs..." << std::endl;

        // DDR5 uses different protocol via SPD5 hub
        for (uint8_t hub = 0x50; hub <= 0x57; hub++) {
            if (read_byte(hub) >= 0) {
                std::cout << "Found SPD5 hub at 0x" << std::hex << (int)hub << std::dec << std::endl;

                // DDR5 RGB controllers are typically at 0x50-0x57 with different registers
                // Try to read PMIC/RGB data
                uint8_t data[16];
                for (int i = 0; i < 16; i++) {
                    data[i] = read_byte_data(hub, i);
                }
                std::cout << "First 16 bytes: ";
                for (int i = 0; i < 16; i++) {
                    std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)data[i] << " ";
                }
                std::cout << std::dec << std::endl;
            }
        }
    }

    p_close(g_pawnio);
    FreeLibrary(dll);

    std::cout << "\nDone!" << std::endl;
    return 0;
}
