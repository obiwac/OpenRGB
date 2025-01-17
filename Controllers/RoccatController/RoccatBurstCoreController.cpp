/*-------------------------------------------------------------------*\
|  RoccatBurstCoreController.cpp                                      |
|                                                                     |
|  Driver for Roccat Horde Aimo Keyboard                              |
|                                                                     |
|  Morgan Guimard (morg)          2/24/2022                           |
|                                                                     |
\*-------------------------------------------------------------------*/

#include "RoccatBurstCoreController.h"

#include <cstring>

RoccatBurstCoreController::RoccatBurstCoreController(hid_device* dev_handle, const hid_device_info& info)
{
    dev                 = dev_handle;
    version             = "";
    location            = info.path;

    wchar_t serial_string[128];
    int ret = hid_get_serial_number_string(dev, serial_string, 128);

    if(ret != 0)
    {
        serial_number = "";
    }
    else
    {
        std::wstring return_wstring = serial_string;
        serial_number = std::string(return_wstring.begin(), return_wstring.end());
    }

    SetupDirectMode();
}

RoccatBurstCoreController::~RoccatBurstCoreController()
{
    hid_close(dev);
}

std::string RoccatBurstCoreController::GetFirmwareVersion()
{
    return version;
}

std::string RoccatBurstCoreController::GetSerialString()
{
    return serial_number;
}

std::string RoccatBurstCoreController::GetDeviceLocation()
{
    return("HID: " + location);
}

void RoccatBurstCoreController::SetupDirectMode()
{
    SwitchControl(true);
}

void RoccatBurstCoreController::SwitchControl(bool direct)
{
    unsigned char usb_buf[ROCCAT_BURST_CORE_CONTROL_MODE_PACKET_LENGTH];

    usb_buf[0x00] = 0x0E;
    usb_buf[0x01] = 0x06;
    usb_buf[0x02] = 0x01;
    usb_buf[0x03] = direct ? 0x01 : 0x00;
    usb_buf[0x04] = 0x00;
    usb_buf[0x05] = 0xFF;

    hid_send_feature_report(dev, usb_buf, ROCCAT_BURST_CORE_CONTROL_MODE_PACKET_LENGTH);
}

void RoccatBurstCoreController::SendDirect(std::vector<RGBColor> colors)
{
    unsigned char usb_buf[ROCCAT_BURST_CORE_DIRECT_MODE_PACKET_LENGTH];

    memset(usb_buf, 0x00, ROCCAT_BURST_CORE_DIRECT_MODE_PACKET_LENGTH);

    usb_buf[0x00] = ROCCAT_BURST_CORE_DIRECT_MODE_REPORT_ID;
    usb_buf[0x01] = ROCCAT_BURST_CORE_DIRECT_MODE_BYTE;

    usb_buf[0x02] = RGBGetRValue(colors[0]);
    usb_buf[0x03] = RGBGetGValue(colors[0]);
    usb_buf[0x04] = RGBGetBValue(colors[0]);

    hid_send_feature_report(dev, usb_buf, ROCCAT_BURST_CORE_DIRECT_MODE_PACKET_LENGTH);
}

void RoccatBurstCoreController::SetMode(std::vector<RGBColor> colors, unsigned char mode_value, unsigned char speed, unsigned char brightness, unsigned int color_mode, unsigned int mode_flags)
{
    /*---------------------------------------------------------*\
    | 1. Read from flash                                        |
    \*---------------------------------------------------------*/
    unsigned char usb_buf[ROCCAT_BURST_CORE_FLASH_PACKET_LENGTH];
    memset(usb_buf, 0x00, ROCCAT_BURST_CORE_FLASH_PACKET_LENGTH);

    usb_buf[0x00] = 0x06;

    hid_get_feature_report(dev, usb_buf, ROCCAT_BURST_CORE_FLASH_PACKET_LENGTH);

    /*---------------------------------------------------------*\
    | 2. Update needed bytes                                    |
    \*---------------------------------------------------------*/
    usb_buf[0x01] = 0x3F;
    usb_buf[0x03] = 0x06;
    usb_buf[0x04] = 0x06;
    usb_buf[0x05] = 0x1F;

    usb_buf[30] = mode_value;
    usb_buf[31] = mode_flags & MODE_FLAG_HAS_SPEED ? speed : 0xFF;
    usb_buf[32] = brightness;

    usb_buf[34] = 0xFF;

    if(color_mode & MODE_COLORS_MODE_SPECIFIC)
    {
        usb_buf[36] = 0x14;
        usb_buf[37] = 0xFF;
        usb_buf[38] = RGBGetRValue(colors[0]);
        usb_buf[39] = RGBGetGValue(colors[0]);
        usb_buf[40] = RGBGetBValue(colors[0]);
    }
    else if (color_mode & MODE_COLORS_NONE)
    {
        usb_buf[38] = 0xF4;
        usb_buf[39] = 0x00;
        usb_buf[40] = 0x00;
    }

    unsigned int crc = CalculateCRC(&usb_buf[0]);

    usb_buf[61] = (unsigned char) crc;
    usb_buf[62] = crc >> 8;

    /*---------------------------------------------------------*\
    | 3. Send to flash                                          |
    \*---------------------------------------------------------*/
    hid_send_feature_report(dev, usb_buf, ROCCAT_BURST_CORE_FLASH_PACKET_LENGTH);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    /*---------------------------------------------------------*\
    | 4. Switch to built-in mode                                |
    \*---------------------------------------------------------*/
    SwitchControl(false);
}

unsigned int RoccatBurstCoreController::CalculateCRC(unsigned char* bytes)
{
    unsigned int crc = 0;

    for(unsigned int i = 0; i < ROCCAT_BURST_CORE_FLASH_PACKET_LENGTH - 2; i++)
    {
        crc += bytes[i];
    }

    return crc;
}
