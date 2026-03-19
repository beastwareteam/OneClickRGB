// GPU Info - Get exact GPU model and scan I2C addresses
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <cstring>

#define ADL_OK 0
#define ADL_MAX_PATH 256
#define ADL_DL_I2C_ACTIONREAD  0x00000001
#define ADL_DL_I2C_ACTIONWRITE 0x00000002

typedef void* ADL_CONTEXT_HANDLE;
typedef void* (__stdcall *ADL_MAIN_MALLOC_CALLBACK)(int);

struct AdapterInfoX2 {
    int iSize;
    int iAdapterIndex;
    char strUDID[ADL_MAX_PATH];
    int iBusNumber;
    int iDeviceNumber;
    int iFunctionNumber;
    int iVendorID;
    char strAdapterName[ADL_MAX_PATH];
    char strDisplayName[ADL_MAX_PATH];
    int iPresent;
    int iExist;
    char strDriverPath[ADL_MAX_PATH];
    char strDriverPathExt[ADL_MAX_PATH];
    char strPNPString[ADL_MAX_PATH];
    int iOSDisplayIndex;
    int iInfoMask;
    int iInfoValue;
};

struct ADLI2C {
    int iSize;
    int iLine;
    int iAddress;
    int iOffset;
    int iAction;
    int iSpeed;
    int iDataSize;
    char *pcData;
};

