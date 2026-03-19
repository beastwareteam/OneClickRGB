// SMBus Full Scan - Find all devices including GPU
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <cstdint>

typedef HRESULT (__stdcall *pawnio_version_t)(PULONG version);
typedef HRESULT (__stdcall *pawnio_open_t)(PHANDLE handle);
typedef HRESULT (__stdcall *pawnio_load_t)(HANDLE handle, const UCHAR* blob, SIZE_T size);
typedef HRESULT (__stdcall *pawnio_execute_t)(HANDLE handle, PCSTR name, const ULONG64* in, SIZE_T in_size, PULONG64 out, SIZE_T out_size, PSIZE_T return_size);
typedef HRESULT (__stdcall *pawnio_close_t)(HANDLE handle);

pawnio_open_t p_pawnio_open = nullptr;
pawnio_load_t p_pawnio_load = nullptr;
pawnio_execute_t p_pawnio_execute = nullptr;
pawnio_close_t p_pawnio_close = nullptr;

HANDLE g_pawnio_handle = nullptr;

#define I2C_SMBUS_READ  1
#define I2C_SMBUS_WRITE 0
#define I2C_SMBUS_BYTE      1
#define I2C_SMBUS_BYTE_DATA 2

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

int SMBusReadByte(uint8_t addr) {
    i2c_smbus_data data;
    if (SMBusXfer(addr, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data) < 0) return -1;
    return data.byte;
}

int main() {
    std::cout << "=== SMBus Full Scan ===" << std::endl;

    HMODULE dll = LoadLibraryA("PawnIOLib.dll");
    if (!dll) { std::cout << "PawnIOLib.dll not found!" << std::endl; return 1; }

    p_pawnio_open = (pawnio_open_t)GetProcAddress(dll, "pawnio_open");
    p_pawnio_load = (pawnio_load_t)GetProcAddress(dll, "pawnio_load");
    p_pawnio_execute = (pawnio_execute_t)GetProcAddress(dll, "pawnio_execute");
    p_pawnio_close = (pawnio_close_t)GetProcAddress(dll, "pawnio_close");

    if (p_pawnio_open(&g_pawnio_handle) != S_OK) {
        std::cout << "Failed to open PawnIO!" << std::endl;
        return 1;
    }

    std::ifstream file("SmbusI801.bin", std::ios::binary);
    if (!file.is_open()) { std::cout << "SmbusI801.bin not found!" << std::endl; return 1; }
    std::vector<char> blob((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (p_pawnio_load(g_pawnio_handle, (const UCHAR*)blob.data(), blob.size()) != S_OK) {
        std::cout << "Failed to load SMBus module!" << std::endl;
        return 1;
    }

    std::cout << "\nScanning all SMBus addresses (0x00-0x7F)...\n" << std::endl;

    std::cout << "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f" << std::endl;

    for (int row = 0; row < 8; row++) {
        std::cout << std::hex << row << "0: ";
        for (int col = 0; col < 16; col++) {
            uint8_t addr = row * 16 + col;

            // Skip reserved addresses
            if (addr < 0x03 || addr > 0x77) {
                std::cout << "   ";
                continue;
            }

            int res = SMBusReadByte(addr);
            if (res >= 0) {
                std::cout << std::setfill('0') << std::setw(2) << (int)addr << " ";
            } else {
                std::cout << "-- ";
            }
        }
        std::cout << std::dec << std::endl;
    }

    std::cout << "\n\nKnown device types:" << std::endl;
    std::cout << "  0x50-0x57: SPD EEPROM / DDR5 Hub" << std::endl;
    std::cout << "  0x70-0x77: ENE DRAM RGB (DDR4/DDR5)" << std::endl;
    std::cout << "  0x29-0x2F: NVIDIA GPU I2C" << std::endl;
    std::cout << "  0x48-0x4F: GPU RGB Controller (some)" << std::endl;
    std::cout << "  0x60-0x67: ASUS Aura GPU" << std::endl;

    p_pawnio_close(g_pawnio_handle);
    FreeLibrary(dll);

    return 0;
}
