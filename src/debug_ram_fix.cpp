/**
 * RAM RGB Fix Attempt - Test different approaches to make RAM LEDs respond
 *
 * The issue: Colors are being written to registers, but LEDs don't change.
 * Possible causes:
 * 1. Direct mode not properly enabled
 * 2. Apply command not correct
 * 3. Need to disable software control first
 * 4. Different register set needed
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

void set_ram_color(uint8_t addr, uint8_t r, uint8_t g, uint8_t b, int method) {
    uint8_t led_count = ene_read(addr, 0x1C02);
    if (led_count == 0 || led_count > 20) led_count = 8;

    std::cout << "  Using method " << method << " with " << (int)led_count << " LEDs" << std::endl;

    switch (method) {
        case 1: {
            // Method 1: Original approach
            std::cout << "  Method 1: Direct mode 0x01, apply 0x80A0" << std::endl;
            ene_write(addr, 0x8020, 0x01); Sleep(10);
            for (int i = 0; i < led_count; i++) {
                ene_write(addr, 0x8100 + (i * 3) + 0, r);
                ene_write(addr, 0x8100 + (i * 3) + 1, b);  // RBG
                ene_write(addr, 0x8100 + (i * 3) + 2, g);
            }
            Sleep(10);
            ene_write(addr, 0x80A0, 0x01);
            break;
        }
        case 2: {
            // Method 2: Set mode to static (0x01) before colors
            std::cout << "  Method 2: Set mode 0x8021=0x01 first" << std::endl;
            ene_write(addr, 0x8021, 0x01); Sleep(10);  // Static mode
            ene_write(addr, 0x8020, 0x01); Sleep(10);  // Direct control
            for (int i = 0; i < led_count; i++) {
                ene_write(addr, 0x8100 + (i * 3) + 0, r);
                ene_write(addr, 0x8100 + (i * 3) + 1, b);
                ene_write(addr, 0x8100 + (i * 3) + 2, g);
            }
            Sleep(10);
            ene_write(addr, 0x80A0, 0x01);
            break;
        }
        case 3: {
            // Method 3: Disable effects first
            std::cout << "  Method 3: Disable effects (0x8021=0x00)" << std::endl;
            ene_write(addr, 0x8021, 0x00); Sleep(10);  // Off/direct mode
            for (int i = 0; i < led_count; i++) {
                ene_write(addr, 0x8100 + (i * 3) + 0, r);
                ene_write(addr, 0x8100 + (i * 3) + 1, b);
                ene_write(addr, 0x8100 + (i * 3) + 2, g);
            }
            Sleep(10);
            ene_write(addr, 0x80A0, 0x01);
            break;
        }
        case 4: {
            // Method 4: Use different apply register
            std::cout << "  Method 4: Apply via 0x80FF" << std::endl;
            ene_write(addr, 0x8020, 0x01); Sleep(10);
            for (int i = 0; i < led_count; i++) {
                ene_write(addr, 0x8100 + (i * 3) + 0, r);
                ene_write(addr, 0x8100 + (i * 3) + 1, b);
                ene_write(addr, 0x8100 + (i * 3) + 2, g);
            }
            Sleep(10);
            ene_write(addr, 0x80FF, 0x01);  // Alternative apply
            break;
        }
        case 5: {
            // Method 5: Write to save register
            std::cout << "  Method 5: Save config (0x8000=0xAA)" << std::endl;
            ene_write(addr, 0x8020, 0x01); Sleep(10);
            for (int i = 0; i < led_count; i++) {
                ene_write(addr, 0x8100 + (i * 3) + 0, r);
                ene_write(addr, 0x8100 + (i * 3) + 1, b);
                ene_write(addr, 0x8100 + (i * 3) + 2, g);
            }
            Sleep(10);
            ene_write(addr, 0x8000, 0xAA);  // Save command
            break;
        }
        case 6: {
            // Method 6: Full OpenRGB sequence
            std::cout << "  Method 6: Full OpenRGB sequence" << std::endl;
            // Disable software control first
            ene_write(addr, 0x8020, 0x00); Sleep(50);
            // Enable direct mode
            ene_write(addr, 0x8020, 0x01); Sleep(50);
            // Set colors
            for (int i = 0; i < led_count; i++) {
                ene_write(addr, 0x8100 + (i * 3) + 0, r);
                ene_write(addr, 0x8100 + (i * 3) + 1, b);
                ene_write(addr, 0x8100 + (i * 3) + 2, g);
            }
            Sleep(50);
            // Apply
            ene_write(addr, 0x80A0, 0x01);
            Sleep(100);
            break;
        }
        default:
            std::cout << "  Unknown method!" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    uint8_t r = 255, g = 0, b = 0;  // Default red
    int method = 0;  // 0 = try all

    if (argc >= 4) {
        r = atoi(argv[1]);
        g = atoi(argv[2]);
        b = atoi(argv[3]);
    }
    if (argc >= 5) {
        method = atoi(argv[4]);
    }

    std::cout << "=== RAM RGB Fix Test ===" << std::endl;
    std::cout << "Target: RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;
    if (method > 0) {
        std::cout << "Testing method: " << method << std::endl;
    } else {
        std::cout << "Testing ALL methods (press Enter between each)" << std::endl;
    }
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

    std::cout << "PawnIO initialized" << std::endl << std::endl;

    // Find RAM modules
    std::vector<uint8_t> found_addrs;
    uint8_t addrs[] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};

    for (uint8_t addr : addrs) {
        if (read_byte(addr) < 0) continue;

        char name[17] = {0};
        for (int i = 0; i < 16; i++) name[i] = ene_read(addr, 0x1000 + i);

        if (strstr(name, "AUDA") || strstr(name, "DIMM") || strstr(name, "Trident")) {
            std::cout << "Found: 0x" << std::hex << (int)addr << std::dec << " - " << name << std::endl;
            found_addrs.push_back(addr);
        }
    }

    if (found_addrs.empty()) {
        std::cout << "No RAM RGB modules found!" << std::endl;
        p_close(g_pawnio);
        FreeLibrary(dll);
        return 1;
    }

    std::cout << std::endl;

    // Test methods
    if (method > 0 && method <= 6) {
        // Test single method
        for (uint8_t addr : found_addrs) {
            std::cout << "Device 0x" << std::hex << (int)addr << std::dec << ":" << std::endl;
            set_ram_color(addr, r, g, b, method);
        }
    } else {
        // Test all methods
        for (int m = 1; m <= 6; m++) {
            std::cout << "=== Testing Method " << m << " ===" << std::endl;
            for (uint8_t addr : found_addrs) {
                std::cout << "Device 0x" << std::hex << (int)addr << std::dec << ":" << std::endl;
                set_ram_color(addr, r, g, b, m);
            }
            std::cout << std::endl;
            std::cout << "Did the RAM LEDs change? Press Enter to try next method..." << std::endl;
            std::cin.get();
        }
    }

    p_close(g_pawnio);
    FreeLibrary(dll);

    std::cout << "\nDone!" << std::endl;
    return 0;
}
