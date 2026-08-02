#include "hid_backend_hidapi.h"

#include <windows.h>

#include "hidapi.h"

namespace hal {
namespace {

class HidapiDevice : public IHidDevice {
public:
    explicit HidapiDevice(hid_device* dev) : dev_(dev) {}
    ~HidapiDevice() override {
        if (dev_) hid_close(dev_);
    }

    int Write(const uint8_t* data, size_t len) override {
        return hid_write(dev_, data, len);
    }

    int ReadTimeout(uint8_t* data, size_t len, int timeout_ms) override {
        return hid_read_timeout(dev_, data, len, timeout_ms);
    }

private:
    hid_device* dev_;
};

}  // namespace

bool HidapiBackend::Init() {
    if (init_count_ == 0) {
        if (hid_init() != 0) return false;
    }
    ++init_count_;
    return true;
}

void HidapiBackend::Exit() {
    if (init_count_ == 0) return;
    if (--init_count_ == 0) hid_exit();
}

void HidapiBackend::HardReset() {
    hid_exit();
    ::Sleep(500);
    init_count_ = (hid_init() == 0) ? 1 : 0;
    if (init_count_ > 0) ::Sleep(200);
}

std::vector<HidDeviceInfo> HidapiBackend::Enumerate(uint16_t vid, uint16_t pid) {
    std::vector<HidDeviceInfo> out;
    hid_device_info* devs = hid_enumerate(vid, pid);
    for (hid_device_info* cur = devs; cur; cur = cur->next) {
        HidDeviceInfo info;
        info.path       = cur->path ? cur->path : "";
        info.vendor_id  = cur->vendor_id;
        info.product_id = cur->product_id;
        info.usage_page = cur->usage_page;
        info.interface_number = cur->interface_number;
        out.push_back(std::move(info));
    }
    if (devs) hid_free_enumeration(devs);
    return out;
}

std::unique_ptr<IHidDevice> HidapiBackend::Open(const std::string& path) {
    hid_device* dev = hid_open_path(path.c_str());
    if (!dev) return nullptr;
    return std::unique_ptr<IHidDevice>(new HidapiDevice(dev));
}

void HidapiBackend::Sleep(int ms) {
    ::Sleep(ms);
}

HidapiBackend& RealHid() {
    static HidapiBackend instance;
    return instance;
}

}  // namespace hal
