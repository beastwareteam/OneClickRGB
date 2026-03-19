/*---------------------------------------------------------*\
| SteelSeriesRival600Controller.h                           |
|                                                           |
| SteelSeries Rival 600 RGB Controller for OneClickRGB      |
| Based on OpenRGB implementation                           |
\*---------------------------------------------------------*/

#pragma once

#include <string>
#include <hidapi.h>

namespace OneClickRGB {

// Rival 600 has 8 RGB zones
#define RIVAL_600_NUM_ZONES 8

class SteelSeriesRival600Controller
{
public:
    SteelSeriesRival600Controller(hid_device* dev_handle, const char* path);
    ~SteelSeriesRival600Controller();

    bool Initialize();

    std::string GetDeviceLocation();
    std::string GetDeviceName();

    // Set color for a specific zone (0-7)
    void SetZoneColor(unsigned char zone_id, unsigned char red, unsigned char green, unsigned char blue);

    // Set all zones to the same color
    void SetAllZones(unsigned char red, unsigned char green, unsigned char blue);

    // Save current settings to device memory
    void SaveMode();

private:
    hid_device* dev;
    std::string location;
    std::string name;
};

} // namespace OneClickRGB