typedef int (*ADL2_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
typedef int (*ADL2_MAIN_CONTROL_DESTROY)(ADL_CONTEXT_HANDLE);
typedef int (*ADL2_ADAPTER_NUMBEROFADAPTERS_GET)(ADL_CONTEXT_HANDLE, int*);
typedef int (*ADL2_ADAPTER_PRIMARY_GET)(ADL_CONTEXT_HANDLE, int*);
typedef int (*ADL2_ADAPTER_ADAPTERINFOX4_GET)(ADL_CONTEXT_HANDLE, int, int*, AdapterInfoX2**);
typedef int (*ADL2_DISPLAY_WRITEANDREADI2C)(ADL_CONTEXT_HANDLE, int, ADLI2C*);

void* __stdcall ADL_Main_Memory_Alloc(int iSize) { return malloc(iSize); }

int main() {
    std::cout << "=== GPU Info & I2C Scan ===" << std::endl;

    HMODULE dll = LoadLibraryA("atiadlxx.dll");
    if (!dll) dll = LoadLibraryA("atiadlxy.dll");
    if (!dll) {
        std::cout << "AMD ADL not found" << std::endl;
        return 1;
    }

    auto ADL2_Main_Control_Create = (ADL2_MAIN_CONTROL_CREATE)GetProcAddress(dll, "ADL2_Main_Control_Create");
    auto ADL2_Main_Control_Destroy = (ADL2_MAIN_CONTROL_DESTROY)GetProcAddress(dll, "ADL2_Main_Control_Destroy");
    auto ADL2_Adapter_NumberOfAdapters_Get = (ADL2_ADAPTER_NUMBEROFADAPTERS_GET)GetProcAddress(dll, "ADL2_Adapter_NumberOfAdapters_Get");
    auto ADL2_Adapter_AdapterInfoX4_Get = (ADL2_ADAPTER_ADAPTERINFOX4_GET)GetProcAddress(dll, "ADL2_Adapter_AdapterInfoX4_Get");
    auto ADL2_Adapter_Primary_Get = (ADL2_ADAPTER_PRIMARY_GET)GetProcAddress(dll, "ADL2_Adapter_Primary_Get");
    auto ADL2_Display_WriteAndReadI2C = (ADL2_DISPLAY_WRITEANDREADI2C)GetProcAddress(dll, "ADL2_Display_WriteAndReadI2C");

    ADL_CONTEXT_HANDLE context;
    if (ADL2_Main_Control_Create(ADL_Main_Memory_Alloc, 1, &context) != ADL_OK) {
        std::cout << "ADL init failed" << std::endl;
        return 1;
    }

    int num_adapters = 0;
    ADL2_Adapter_NumberOfAdapters_Get(context, &num_adapters);
    std::cout << "Found " << num_adapters << " adapter(s)" << std::endl;

    int num_devices = 0;
    AdapterInfoX2* info = nullptr;
    if (ADL2_Adapter_AdapterInfoX4_Get(context, -1, &num_devices, &info) == ADL_OK) {
        std::cout << "\n=== Adapters ===" << std::endl;
        for (int i = 0; i < num_devices; i++) {
            std::cout << "\nAdapter " << i << ":" << std::endl;
            std::cout << "  Name: " << info[i].strAdapterName << std::endl;
            std::cout << "  PNP:  " << info[i].strPNPString << std::endl;
            std::cout << "  Vendor ID: 0x" << std::hex << info[i].iVendorID << std::dec << std::endl;
            std::cout << "  Bus: " << info[i].iBusNumber << std::endl;
            std::cout << "  Present: " << info[i].iPresent << ", Exist: " << info[i].iExist << std::endl;
        }
    }

    int primary = 0;
    ADL2_Adapter_Primary_Get(context, &primary);
    std::cout << "\nPrimary adapter: " << primary << std::endl;

    // Scan I2C addresses on GPU
    std::cout << "\n=== GPU I2C Scan (Adapter " << primary << ") ===" << std::endl;
    std::cout << "Scanning addresses 0x08-0x77..." << std::endl;

    // Try different I2C lines (0, 1, 2)
    for (int line = 0; line <= 2; line++) {
        std::cout << "\nI2C Line " << line << ":" << std::endl;
        std::cout << "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f" << std::endl;

        for (int row = 0; row < 8; row++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (row * 16) << ": ";

            for (int col = 0; col < 16; col++) {
                int addr = row * 16 + col;
                if (addr < 0x08 || addr > 0x77) {
                    std::cout << "   ";
                    continue;
                }

                char data[4] = {0};
                ADLI2C i2c;
                i2c.iSize = sizeof(ADLI2C);
                i2c.iSpeed = 100;
                i2c.iLine = line;
                i2c.iAddress = addr << 1;
                i2c.iOffset = 0;
                i2c.iAction = ADL_DL_I2C_ACTIONREAD;
                i2c.iDataSize = 1;
                i2c.pcData = data;

                int ret = ADL2_Display_WriteAndReadI2C(context, primary, &i2c);

                if (ret == ADL_OK) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') << addr << " ";
                } else {
                    std::cout << "-- ";
                }
            }
            std::cout << std::dec << std::endl;
        }
    }

    // Try to identify RGB controller at known addresses
    std::cout << "\n=== Checking known RGB addresses ===" << std::endl;

    uint8_t rgb_addrs[] = {0x22, 0x29, 0x2A, 0x48, 0x49, 0x4A, 0x60, 0x61, 0x62, 0x68, 0x69};

    for (uint8_t addr : rgb_addrs) {
        for (int line = 0; line <= 2; line++) {
            char data[8] = {0};
            ADLI2C i2c;
            i2c.iSize = sizeof(ADLI2C);
            i2c.iSpeed = 100;
            i2c.iLine = line;
            i2c.iAddress = addr << 1;
            i2c.iOffset = 0x00;  // Read register 0
            i2c.iAction = ADL_DL_I2C_ACTIONREAD;
            i2c.iDataSize = 4;
            i2c.pcData = data;

            int ret = ADL2_Display_WriteAndReadI2C(context, primary, &i2c);

            if (ret == ADL_OK) {
                std::cout << "Found device at 0x" << std::hex << (int)addr
                          << " on line " << line << ": ";
                for (int j = 0; j < 4; j++) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << (int)(uint8_t)data[j] << " ";
                }
                std::cout << std::dec << std::endl;
            }
        }
    }

    ADL2_Main_Control_Destroy(context);
    FreeLibrary(dll);

    std::cout << "\nDone!" << std::endl;
    return 0;
}
