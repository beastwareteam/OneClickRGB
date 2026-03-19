#include "../modules/IModule.h"
#include "../IRGBController.h"
#include "../../controllers/SteelSeriesRival600Controller.h"
#include <memory>

namespace OneClickRGB {

/**
 * @brief SteelSeries RGB Module
 *
 * Provides support for SteelSeries RGB gaming peripherals including:
 * - SteelSeries Rival gaming mice
 * - SteelSeries Apex keyboards
 * - SteelSeries Arctis headsets
 */
class SteelSeriesModule : public IModule {
public:
    SteelSeriesModule() = default;
    ~SteelSeriesModule() override = default;

    // IModule interface implementation
    const char* GetModuleName() const override { return "SteelSeries"; }
    const char* GetModuleVersion() const override { return "1.0.0"; }
    const char* GetModuleDescription() const override {
        return "SteelSeries RGB controller module for gaming mice, keyboards, and peripherals";
    }

    std::vector<std::string> GetSupportedDeviceTypes() const override {
        return {"mouse", "keyboard", "headset"};
    }

    bool SupportsDevice(uint16_t vendorId, uint16_t productId) const override {
        // SteelSeries vendor ID
        if (vendorId != 0x1038) return false;

        // Known SteelSeries product IDs
        switch (productId) {
            // Mice
            case 0x1724: // SteelSeries Rival 600
            case 0x1832: // SteelSeries Sensei Ten
            case 0x1836: // SteelSeries Aerox 3
            case 0x184C: // SteelSeries Rival 3
            // Keyboards
            case 0x1610: // SteelSeries Apex Pro
            case 0x1612: // SteelSeries Apex 7
            case 0x161C: // SteelSeries Apex 5
            case 0x161A: // SteelSeries Apex 3
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
                case 0x1724: // SteelSeries Rival 600
                    return std::make_unique<SteelSeriesRival600Controller>(devicePath);

                case 0x1832: // SteelSeries Sensei Ten
                case 0x1836: // SteelSeries Aerox 3
                case 0x184C: // SteelSeries Rival 3
                    // TODO: Implement additional mouse controllers
                    return nullptr;

                case 0x1610: // SteelSeries Apex Pro
                case 0x1612: // SteelSeries Apex 7
                case 0x161C: // SteelSeries Apex 5
                case 0x161A: // SteelSeries Apex 3
                    // TODO: Implement keyboard controllers
                    return nullptr;

                default:
                    return nullptr;
            }
        } catch (const std::exception& e) {
            std::cerr << "[SteelSeriesModule] Failed to create controller: " << e.what() << std::endl;
            return nullptr;
        }
    }

    // Capabilities
    bool SupportsDirectMode() const override { return true; }
    bool SupportsEffectEngine() const override { return true; }
    bool SupportsCustomPatterns() const override { return false; }
    bool RequiresAdminPrivileges() const override { return false; }

    bool Initialize() override {
        std::cout << "[SteelSeriesModule] SteelSeries module initialized" << std::endl;
        return true;
    }

    void Shutdown() override {
        std::cout << "[SteelSeriesModule] SteelSeries module shutdown" << std::endl;
    }
};

} // namespace OneClickRGB

// Module factory function (required for dynamic loading)
extern "C" __declspec(dllexport) OneClickRGB::IModule* CreateModule() {
    return new OneClickRGB::SteelSeriesModule();
}