#include <windows.h>
#include <iostream>
#include <iomanip>
#include <hidapi.h>

int main() {
    hid_init();
    struct hid_device_info* devs = hid_enumerate(0x3299, 0x4E9F);
    int count = 0;
    for (auto* cur = devs; cur; cur = cur->next) {
        count++;
        std::cout << "Interface " << count << ":" << std::endl;
        std::cout << "  Path: " << cur->path << std::endl;
        std::cout << "  Usage Page: 0x" << std::hex << cur->usage_page << std::dec << std::endl;
        std::cout << "  Usage: 0x" << std::hex << cur->usage << std::dec << std::endl;
        std::cout << "  Interface: " << cur->interface_number << std::endl;
        std::wcout << L"  Product: " << (cur->product_string ? cur->product_string : L"?") << std::endl;
        std::cout << std::endl;
    }
    hid_free_enumeration(devs);
    hid_exit();
    std::cout << "Total: " << count << " interfaces" << std::endl;
    return 0;
}
