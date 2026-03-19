/**
 * OneClickRGB GUI - Modern Windows GUI for RGB Control
 *
 * Features:
 * - Color picker with hex input
 * - Device status display
 * - One-click apply to all devices
 * - Preset colors
 * - Individual device control
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "hidapi.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

// Window dimensions
#define WINDOW_WIDTH 500
#define WINDOW_HEIGHT 600

// Control IDs
#define ID_BTN_APPLY      1001
#define ID_BTN_PICK_COLOR 1002
#define ID_EDIT_HEX       1003
#define ID_SLIDER_R       1004
#define ID_SLIDER_G       1005
#define ID_SLIDER_B       1006
#define ID_STATIC_PREVIEW 1007
#define ID_BTN_PRESET1    1010
#define ID_BTN_PRESET2    1011
#define ID_BTN_PRESET3    1012
#define ID_BTN_PRESET4    1013
#define ID_BTN_PRESET5    1014
#define ID_BTN_PRESET6    1015
#define ID_CHECK_AURA     1020
#define ID_CHECK_MOUSE    1021
#define ID_CHECK_KEYBOARD 1022
#define ID_CHECK_RAM      1023
#define ID_CHECK_GPU      1024
#define ID_STATIC_STATUS  1030
#define ID_STATIC_R       1031
#define ID_STATIC_G       1032
#define ID_STATIC_B       1033

// Global variables
HWND g_hWnd = NULL;
HWND g_hPreview = NULL;
HWND g_hSliderR = NULL, g_hSliderG = NULL, g_hSliderB = NULL;
HWND g_hEditHex = NULL;
HWND g_hStaticR = NULL, g_hStaticG = NULL, g_hStaticB = NULL;
HWND g_hStatus = NULL;
HWND g_hCheckAura = NULL, g_hCheckMouse = NULL, g_hCheckKeyboard = NULL;
HWND g_hCheckRAM = NULL, g_hCheckGPU = NULL;

uint8_t g_red = 0, g_green = 34, g_blue = 255;
std::atomic<bool> g_applying{false};

// Forward declarations
void UpdatePreview();
void UpdateSliders();
void UpdateHexEdit();
void ApplyColors();
void SetStatus(const wchar_t* text);

//=============================================================================
// DEVICE CONTROL CODE (from oneclick_rgb.cpp)
//=============================================================================

namespace Devices {
    constexpr uint16_t ASUS_VID = 0x0B05;
    constexpr uint16_t ASUS_AURA_PID = 0x19AF;
    constexpr uint16_t ASUS_USAGE_PAGE = 0xFF72;
    constexpr uint16_t STEELSERIES_VID = 0x1038;
    constexpr uint16_t RIVAL_600_PID = 0x1724;
    constexpr uint16_t EVISION_VID = 0x3299;
    constexpr uint16_t EVISION_PID = 0x4E9F;
    constexpr uint16_t EVISION_USAGE_PAGE = 0xFF1C;
}

// EVision constants
constexpr uint8_t EVISION_V2_REPORT_ID = 4;
constexpr uint8_t EVISION_V2_PACKET_SIZE = 64;
constexpr uint8_t EVISION_V2_CMD_BEGIN_CONFIGURE = 0x01;
constexpr uint8_t EVISION_V2_CMD_END_CONFIGURE = 0x02;
constexpr uint8_t EVISION_V2_CMD_READ_CONFIG = 0x05;
constexpr uint8_t EVISION_V2_CMD_WRITE_CONFIG = 0x06;
constexpr uint8_t ENDORFY_MODE2_STATIC = 0x04;
constexpr uint8_t EVISION_V2_EDGE_OFFSET = 0x1a;

std::wstring g_statusLog;

void AppendStatus(const wchar_t* text) {
    g_statusLog += text;
    g_statusLog += L"\r\n";
    if (g_hStatus) {
        SetWindowTextW(g_hStatus, g_statusLog.c_str());
        SendMessage(g_hStatus, EM_SETSEL, g_statusLog.length(), g_statusLog.length());
        SendMessage(g_hStatus, EM_SCROLLCARET, 0, 0);
    }
}

void ClearStatus() {
    g_statusLog.clear();
    if (g_hStatus) SetWindowTextW(g_hStatus, L"");
}

//=== ASUS Aura ===
bool SetAsusAura(uint8_t r, uint8_t g, uint8_t b) {
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(Devices::ASUS_VID, Devices::ASUS_AURA_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == Devices::ASUS_USAGE_PAGE) {
            dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) {
        AppendStatus(L"[ASUS Aura] Not found");
        return false;
    }

    constexpr int MAX_LEDS_PER_PACKET = 19;
    for (int channel = 0; channel < 8; channel++) {
        int led_count = 60, offset = 0;
        while (offset < led_count) {
            int send_count = (led_count - offset > MAX_LEDS_PER_PACKET) ? MAX_LEDS_PER_PACKET : (led_count - offset);
            bool is_last = (offset + send_count >= led_count);
            uint8_t buf[64] = {0};
            buf[0] = 0xEC; buf[1] = 0x40;
            buf[2] = channel | (is_last ? 0x80 : 0x00);
            buf[3] = offset; buf[4] = send_count;
            for (int i = 0; i < send_count; i++) {
                buf[5 + i*3] = r; buf[5 + i*3 + 1] = g; buf[5 + i*3 + 2] = b;
            }
            hid_write(dev, buf, 64);
            Sleep(2);
            offset += send_count;
        }
    }
    hid_close(dev);
    AppendStatus(L"[ASUS Aura] 8 channels set");
    return true;
}

//=== SteelSeries ===
bool SetSteelSeries(uint8_t r, uint8_t g, uint8_t b) {
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(Devices::STEELSERIES_VID, Devices::RIVAL_600_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->interface_number == 0) {
            dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) {
        AppendStatus(L"[SteelSeries] Not found");
        return false;
    }

    for (int i = 0; i < 8; i++) {
        uint8_t pkt[8] = {0x1C, 0x27, 0x00, (uint8_t)(1 << i), r, g, b, 0};
        hid_write(dev, pkt, 7);
        Sleep(10);
    }
    uint8_t save[10] = {0x09};
    hid_write(dev, save, 9);
    hid_close(dev);
    AppendStatus(L"[SteelSeries] Rival 600 set");
    return true;
}

//=== EVision Keyboard ===
int EVisionQuery(hid_device* dev, uint8_t cmd, uint16_t offset, const uint8_t* idata, uint8_t size, uint8_t* odata) {
    uint8_t buffer[EVISION_V2_PACKET_SIZE];
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = EVISION_V2_REPORT_ID;
    buffer[3] = cmd; buffer[4] = size;
    buffer[5] = offset & 0xff; buffer[6] = (offset >> 8) & 0xff;
    if (idata && size > 0) memcpy(buffer + 8, idata, size);
    uint16_t chksum = 0;
    for (int i = 3; i < EVISION_V2_PACKET_SIZE; i++) chksum += buffer[i];
    buffer[1] = chksum & 0xff; buffer[2] = (chksum >> 8) & 0xff;
    if (hid_write(dev, buffer, sizeof(buffer)) < 0) return -1;
    int bytes_read, retries = 10;
    do { bytes_read = hid_read_timeout(dev, buffer, sizeof(buffer), 100); retries--; }
    while (bytes_read > 0 && buffer[0] != EVISION_V2_REPORT_ID && retries > 0);
    if (bytes_read != sizeof(buffer)) return -2;
    if (buffer[7] != 0) return -buffer[7];
    if (odata && buffer[4] > 0) memcpy(odata, buffer + 8, buffer[4]);
    return buffer[4];
}

int EVisionRead(hid_device* dev, uint16_t offset, uint8_t* data, uint16_t size) {
    uint8_t buffer[56]; uint16_t bytes_read = 0;
    while (bytes_read < size) {
        uint8_t chunk = (size - bytes_read > 56) ? 56 : (size - bytes_read);
        int res = EVisionQuery(dev, EVISION_V2_CMD_READ_CONFIG, offset + bytes_read, nullptr, chunk, buffer);
        if (res < 0) return res;
        memcpy(data + bytes_read, buffer, chunk);
        bytes_read += chunk;
    }
    return bytes_read;
}

int EVisionWrite(hid_device* dev, uint16_t offset, const uint8_t* data, uint16_t size) {
    uint16_t written = 0;
    while (written < size) {
        uint8_t chunk = (size - written > 56) ? 56 : (size - written);
        int res = EVisionQuery(dev, EVISION_V2_CMD_WRITE_CONFIG, offset + written, data + written, chunk, nullptr);
        if (res < 0) return res;
        written += chunk; Sleep(5);
    }
    return written;
}

bool SetEVisionKeyboard(uint8_t r, uint8_t g, uint8_t b) {
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(Devices::EVISION_VID, Devices::EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == Devices::EVISION_USAGE_PAGE) {
            dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) {
        AppendStatus(L"[EVision] Keyboard not found");
        return false;
    }

    EVisionQuery(dev, EVISION_V2_CMD_BEGIN_CONFIGURE, 0, nullptr, 0, nullptr);
    Sleep(20);

    uint8_t profile = 0;
    EVisionQuery(dev, EVISION_V2_CMD_READ_CONFIG, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;
    uint16_t profile_offset = profile * 0x40 + 0x01;

    // Keyboard keys
    uint8_t config[18];
    EVisionRead(dev, profile_offset, config, 18);
    config[0] = 0x06; if (config[1] == 0) config[1] = 0x04;
    config[4] = 0x00; config[5] = r; config[6] = g; config[7] = b;
    EVisionWrite(dev, profile_offset, config, 18);
    Sleep(10);

    // Edge LEDs (side lighting) - Endorfy protocol
    uint16_t edge_offset = profile_offset + EVISION_V2_EDGE_OFFSET;
    uint8_t edge[10] = {0};
    EVisionQuery(dev, EVISION_V2_CMD_READ_CONFIG, edge_offset, nullptr, 10, edge);

    // Set all edge parameters explicitly
    edge[0] = ENDORFY_MODE2_STATIC;  // Mode = Static (0x04 for Endorfy)
    edge[1] = 0x04;                   // Brightness = max
    edge[2] = 0x02;                   // Speed (keep default)
    edge[3] = 0x00;                   // Direction
    edge[4] = 0x00;                   // Random = off
    edge[5] = r;                      // Red
    edge[6] = g;                      // Green
    edge[7] = b;                      // Blue
    edge[8] = 0x00;                   // Reserved
    edge[9] = 0x01;                   // On = enabled

    int edge_res = EVisionQuery(dev, EVISION_V2_CMD_WRITE_CONFIG, edge_offset, edge, 10, nullptr);
    Sleep(20);

    // Unlock Windows key
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, EVISION_V2_CMD_WRITE_CONFIG, 0x14, unlock, 2, nullptr);

    EVisionQuery(dev, EVISION_V2_CMD_END_CONFIGURE, 0, nullptr, 0, nullptr);
    hid_close(dev);
    AppendStatus(L"[EVision] Keyboard + Edge LEDs set");
    return true;
}

//=== G.Skill RAM (PawnIO) ===
typedef HRESULT (__stdcall *pawnio_open_t)(PHANDLE);
typedef HRESULT (__stdcall *pawnio_load_t)(HANDLE, const UCHAR*, SIZE_T);
typedef HRESULT (__stdcall *pawnio_execute_t)(HANDLE, PCSTR, const ULONG64*, SIZE_T, PULONG64, SIZE_T, PSIZE_T);
typedef HRESULT (__stdcall *pawnio_close_t)(HANDLE);

union i2c_smbus_data { uint8_t byte; uint16_t word; uint8_t block[34]; };

bool SetGSkillRAM(uint8_t r, uint8_t g, uint8_t b) {
    HMODULE dll = LoadLibraryA("PawnIOLib.dll");
    if (!dll) {
        AppendStatus(L"[G.Skill RAM] PawnIOLib.dll not found");
        return false;
    }

    auto p_open = (pawnio_open_t)GetProcAddress(dll, "pawnio_open");
    auto p_load = (pawnio_load_t)GetProcAddress(dll, "pawnio_load");
    auto p_exec = (pawnio_execute_t)GetProcAddress(dll, "pawnio_execute");
    auto p_close = (pawnio_close_t)GetProcAddress(dll, "pawnio_close");

    if (!p_open || !p_load || !p_exec || !p_close) {
        FreeLibrary(dll);
        return false;
    }

    HANDLE handle;
    if (p_open(&handle) != S_OK) {
        AppendStatus(L"[G.Skill RAM] PawnIO driver not running");
        FreeLibrary(dll);
        return false;
    }

    // Load SMBus module
    HANDLE hFile = CreateFileA("SmbusI801.bin", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        p_close(handle);
        FreeLibrary(dll);
        AppendStatus(L"[G.Skill RAM] SmbusI801.bin not found");
        return false;
    }

    DWORD size = GetFileSize(hFile, NULL);
    std::vector<uint8_t> blob(size);
    ReadFile(hFile, blob.data(), size, &size, NULL);
    CloseHandle(hFile);

    if (p_load(handle, blob.data(), blob.size()) != S_OK) {
        p_close(handle);
        FreeLibrary(dll);
        return false;
    }

    // Helper lambdas for SMBus
    auto smbus_xfer = [&](uint8_t addr, char rw, uint8_t cmd, int sz, i2c_smbus_data* data) -> int {
        ULONG64 in[9] = {addr, (ULONG64)rw, cmd, (ULONG64)sz};
        if (data) memcpy(&in[4], data, sizeof(i2c_smbus_data));
        ULONG64 out[5] = {0}; SIZE_T ret_sz;
        HRESULT hr = p_exec(handle, "ioctl_smbus_xfer", in, 9, out, 5, &ret_sz);
        if (data) memcpy(data, &out[0], sizeof(i2c_smbus_data));
        return hr == S_OK ? 0 : -1;
    };

    auto read_byte = [&](uint8_t addr) -> int {
        i2c_smbus_data d; return smbus_xfer(addr, 1, 0, 1, &d) < 0 ? -1 : d.byte;
    };

    auto write_word = [&](uint8_t addr, uint8_t cmd, uint16_t val) {
        i2c_smbus_data d; d.word = val; smbus_xfer(addr, 0, cmd, 3, &d);
    };

    auto write_byte = [&](uint8_t addr, uint8_t cmd, uint8_t val) {
        i2c_smbus_data d; d.byte = val; smbus_xfer(addr, 0, cmd, 2, &d);
    };

    auto read_byte_data = [&](uint8_t addr, uint8_t cmd) -> int {
        i2c_smbus_data d; return smbus_xfer(addr, 1, cmd, 2, &d) < 0 ? -1 : d.byte;
    };

    auto ene_write = [&](uint8_t addr, uint16_t reg, uint8_t val) {
        uint16_t sw = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF);
        write_word(addr, 0x00, sw); Sleep(1);
        write_byte(addr, 0x01, val); Sleep(1);
    };

    auto ene_read = [&](uint8_t addr, uint16_t reg) -> uint8_t {
        uint16_t sw = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF);
        write_word(addr, 0x00, sw); Sleep(1);
        int res = read_byte_data(addr, 0x81);
        return res >= 0 ? (uint8_t)res : 0;
    };

    int found = 0;
    uint8_t addrs[] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};

    for (uint8_t addr : addrs) {
        if (read_byte(addr) < 0) continue;

        // Read device name
        char name[17] = {0};
        for (int i = 0; i < 16; i++) name[i] = ene_read(addr, 0x1000 + i);

        if (strstr(name, "AUDA") || strstr(name, "DIMM") || strstr(name, "Trident")) {
            // Get LED count
            uint8_t led_count = ene_read(addr, 0x1C02);
            if (led_count == 0 || led_count > 20) led_count = 8;

            // Enable direct mode
            ene_write(addr, 0x8020, 0x01); Sleep(5);

            // Set colors (RBG order for ENE)
            for (int i = 0; i < led_count; i++) {
                uint16_t reg = 0x8100 + (i * 3);
                ene_write(addr, reg + 0, r);
                ene_write(addr, reg + 1, b);  // ENE uses RBG
                ene_write(addr, reg + 2, g);
            }
            Sleep(5);

            // Apply
            ene_write(addr, 0x80A0, 0x01);
            found++;
        }
    }

    p_close(handle);
    FreeLibrary(dll);

    if (found > 0) {
        wchar_t buf[64];
        swprintf(buf, 64, L"[G.Skill RAM] %d module(s) set", found);
        AppendStatus(buf);
        return true;
    } else {
        AppendStatus(L"[G.Skill RAM] No compatible modules found");
        return false;
    }
}

//=============================================================================
// GUI FUNCTIONS
//=============================================================================

void UpdatePreview() {
    if (g_hPreview) {
        InvalidateRect(g_hPreview, NULL, TRUE);
    }
}

void UpdateSliders() {
    if (g_hSliderR) SendMessage(g_hSliderR, TBM_SETPOS, TRUE, g_red);
    if (g_hSliderG) SendMessage(g_hSliderG, TBM_SETPOS, TRUE, g_green);
    if (g_hSliderB) SendMessage(g_hSliderB, TBM_SETPOS, TRUE, g_blue);

    wchar_t buf[8];
    swprintf(buf, 8, L"%d", g_red);   SetWindowTextW(g_hStaticR, buf);
    swprintf(buf, 8, L"%d", g_green); SetWindowTextW(g_hStaticG, buf);
    swprintf(buf, 8, L"%d", g_blue);  SetWindowTextW(g_hStaticB, buf);
}

void UpdateHexEdit() {
    wchar_t hex[8];
    swprintf(hex, 8, L"#%02X%02X%02X", g_red, g_green, g_blue);
    SetWindowTextW(g_hEditHex, hex);
}

void ParseHexColor(const wchar_t* hex) {
    if (hex[0] == L'#') hex++;
    if (wcslen(hex) >= 6) {
        wchar_t r[3] = {hex[0], hex[1], 0};
        wchar_t g[3] = {hex[2], hex[3], 0};
        wchar_t b[3] = {hex[4], hex[5], 0};
        g_red   = (uint8_t)wcstol(r, NULL, 16);
        g_green = (uint8_t)wcstol(g, NULL, 16);
        g_blue  = (uint8_t)wcstol(b, NULL, 16);
    }
}

void SetPresetColor(uint8_t r, uint8_t g, uint8_t b) {
    g_red = r; g_green = g; g_blue = b;
    UpdatePreview();
    UpdateSliders();
    UpdateHexEdit();
}

void ApplyColors() {
    if (g_applying.exchange(true)) return;

    ClearStatus();
    AppendStatus(L"Applying colors...");
    AppendStatus(L"");

    hid_init();

    bool aura = (SendMessage(g_hCheckAura, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool mouse = (SendMessage(g_hCheckMouse, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool kb = (SendMessage(g_hCheckKeyboard, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool ram = (SendMessage(g_hCheckRAM, BM_GETCHECK, 0, 0) == BST_CHECKED);

    if (aura) SetAsusAura(g_red, g_green, g_blue);
    if (mouse) SetSteelSeries(g_red, g_green, g_blue);
    if (kb) SetEVisionKeyboard(g_red, g_green, g_blue);
    if (ram) SetGSkillRAM(g_red, g_green, g_blue);

    hid_exit();

    AppendStatus(L"");
    AppendStatus(L"Done!");

    g_applying = false;
}

void PickColor() {
    CHOOSECOLOR cc = {0};
    static COLORREF customColors[16] = {0};

    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = g_hWnd;
    cc.lpCustColors = customColors;
    cc.rgbResult = RGB(g_red, g_green, g_blue);
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColor(&cc)) {
        g_red = GetRValue(cc.rgbResult);
        g_green = GetGValue(cc.rgbResult);
        g_blue = GetBValue(cc.rgbResult);
        UpdatePreview();
        UpdateSliders();
        UpdateHexEdit();
    }
}

LRESULT CALLBACK PreviewProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        HBRUSH brush = CreateSolidBrush(RGB(g_red, g_green, g_blue));
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);

        // Border
        FrameRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

        EndPaint(hWnd, &ps);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Title
        CreateWindowW(L"STATIC", L"OneClickRGB",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 10, WINDOW_WIDTH - 40, 30, hWnd, NULL, NULL, NULL);

        // Color preview
        WNDCLASS wc = {0};
        wc.lpfnWndProc = PreviewProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"ColorPreview";
        RegisterClass(&wc);

        g_hPreview = CreateWindowW(L"ColorPreview", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            20, 50, 200, 100, hWnd, (HMENU)ID_STATIC_PREVIEW, NULL, NULL);

        // Hex input
        CreateWindowW(L"STATIC", L"Hex:", WS_CHILD | WS_VISIBLE,
            240, 50, 40, 20, hWnd, NULL, NULL, NULL);
        g_hEditHex = CreateWindowW(L"EDIT", L"#0022FF",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_UPPERCASE,
            280, 48, 80, 24, hWnd, (HMENU)ID_EDIT_HEX, NULL, NULL);

        // Pick color button
        CreateWindowW(L"BUTTON", L"Pick Color",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            380, 48, 90, 26, hWnd, (HMENU)ID_BTN_PICK_COLOR, NULL, NULL);

        // RGB Sliders
        int slider_y = 170;

        CreateWindowW(L"STATIC", L"Red:", WS_CHILD | WS_VISIBLE,
            20, slider_y, 40, 20, hWnd, NULL, NULL, NULL);
        g_hSliderR = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ,
            60, slider_y, 300, 30, hWnd, (HMENU)ID_SLIDER_R, NULL, NULL);
        SendMessage(g_hSliderR, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
        g_hStaticR = CreateWindowW(L"STATIC", L"0", WS_CHILD | WS_VISIBLE,
            370, slider_y, 40, 20, hWnd, (HMENU)ID_STATIC_R, NULL, NULL);

        slider_y += 35;
        CreateWindowW(L"STATIC", L"Green:", WS_CHILD | WS_VISIBLE,
            20, slider_y, 40, 20, hWnd, NULL, NULL, NULL);
        g_hSliderG = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ,
            60, slider_y, 300, 30, hWnd, (HMENU)ID_SLIDER_G, NULL, NULL);
        SendMessage(g_hSliderG, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
        g_hStaticG = CreateWindowW(L"STATIC", L"34", WS_CHILD | WS_VISIBLE,
            370, slider_y, 40, 20, hWnd, (HMENU)ID_STATIC_G, NULL, NULL);

        slider_y += 35;
        CreateWindowW(L"STATIC", L"Blue:", WS_CHILD | WS_VISIBLE,
            20, slider_y, 40, 20, hWnd, NULL, NULL, NULL);
        g_hSliderB = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ,
            60, slider_y, 300, 30, hWnd, (HMENU)ID_SLIDER_B, NULL, NULL);
        SendMessage(g_hSliderB, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
        g_hStaticB = CreateWindowW(L"STATIC", L"255", WS_CHILD | WS_VISIBLE,
            370, slider_y, 40, 20, hWnd, (HMENU)ID_STATIC_B, NULL, NULL);

        // Preset colors
        int preset_y = 290;
        CreateWindowW(L"STATIC", L"Presets:", WS_CHILD | WS_VISIBLE,
            20, preset_y, 60, 20, hWnd, NULL, NULL, NULL);

        int px = 90;
        // Blue (default)
        CreateWindowW(L"BUTTON", L"Blue", WS_CHILD | WS_VISIBLE,
            px, preset_y - 2, 55, 24, hWnd, (HMENU)ID_BTN_PRESET1, NULL, NULL);
        px += 60;
        // Red
        CreateWindowW(L"BUTTON", L"Red", WS_CHILD | WS_VISIBLE,
            px, preset_y - 2, 55, 24, hWnd, (HMENU)ID_BTN_PRESET2, NULL, NULL);
        px += 60;
        // Green
        CreateWindowW(L"BUTTON", L"Green", WS_CHILD | WS_VISIBLE,
            px, preset_y - 2, 55, 24, hWnd, (HMENU)ID_BTN_PRESET3, NULL, NULL);
        px += 60;
        // Purple
        CreateWindowW(L"BUTTON", L"Purple", WS_CHILD | WS_VISIBLE,
            px, preset_y - 2, 55, 24, hWnd, (HMENU)ID_BTN_PRESET4, NULL, NULL);
        px += 60;
        // White
        CreateWindowW(L"BUTTON", L"White", WS_CHILD | WS_VISIBLE,
            px, preset_y - 2, 55, 24, hWnd, (HMENU)ID_BTN_PRESET5, NULL, NULL);
        px += 60;
        // Off
        CreateWindowW(L"BUTTON", L"Off", WS_CHILD | WS_VISIBLE,
            px, preset_y - 2, 45, 24, hWnd, (HMENU)ID_BTN_PRESET6, NULL, NULL);

        // Device checkboxes
        int check_y = 330;
        CreateWindowW(L"STATIC", L"Devices:", WS_CHILD | WS_VISIBLE,
            20, check_y, 60, 20, hWnd, NULL, NULL, NULL);

        g_hCheckAura = CreateWindowW(L"BUTTON", L"ASUS Aura",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            90, check_y, 100, 20, hWnd, (HMENU)ID_CHECK_AURA, NULL, NULL);
        SendMessage(g_hCheckAura, BM_SETCHECK, BST_CHECKED, 0);

        g_hCheckMouse = CreateWindowW(L"BUTTON", L"SteelSeries Mouse",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            200, check_y, 130, 20, hWnd, (HMENU)ID_CHECK_MOUSE, NULL, NULL);
        SendMessage(g_hCheckMouse, BM_SETCHECK, BST_CHECKED, 0);

        g_hCheckKeyboard = CreateWindowW(L"BUTTON", L"EVision Keyboard",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            340, check_y, 130, 20, hWnd, (HMENU)ID_CHECK_KEYBOARD, NULL, NULL);
        SendMessage(g_hCheckKeyboard, BM_SETCHECK, BST_CHECKED, 0);

        check_y += 25;
        g_hCheckRAM = CreateWindowW(L"BUTTON", L"G.Skill RAM",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            90, check_y, 100, 20, hWnd, (HMENU)ID_CHECK_RAM, NULL, NULL);
        SendMessage(g_hCheckRAM, BM_SETCHECK, BST_CHECKED, 0);

        g_hCheckGPU = CreateWindowW(L"BUTTON", L"GPU (N/A)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_DISABLED,
            200, check_y, 100, 20, hWnd, (HMENU)ID_CHECK_GPU, NULL, NULL);

        // Apply button
        CreateWindowW(L"BUTTON", L"APPLY TO ALL",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
            20, 400, WINDOW_WIDTH - 60, 40, hWnd, (HMENU)ID_BTN_APPLY, NULL, NULL);

        // Status log
        CreateWindowW(L"STATIC", L"Status:", WS_CHILD | WS_VISIBLE,
            20, 450, 60, 20, hWnd, NULL, NULL, NULL);

        g_hStatus = CreateWindowW(L"EDIT", L"Ready",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
            20, 470, WINDOW_WIDTH - 60, 100, hWnd, (HMENU)ID_STATIC_STATUS, NULL, NULL);

        // Initialize
        UpdateSliders();
        UpdateHexEdit();
        break;
    }

    case WM_HSCROLL: {
        HWND slider = (HWND)lParam;
        int pos = SendMessage(slider, TBM_GETPOS, 0, 0);

        if (slider == g_hSliderR) g_red = pos;
        else if (slider == g_hSliderG) g_green = pos;
        else if (slider == g_hSliderB) g_blue = pos;

        UpdatePreview();
        UpdateHexEdit();

        wchar_t buf[8];
        swprintf(buf, 8, L"%d", pos);
        if (slider == g_hSliderR) SetWindowTextW(g_hStaticR, buf);
        else if (slider == g_hSliderG) SetWindowTextW(g_hStaticG, buf);
        else if (slider == g_hSliderB) SetWindowTextW(g_hStaticB, buf);
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == ID_BTN_APPLY) {
            std::thread(ApplyColors).detach();
        }
        else if (id == ID_BTN_PICK_COLOR) {
            PickColor();
        }
        else if (id == ID_EDIT_HEX && code == EN_KILLFOCUS) {
            wchar_t hex[16];
            GetWindowTextW(g_hEditHex, hex, 16);
            ParseHexColor(hex);
            UpdatePreview();
            UpdateSliders();
        }
        else if (id == ID_BTN_PRESET1) SetPresetColor(0, 34, 255);    // Blue
        else if (id == ID_BTN_PRESET2) SetPresetColor(255, 0, 0);     // Red
        else if (id == ID_BTN_PRESET3) SetPresetColor(0, 255, 0);     // Green
        else if (id == ID_BTN_PRESET4) SetPresetColor(128, 0, 255);   // Purple
        else if (id == ID_BTN_PRESET5) SetPresetColor(255, 255, 255); // White
        else if (id == ID_BTN_PRESET6) SetPresetColor(0, 0, 0);       // Off
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Init common controls
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc);

    // Register window class
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.lpszClassName = L"OneClickRGBClass";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassW(&wc);

    // Create window
    g_hWnd = CreateWindowW(L"OneClickRGBClass", L"OneClickRGB - RGB Control",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL);

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
