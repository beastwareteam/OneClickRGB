/*---------------------------------------------------------*\
| SteelSeriesRival600Controller.cpp                         |
|                                                           |
| SteelSeries Rival 600 RGB Controller for OneClickRGB      |
| Based on OpenRGB implementation                           |
\*---------------------------------------------------------*/

#include "SteelSeriesRival600Controller.h"
#include <cstring>
#include <iostream>

namespace OneClickRGB {

SteelSeriesRival600Controller::SteelSeriesRival600Controller(hid_device* dev_handle, const char* path)
{
    dev = dev_handle;
    location = path;
    name = "SteelSeries Rival 600";
}

SteelSeriesRival600Controller::~SteelSeriesRival600Controller()
{
    if (dev)
    {
        hid_close(dev);
        dev = nullptr;
    }
}

bool SteelSeriesRival600Controller::Initialize()
{
    if (!dev)
    {
        std::cerr << "[SteelSeries] No device handle!\n";
        return false;
    }

    std::cout << "[SteelSeries] Rival 600 initialized\n";
    return true;
}

std::string SteelSeriesRival600Controller::GetDeviceLocation()
{
    return "HID: " + location;
}

std::string SteelSeriesRival600Controller::GetDeviceName()
{
    return name;
}

void SteelSeriesRival600Controller::SetZoneColor(unsigned char zone_id, unsigned char red, unsigned char green, unsigned char blue)
{
    if (zone_id >= RIVAL_600_NUM_ZONES)
    {
        return;
    }

    // Rival 600: 7-byte packet, NO Report ID (verified working!)
    unsigned char usb_pkt[7];
    memset(usb_pkt, 0x00, sizeof(usb_pkt));

    usb_pkt[0] = 0x1C;           // Command
    usb_pkt[1] = 0x27;           // Sub-command
    usb_pkt[2] = 0x00;           // Reserved
    usb_pkt[3] = 1 << zone_id;   // Zone mask
    usb_pkt[4] = red;
    usb_pkt[5] = green;
    usb_pkt[6] = blue;

    hid_write(dev, usb_pkt, 7);
}

void SteelSeriesRival600Controller::SetAllZones(unsigned char red, unsigned char green, unsigned char blue)
{
    // Set each zone individually (verified working method)
    for (int zone = 0; zone < RIVAL_600_NUM_ZONES; zone++)
    {
        SetZoneColor(zone, red, green, blue);
    }
}

void SteelSeriesRival600Controller::SaveMode()
{
    // Save command: 0x09 with Report ID prefix (10 bytes total)
    unsigned char usb_pkt[10];
    memset(usb_pkt, 0x00, sizeof(usb_pkt));

    usb_pkt[0] = 0x00;  // Report ID
    usb_pkt[1] = 0x09;  // Save command

    hid_write(dev, usb_pkt, 10);
}

} // namespace OneClickRGB
