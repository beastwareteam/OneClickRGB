/**
 * Channel Settings Dialog - Configure per-channel color correction
 *
 * Standalone tool to configure RGB channels
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <commctrl.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include "channel_config.h"

#pragma comment(lib, "comctl32.lib")

#define WINDOW_WIDTH 700
#define WINDOW_HEIGHT 600

#define ID_BTN_SAVE 2001
#define ID_BTN_RESET 2002
#define ID_BTN_TEST 2003

// Slider base IDs (actual ID = base + channel * 10 + component)
#define ID_SLIDER_BASE 3000
#define ID_CHECK_BASE 4000

HWND g_hWnd = NULL;
ChannelManager g_channels;

// Current selected tab
int g_currentTab = 0;

// Control handles for current tab
struct ChannelControls {
    HWND hCheck;
    HWND hSliderR, hSliderG, hSliderB, hSliderBright;
    HWND hLabelR, hLabelG, hLabelB, hLabelBright;
};
std::vector<ChannelControls> g_controls;

HWND g_hTab = NULL;
HWND g_hStatus = NULL;

void UpdateSliderLabels(int index) {
    if (index >= (int)g_controls.size()) return;

    ChannelConfig* cfg = nullptr;
    if (g_currentTab == 0 && index < 8) cfg = &g_channels.aura_channels[index];
    else if (g_currentTab == 1 && index < 4) cfg = &g_channels.ram_modules[index];
    else if (g_currentTab == 2) {
        if (index == 0) cfg = &g_channels.steelseries;
        else if (index == 1) cfg = &g_channels.keyboard;
        else if (index == 2) cfg = &g_channels.edge;
    }

    if (!cfg) return;

    wchar_t buf[16];
    swprintf(buf, 16, L"%d%%", cfg->red_adjust);
    SetWindowTextW(g_controls[index].hLabelR, buf);
    swprintf(buf, 16, L"%d%%", cfg->green_adjust);
    SetWindowTextW(g_controls[index].hLabelG, buf);
    swprintf(buf, 16, L"%d%%", cfg->blue_adjust);
    SetWindowTextW(g_controls[index].hLabelB, buf);
    swprintf(buf, 16, L"%d%%", cfg->brightness);
    SetWindowTextW(g_controls[index].hLabelBright, buf);
}

void CreateChannelRow(HWND hWnd, int index, int y, const wchar_t* name, ChannelConfig* cfg) {
    ChannelControls ctrl = {};

    // Enable checkbox
    ctrl.hCheck = CreateWindowW(L"BUTTON", name,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, y, 120, 20, hWnd, (HMENU)(INT_PTR)(ID_CHECK_BASE + index), NULL, NULL);
    SendMessage(ctrl.hCheck, BM_SETCHECK, cfg->enabled ? BST_CHECKED : BST_UNCHECKED, 0);

    // Red slider
    CreateWindowW(L"STATIC", L"R:", WS_CHILD | WS_VISIBLE, 150, y + 2, 15, 18, hWnd, NULL, NULL, NULL);
    ctrl.hSliderR = CreateWindowW(TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        165, y, 100, 22, hWnd, (HMENU)(INT_PTR)(ID_SLIDER_BASE + index * 10 + 0), NULL, NULL);
    SendMessage(ctrl.hSliderR, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
    SendMessage(ctrl.hSliderR, TBM_SETPOS, TRUE, cfg->red_adjust);
    ctrl.hLabelR = CreateWindowW(L"STATIC", L"100%", WS_CHILD | WS_VISIBLE, 265, y + 2, 40, 18, hWnd, NULL, NULL, NULL);

    // Green slider
    CreateWindowW(L"STATIC", L"G:", WS_CHILD | WS_VISIBLE, 310, y + 2, 15, 18, hWnd, NULL, NULL, NULL);
    ctrl.hSliderG = CreateWindowW(TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        325, y, 100, 22, hWnd, (HMENU)(INT_PTR)(ID_SLIDER_BASE + index * 10 + 1), NULL, NULL);
    SendMessage(ctrl.hSliderG, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
    SendMessage(ctrl.hSliderG, TBM_SETPOS, TRUE, cfg->green_adjust);
    ctrl.hLabelG = CreateWindowW(L"STATIC", L"100%", WS_CHILD | WS_VISIBLE, 425, y + 2, 40, 18, hWnd, NULL, NULL, NULL);

    // Blue slider
    CreateWindowW(L"STATIC", L"B:", WS_CHILD | WS_VISIBLE, 470, y + 2, 15, 18, hWnd, NULL, NULL, NULL);
    ctrl.hSliderB = CreateWindowW(TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        485, y, 100, 22, hWnd, (HMENU)(INT_PTR)(ID_SLIDER_BASE + index * 10 + 2), NULL, NULL);
    SendMessage(ctrl.hSliderB, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
    SendMessage(ctrl.hSliderB, TBM_SETPOS, TRUE, cfg->blue_adjust);
    ctrl.hLabelB = CreateWindowW(L"STATIC", L"100%", WS_CHILD | WS_VISIBLE, 585, y + 2, 40, 18, hWnd, NULL, NULL, NULL);

    // Brightness slider (second row)
    CreateWindowW(L"STATIC", L"Brightness:", WS_CHILD | WS_VISIBLE, 150, y + 25, 70, 18, hWnd, NULL, NULL, NULL);
    ctrl.hSliderBright = CreateWindowW(TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        220, y + 23, 150, 22, hWnd, (HMENU)(INT_PTR)(ID_SLIDER_BASE + index * 10 + 3), NULL, NULL);
    SendMessage(ctrl.hSliderBright, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendMessage(ctrl.hSliderBright, TBM_SETPOS, TRUE, cfg->brightness);
    ctrl.hLabelBright = CreateWindowW(L"STATIC", L"100%", WS_CHILD | WS_VISIBLE, 375, y + 25, 40, 18, hWnd, NULL, NULL, NULL);

    g_controls.push_back(ctrl);
    UpdateSliderLabels(index);
}

void ClearControls() {
    for (auto& ctrl : g_controls) {
        DestroyWindow(ctrl.hCheck);
        DestroyWindow(ctrl.hSliderR);
        DestroyWindow(ctrl.hSliderG);
        DestroyWindow(ctrl.hSliderB);
        DestroyWindow(ctrl.hSliderBright);
        DestroyWindow(ctrl.hLabelR);
        DestroyWindow(ctrl.hLabelG);
        DestroyWindow(ctrl.hLabelB);
        DestroyWindow(ctrl.hLabelBright);
    }
    g_controls.clear();
}

void CreateTabContent(int tab) {
    ClearControls();

    int y = 80;

    if (tab == 0) {  // ASUS Aura
        for (int i = 0; i < 8; i++) {
            wchar_t name[32];
            swprintf(name, 32, L"Channel %d", i);
            CreateChannelRow(g_hWnd, i, y, name, &g_channels.aura_channels[i]);
            y += 55;
        }
    }
    else if (tab == 1) {  // RAM
        const wchar_t* names[] = {L"DIMM Slot 0", L"DIMM Slot 1", L"DIMM Slot 2", L"DIMM Slot 3"};
        for (int i = 0; i < 4; i++) {
            CreateChannelRow(g_hWnd, i, y, names[i], &g_channels.ram_modules[i]);
            y += 55;
        }
    }
    else if (tab == 2) {  // Other
        CreateChannelRow(g_hWnd, 0, y, L"SteelSeries Mouse", &g_channels.steelseries);
        y += 55;
        CreateChannelRow(g_hWnd, 1, y, L"Keyboard Keys", &g_channels.keyboard);
        y += 55;
        CreateChannelRow(g_hWnd, 2, y, L"Keyboard Edge", &g_channels.edge);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Title
        CreateWindowW(L"STATIC", L"Channel Settings - Color Correction",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 10, WINDOW_WIDTH - 40, 25, hWnd, NULL, NULL, NULL);

        // Tab control
        g_hTab = CreateWindowW(WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            10, 40, WINDOW_WIDTH - 40, 30, hWnd, NULL, NULL, NULL);

        TCITEMW tie = {};
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)L"ASUS Aura (8 Ch)";
        SendMessage(g_hTab, TCM_INSERTITEM, 0, (LPARAM)&tie);
        tie.pszText = (LPWSTR)L"RAM (4 Slots)";
        SendMessage(g_hTab, TCM_INSERTITEM, 1, (LPARAM)&tie);
        tie.pszText = (LPWSTR)L"Other Devices";
        SendMessage(g_hTab, TCM_INSERTITEM, 2, (LPARAM)&tie);

        // Buttons
        CreateWindowW(L"BUTTON", L"Save",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, WINDOW_HEIGHT - 80, 100, 30, hWnd, (HMENU)ID_BTN_SAVE, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Reset All",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            130, WINDOW_HEIGHT - 80, 100, 30, hWnd, (HMENU)ID_BTN_RESET, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Test Color",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            240, WINDOW_HEIGHT - 80, 100, 30, hWnd, (HMENU)ID_BTN_TEST, NULL, NULL);

        // Status
        g_hStatus = CreateWindowW(L"STATIC", L"Adjust color correction for each channel. 100% = no change.",
            WS_CHILD | WS_VISIBLE,
            20, WINDOW_HEIGHT - 45, WINDOW_WIDTH - 60, 20, hWnd, NULL, NULL, NULL);

        // Load settings
        g_channels.Load();

        // Create initial tab content
        g_hWnd = hWnd;
        CreateTabContent(0);
        break;
    }

    case WM_NOTIFY: {
        NMHDR* nmhdr = (NMHDR*)lParam;
        if (nmhdr->hwndFrom == g_hTab && nmhdr->code == TCN_SELCHANGE) {
            g_currentTab = TabCtrl_GetCurSel(g_hTab);
            CreateTabContent(g_currentTab);
        }
        break;
    }

    case WM_HSCROLL: {
        HWND slider = (HWND)lParam;
        int pos = SendMessage(slider, TBM_GETPOS, 0, 0);
        int id = GetDlgCtrlID(slider);

        if (id >= ID_SLIDER_BASE) {
            int index = (id - ID_SLIDER_BASE) / 10;
            int component = (id - ID_SLIDER_BASE) % 10;

            ChannelConfig* cfg = nullptr;
            if (g_currentTab == 0 && index < 8) cfg = &g_channels.aura_channels[index];
            else if (g_currentTab == 1 && index < 4) cfg = &g_channels.ram_modules[index];
            else if (g_currentTab == 2) {
                if (index == 0) cfg = &g_channels.steelseries;
                else if (index == 1) cfg = &g_channels.keyboard;
                else if (index == 2) cfg = &g_channels.edge;
            }

            if (cfg) {
                switch (component) {
                    case 0: cfg->red_adjust = pos; break;
                    case 1: cfg->green_adjust = pos; break;
                    case 2: cfg->blue_adjust = pos; break;
                    case 3: cfg->brightness = pos; break;
                }
                UpdateSliderLabels(index);
            }
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        if (id >= ID_CHECK_BASE && id < ID_CHECK_BASE + 100) {
            int index = id - ID_CHECK_BASE;
            bool checked = (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);

            ChannelConfig* cfg = nullptr;
            if (g_currentTab == 0 && index < 8) cfg = &g_channels.aura_channels[index];
            else if (g_currentTab == 1 && index < 4) cfg = &g_channels.ram_modules[index];
            else if (g_currentTab == 2) {
                if (index == 0) cfg = &g_channels.steelseries;
                else if (index == 1) cfg = &g_channels.keyboard;
                else if (index == 2) cfg = &g_channels.edge;
            }
            if (cfg) cfg->enabled = checked;
        }
        else if (id == ID_BTN_SAVE) {
            g_channels.Save();
            SetWindowTextW(g_hStatus, L"Settings saved!");
        }
        else if (id == ID_BTN_RESET) {
            // Reset all to defaults
            for (int i = 0; i < 8; i++) {
                g_channels.aura_channels[i].enabled = true;
                g_channels.aura_channels[i].red_adjust = 100;
                g_channels.aura_channels[i].green_adjust = 100;
                g_channels.aura_channels[i].blue_adjust = 100;
                g_channels.aura_channels[i].brightness = 100;
            }
            for (int i = 0; i < 4; i++) {
                g_channels.ram_modules[i].enabled = true;
                g_channels.ram_modules[i].red_adjust = 100;
                g_channels.ram_modules[i].green_adjust = 100;
                g_channels.ram_modules[i].blue_adjust = 100;
                g_channels.ram_modules[i].brightness = 100;
            }
            g_channels.steelseries = ChannelConfig();
            g_channels.keyboard = ChannelConfig();
            g_channels.edge = ChannelConfig();

            CreateTabContent(g_currentTab);
            SetWindowTextW(g_hStatus, L"All settings reset to defaults.");
        }
        else if (id == ID_BTN_TEST) {
            // Launch test with current settings
            STARTUPINFOW si = {sizeof(si)};
            PROCESS_INFORMATION pi;
            wchar_t cmd[] = L"oneclick_rgb.exe 0 34 255";
            CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            SetWindowTextW(g_hStatus, L"Testing with blue color...");
        }
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
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_TAB_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.lpszClassName = L"ChannelSettingsClass";
    RegisterClassW(&wc);

    g_hWnd = CreateWindowW(L"ChannelSettingsClass", L"OneClickRGB - Channel Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL);

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
