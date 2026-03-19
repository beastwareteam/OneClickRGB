// ENE DRAM Debug Tool - Read config table and LED info
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <cstdint>

// PawnIO function pointers
typedef HRESULT (__stdcall *pawnio_version_t)(PULONG version);
typedef HRESULT (__stdcall *pawnio_open_t)(PHANDLE handle);
typedef HRESULT (__stdcall *pawnio_load_t)(HANDLE handle, const UCHAR* blob, SIZE_T size);
typedef HRESULT (__stdcall *pawnio_execute_t)(HANDLE handle, PCSTR name, const ULONG64* in, SIZE_T in_size, PULONG64 out, SIZE_T out_size, PSIZE_T return_size);
typedef HRESULT (__stdcall *pawnio_close_t)(HANDLE handle);

pawnio_version_t p_pawnio_version = nullptr;
pawnio_open_t p_pawnio_open = nullptr;
pawnio_load_t p_pawnio_load = nullptr;
pawnio_execute_t p_pawnio_execute = nullptr;
pawnio_close_t p_pawnio_close = nullptr;

HANDLE g_pawnio_handle = nullptr;
HMODULE g_pawnio_dll = nullptr;

#define I2C_SMBUS_READ  1
#define I2C_SMBUS_WRITE 0
#define I2C_SMBUS_BYTE_DATA 2
#define I2C_SMBUS_WORD_DATA 3

union i2c_smbus_data {
    uint8_t  byte;
    uint16_t word;
    uint8_t  block[34];
};

int SMBusXfer(uint8_t addr, char read_write, uint8_t command, int size, i2c_smbus_data* data) {
    const SIZE_T in_size = 9;
    ULONG64 in[in_size] = {0};
    const SIZE_T out_size = 5;
    ULONG64 out[out_size] = {0};
    SIZE_T return_size;

    in[0] = addr;
    in[1] = read_write;
    in[2] = command;
    in[3] = size;

    if (data) memcpy(&in[4], data, sizeof(i2c_smbus_data));

    HRESULT hr = p_pawnio_execute(g_pawnio_handle, "ioctl_smbus_xfer", in, in_size, out, out_size, &return_size);

    if (data) memcpy(data, &out[0], sizeof(i2c_smbus_data));

    return hr == S_OK ? 0 : -1;
}

int SMBusReadByteData(uint8_t addr, uint8_t cmd) {
    i2c_smbus_data data;
    if (SMBusXfer(addr, I2C_SMBUS_READ, cmd, I2C_SMBUS_BYTE_DATA, &data) < 0) return -1;
    return data.byte;
}

int SMBusWriteWordData(uint8_t addr, uint8_t cmd, uint16_t value) {
    i2c_smbus_data data;
    data.word = value;
    return SMBusXfer(addr, I2C_SMBUS_WRITE, cmd, I2C_SMBUS_WORD_DATA, &data);
}

uint8_t ENERegisterRead(uint8_t addr, uint16_t reg) {
    uint16_t reg_swapped = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF);
    SMBusWriteWordData(addr, 0x00, reg_swapped);
    Sleep(1);
    int res = SMBusReadByteData(addr, 0x81);
    return (res >= 0) ? (uint8_t)res : 0;
}

int main() {
    std::cout << "=== ENE DRAM Debug Tool ===" << std::endl;

    // Load PawnIO
    g_pawnio_dll = LoadLibraryA("PawnIOLib.dll");
    if (!g_pawnio_dll) {
        std::cout << "PawnIOLib.dll not found!" << std::endl;
        return 1;
    }

    p_pawnio_version = (pawnio_version_t)GetProcAddress(g_pawnio_dll, "pawnio_version");
    p_pawnio_open = (pawnio_open_t)GetProcAddress(g_pawnio_dll, "pawnio_open");
    p_pawnio_load = (pawnio_load_t)GetProcAddress(g_pawnio_dll, "pawnio_load");
    p_pawnio_execute = (pawnio_execute_t)GetProcAddress(g_pawnio_dll, "pawnio_execute");
    p_pawnio_close = (pawnio_close_t)GetProcAddress(g_pawnio_dll, "pawnio_close");

    if (p_pawnio_open(&g_pawnio_handle) != S_OK) {
        std::cout << "Failed to open PawnIO!" << std::endl;
        return 1;
    }

    // Load SMBus module
    std::ifstream file("SmbusI801.bin", std::ios::binary);
    if (!file.is_open()) {
        std::cout << "SmbusI801.bin not found!" << std::endl;
        return 1;
    }
    std::vector<char> blob((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (p_pawnio_load(g_pawnio_handle, (const UCHAR*)blob.data(), blob.size()) != S_OK) {
        std::cout << "Failed to load SMBus module!" << std::endl;
        return 1;
    }

    std::cout << "PawnIO initialized.\n" << std::endl;

    // Scan ENE addresses
    uint8_t ene_addresses[] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};

    for (uint8_t addr : ene_addresses) {
        // Read device name
        char name[17] = {0};
        for (int i = 0; i < 16; i++) {
            name[i] = ENERegisterRead(addr, 0x1000 + i);
        }

        if (name[0] == 0 || name[0] == 0xFF) continue;

        std::cout << "=== Device at 0x" << std::hex << (int)addr << std::dec << ": " << name << " ===" << std::endl;

        // Read config table (64 bytes at 0x1C00)
        std::cout << "\nConfig Table (0x1C00):" << std::endl;
        for (int row = 0; row < 4; row++) {
            std::cout << "  " << std::hex << std::setfill('0') << std::setw(2) << (row * 16) << ": ";
            for (int col = 0; col < 16; col++) {
                uint8_t val = ENERegisterRead(addr, 0x1C00 + row * 16 + col);
                std::cout << std::setw(2) << (int)val << " ";
            }
            std::cout << std::dec << std::endl;
        }

        // Interpret key values
        uint8_t led_count_02 = ENERegisterRead(addr, 0x1C02);
        uint8_t led_count_03 = ENERegisterRead(addr, 0x1C03);

        std::cout << "\nLED Count at offset 0x02: " << (int)led_count_02 << std::endl;
        std::cout << "LED Count at offset 0x03: " << (int)led_count_03 << std::endl;

        // Read current direct colors
        std::cout << "\nDirect Colors V2 (0x8100):" << std::endl;
        for (int i = 0; i < 10; i++) {
            uint8_t r = ENERegisterRead(addr, 0x8100 + i * 3 + 0);
            uint8_t b = ENERegisterRead(addr, 0x8100 + i * 3 + 1);
            uint8_t g = ENERegisterRead(addr, 0x8100 + i * 3 + 2);
            std::cout << "  LED " << i << ": R=" << (int)r << " G=" << (int)g << " B=" << (int)b << std::endl;
        }

        std::cout << std::endl;
    }

    p_pawnio_close(g_pawnio_handle);
    FreeLibrary(g_pawnio_dll);

    return 0;
}
