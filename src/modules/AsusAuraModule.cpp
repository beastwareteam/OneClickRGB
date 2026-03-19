#include "../modules/IModule.h"
#include "../IRGBController.h"
#include "../../controllers/AsusAuraMainboardController.h"
#include <memory>

namespace OneClickRGB {

/**
 * @brief ASUS Aura Module
 *
 * Provides support for ASUS Aura-enabled devices including:
 * - ASUS Aura Mainboard controllers
 * - ASUS Aura Addressable LED headers
 * - ASUS Aura Terminal devices
 */
class AsusAuraModule : public IModule {
public:
    AsusAuraModule() = default;
    ~AsusAuraModule() override = default;

    // IModule interface implementation
    const char* GetModuleName() const override { return "AsusAura"; }
    const char* GetModuleVersion() const override { return "1.0.0"; }
    const char* GetModuleDescription() const override {
        return "ASUS Aura RGB controller module supporting mainboards, LED strips, and peripherals";
    }

    std::vector<std::string> GetSupportedDeviceTypes() const override {
        return {"motherboard", "ledstrip", "terminal"};
    }

    bool SupportsDevice(uint16_t vendorId, uint16_t productId) const override {
        // ASUS vendor ID
        if (vendorId != 0x0B05) return false;

        // Known ASUS Aura product IDs
        switch (productId) {
            case 0x1866: // ASUS Aura Mainboard
            case 0x18F3: // ASUS Aura Mainboard
            case 0x1939: // ASUS Aura Mainboard
            case 0x19AF: // ASUS Aura Mainboard (Steel Legend)
            case 0x1AA6: // ASUS Aura Mainboard
            case 0x1867: // ASUS Aura Addressable Header
            case 0x1872: // ASUS Aura Addressable Header
            case 0x1889: // ASUS Aura Terminal
                return true;
            default:
                return false;
        }
    }

    std::unique_ptr<RGBDevice> CreateController(
        const DeviceInfo& deviceInfo,
        const std::string& devicePath
    ) override {
        try {
            // Determine controller type based on product ID
            switch (deviceInfo.productId) {
                case 0x1866:
                case 0x18F3:
                case 0x1939:
                case 0x19AF:
                case 0x1AA6:
                    // Mainboard controller
                    return std::make_unique<AsusAuraMainboardController>(devicePath);

                case 0x1867:
                case 0x1872:
                    // Addressable LED controller
                    // TODO: Implement AsusAuraAddressableController
                    return nullptr;

                case 0x1889:
                    // Terminal controller
                    // TODO: Implement AsusAuraTerminalController
                    return nullptr;

                default:
                    return nullptr;
            }
        } catch (const std::exception& e) {
            std::cerr << "[AsusAuraModule] Failed to create controller: " << e.what() << std::endl;
            return nullptr;
        }
    }

    // Capabilities
    bool SupportsDirectMode() const override { return true; }
    bool SupportsEffectEngine() const override { return true; }
    bool SupportsCustomPatterns() const override { return true; }
    bool RequiresAdminPrivileges() const override { return false; }

    bool Initialize() override {
        std::cout << "[AsusAuraModule] ASUS Aura module initialized" << std::endl;
        return true;
    }

    void Shutdown() override {
        std::cout << "[AsusAuraModule] ASUS Aura module shutdown" << std::endl;
    }
};

} // namespace OneClickRGB

// Module factory function (required for dynamic loading)
extern "C" __declspec(dllexport) OneClickRGB::IModule* CreateModule() {
    return new OneClickRGB::AsusAuraModule();
}