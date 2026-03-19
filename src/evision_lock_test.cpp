#include <windows.h>
#include <iostream>
#include <hidapi.h>
#include <cstring>

#define EVISION_VID 0x3299
#define EVISION_PID 0x4E9F
#define EVISION_USAGE_PAGE 0xFF1C
#define EVISION_V2_REPORT_ID 0x04
#define EVISION_V2_PACKET_SIZE 64

int EVisionQuery(hid_device* dev, uint8_t cmd, uint16_t offset, const uint8_t* idata, uint8_t size, uint8_t* odata) {
    uint8_t buffer[EVISION_V2_PACKET_SIZE];
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = EVISION_V2_REPORT_ID;
    buffer[3] = cmd;
    buffer[4] = size;
    buffer[5] = offset & 0xff;
    buffer[6] = (offset >> 8) & 0xff;
    if (idata && size > 0) memcpy(buffer + 8, idata, size);
    uint16_t chksum = 0;
    for (int i = 3; i < EVISION_V2_PACKET_SIZE; i++) chksum += buffer[i];
    buffer[1] = chksum & 0xff;
    buffer[2] = (chksum >> 8) & 0xff;
    if (hid_write(dev, buffer, sizeof(buffer)) < 0) return -1;
    int bytes_read, retries = 10;
    do { bytes_read = hid_read_timeout(dev, buffer, sizeof(buffer), 100); retries--; } while (bytes_read > 0 && buffer[0] != EVISION_V2_REPORT_ID && retries > 0);
    if (bytes_read != sizeof(buffer)) return -2;
    if (odata && buffer[4] > 0) memcpy(odata, buffer + 8, buffer[4]);
    return buffer[4];
}

int main(int argc, char* argv[]) {
    bool lock = (argc > 1 && strcmp(argv[1], "lock") == 0);
    hid_init();
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);
    if (!dev) { std::cout << "Keyboard not found!" << std::endl; return 1; }
    
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr); // Begin
    Sleep(20);
    
    uint8_t val[2] = { lock ? (uint8_t)0x01 : (uint8_t)0x00, lock ? (uint8_t)0x01 : (uint8_t)0x00 };
    EVisionQuery(dev, 0x06, 0x14, val, 2, nullptr); // Write
    Sleep(10);
    
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr); // End
    
    std::cout << "Windows-Taste: " << (lock ? "GESPERRT" : "ENTSPERRT") << std::endl;
    hid_close(dev);
    hid_exit();
    return 0;
}
