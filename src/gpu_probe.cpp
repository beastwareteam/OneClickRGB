// GPU RGB Probe - Check potential GPU addresses
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

int SMBusWriteByteData(uint8_t addr, uint8_t cmd, uint8_t value) {
    i2c_smbus_data data;
    data.byte = value;
    return SMBusXfer(addr, I2C_SMBUS_WRITE, cmd, I2C_SMBUS_BYTE_DATA, &data);
}

uint8_t ENERegisterRead(uint8_t addr, uint16_t reg) {
    uint16_t reg_swapped = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF);
    SMBusWriteWordData(addr, 0x00, reg_swapped);
    Sleep(1);
    int res = SMBusReadByteData(addr, 0x81);
    return (res >= 0) ? (uint8_t)res : 0;
}

void ProbeAddress(uint8_t addr) {
    std::cout << "\n=== Probing 0x" << std::hex << (int)addr << std::dec << " ===" << std::endl;

    // Try reading first 16 bytes as simple byte data
    std::cout << "Simple byte read (0x00-0x0F): ";
    for (int i = 0; i < 16; i++) {
        int val = SMBusReadByteData(addr, i);
        if (val >= 0) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) << val << " ";
        } else {
            std::cout << "-- ";
        }
    }
    std::cout << std::dec << std::endl;

    // Try ENE protocol (16-bit register addressing)
    std::cout << "ENE protocol (0x1000 device name): ";
    char name[17] = {0};
    for (int i = 0; i < 16; i++) {
        name[i] = ENERegisterRead(addr, 0x1000 + i);
    }
    bool valid_name = false;
    for (int i = 0; i < 16; i++) {
        if (name[i] >= 0x20 && name[i] < 0x7F) valid_name = true;
    }
    if (valid_name) {
        std::cout << "\"" << name << "\"" << std::endl;
    } else {
        std::cout << "(no valid name)" << std::endl;
    }

    // Check for ASUS Aura GPU signatures
    std::cout << "Checking for Aura GPU..." << std::endl;

    // ASUS Aura GPU uses register 0x20 for firmware version
    int fw = SMBusReadByteData(addr, 0x20);
    if (fw >= 0) {
        std::cout << "  Register 0x20 (FW?): 0x" << std::hex << fw << std::dec << std::endl;
    }

    // Check register 0x30 (LED control on some GPUs)
    int led = SMBusReadByteData(addr, 0x30);
    if (led >= 0) {
        std::cout << "  Register 0x30 (LED?): 0x" << std::hex << led << std::dec << std::endl;
    }
}

int main() {
    std::cout << "=== GPU RGB Probe ===" << std::endl;

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

    // Probe the unknown addresses
    uint8_t addresses[] = {0x44, 0x49, 0x4B};
    for (uint8_t addr : addresses) {
        ProbeAddress(addr);
    }

    p_pawnio_close(g_pawnio_handle);
    FreeLibrary(dll);

    return 0;
}
