#ifndef APP_SYNC_PROJECT
#define APP_SYNC_PROJECT

#include <M5Cardputer.h>
#include <M5GFX.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <vector>
#include <algorithm>

#include "app.hpp"
#include "theme.h"
#include "util.hpp"

using namespace fonts;

// -----------------------------------------------------------------------------
// Config
// -----------------------------------------------------------------------------
static const char*   TARGET_PREFIX = "DEPRTS";
static const int     SCAN_DURATION_SEC = 5;
static const uint8_t MAX_DEVICES    = 50;
static const uint8_t LIST_VISIBLE = 5;

namespace SyncProject {
    struct ScannedDevice {
        String    name;
        String    addr;
        int       rssi;
        uint32_t lastSeen;
    };

    static std::vector<ScannedDevice> g_devices;
    static BLEScan* g_scanner = nullptr;
    static bool     g_bleInitialized = false;
    static bool     g_isScanning = false;
    static int      g_selected = 0;
    static int      g_scrollOffset = 0;
    static uint32_t g_lastScanTick = 0;

    static bool   g_prevScanning = false;
    static size_t g_prevDevCount = 0;
    static int    g_prevSelected = -1;
    static int    g_prevScrollOffset = -1;

    class ArkRadarDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
        void onResult(BLEAdvertisedDevice advertisedDevice) override {
            String name = String(advertisedDevice.getName().c_str());
            if (name.length() == 0) return;
            if (!name.startsWith(TARGET_PREFIX)) return;

            String addr = String(advertisedDevice.getAddress().toString().c_str());
            int rssi = advertisedDevice.getRSSI();

            bool updated = false;
            for (auto& d : g_devices) {
                if (d.addr.equals(addr)) {
                    d.rssi = rssi;
                    d.lastSeen = millis();
                    updated = true;
                    break;
                }
            }
            if (!updated && g_devices.size() < MAX_DEVICES) {
                ScannedDevice dev;
                dev.name = name;
                dev.addr = addr;
                dev.rssi = rssi;
                dev.lastSeen = millis();
                g_devices.push_back(dev);
            }
        }
    };
    static ArkRadarDeviceCallbacks* callback = nullptr;

    static void drawHeader() {
        auto& dsp = M5Cardputer.Display;
        dsp.fillRect(0, 0, dsp.width(), 28, C_BG);
        dsp.fillRect(0, 26, dsp.width(), 2, C_ACCENT);

        dsp.setTextColor(C_TEXT);
        dsp.setTextSize(1);
        dsp.setTextDatum(top_left);
        dsp.setFont(UI_FONT);
        dsp.drawString("SYNC PROJECT", 6, 4);

        dsp.setTextColor(C_ACCENT);
        dsp.setTextDatum(top_right);
        dsp.drawString("DEPRTS SCANNER", dsp.width() - 6, 4);
    }

    static void drawStatusBar() {
        auto& dsp = M5Cardputer.Display;
        int y = dsp.height() - 18;

        dsp.fillRect(0, y, dsp.width(), 18, C_DARK_PANEL);
        dsp.setTextSize(1);
        dsp.setTextDatum(top_left);
        dsp.setFont(UI_FONT);

        String status = g_isScanning ? "SCANNING..." : "IDLE";
        dsp.setTextColor(g_isScanning ? C_ACCENT : C_GRAY);
        dsp.drawString(status, 6, y + 4);

        dsp.setTextColor(C_TEXT);
        dsp.setTextDatum(top_right);
        dsp.drawString("FOUND: " + String(g_devices.size()), dsp.width() - 6, y + 4);
    }

    static void drawDeviceRow(int index, int screenRow, bool selected) {
        auto& dsp = M5Cardputer.Display;
        const int rowY0 = 30;
        const int rowH    = 20;
        const int listW = dsp.width();
        int y = rowY0 + screenRow * rowH;

        dsp.fillRect(2, y, listW - 4, rowH - 1, selected ? C_ACCENT : C_DARK_PANEL);

        dsp.setTextColor(selected ? C_BG : C_TEXT);
        dsp.setTextDatum(top_left);
        dsp.setTextSize(1);
        dsp.setFont(UI_FONT);
        dsp.drawString(String(index + 1), 8, y + 4);

        String name = g_devices[index].name;
        if (name.length() > 14) name = name.substring(0, 13) + "~";
        dsp.drawString(name, 28, y + 4);

        dsp.setTextDatum(top_right);
        dsp.drawString(String(g_devices[index].rssi) + " dBm", listW - 8, y + 4);
    }

    static void drawDeviceList() {
        auto& dsp = M5Cardputer.Display;
        const int rowY0 = 30;
        const int rowH    = 20;
        const int listW = dsp.width();

        if (g_devices.empty()) {
            dsp.fillRect(0, rowY0, listW, dsp.height() - rowY0 - 18, C_BG);
            dsp.setTextColor(C_GRAY);
            dsp.setTextDatum(middle_center);
            dsp.setFont(UI_FONT);
            dsp.drawString("[ press ENTER / S to scan ]", listW / 2, dsp.height() / 2);
            return;
        }

        if (g_selected < 0) g_selected = 0;
        if (g_selected >= (int)g_devices.size()) g_selected = g_devices.size() - 1;
        if (g_selected < g_scrollOffset) g_scrollOffset = g_selected;
        if (g_selected >= g_scrollOffset + LIST_VISIBLE) g_scrollOffset = g_selected - LIST_VISIBLE + 1;

        // clear scrollbar strip only
        dsp.fillRect(listW - 2, rowY0, 2, LIST_VISIBLE * rowH, C_BG);

        int endIdx = std::min((int)g_devices.size(), g_scrollOffset + LIST_VISIBLE);
        for (int i = g_scrollOffset; i < endIdx; ++i) {
            drawDeviceRow(i, i - g_scrollOffset, (i == g_selected));
        }

        int totalRows = g_devices.size();
        if (totalRows > LIST_VISIBLE) {
            int barH = (LIST_VISIBLE * rowH) * LIST_VISIBLE / totalRows;
            int barY = rowY0 + (g_scrollOffset * rowH) * LIST_VISIBLE / totalRows;
            barY = constrain(barY, rowY0, rowY0 + LIST_VISIBLE * rowH - barH);
            dsp.fillRect(listW - 2, barY, 2, barH, C_ACCENT);
        }
    }

    static void clearListArea() {
        auto& dsp = M5Cardputer.Display;
        dsp.fillRect(0, 30, dsp.width(), dsp.height() - 30 - 18, C_BG);
    }

    static void drawSplash() {
        auto& dsp = M5Cardputer.Display;
        dsp.fillScreen(C_BG);
        dsp.setTextColor(C_ACCENT);
        dsp.setTextDatum(middle_center);
        dsp.setFont(UI_FONT_BIG);
        dsp.setTextSize(2);
        dsp.drawString("DEPRTS", dsp.width() / 2, dsp.height() / 2 - 16);
        dsp.setTextSize(1);
        dsp.setTextColor(C_TEXT);
        dsp.setFont(UI_FONT);
        dsp.drawString("BLE BROADCAST SCANNER", dsp.width() / 2, dsp.height() / 2 + 12);
        dsp.setTextColor(C_GRAY);
        dsp.drawString("STYLE: SYNC PROJECT", dsp.width() / 2, dsp.height() / 2 + 28);
    }

    static void startScan() {
        if (g_isScanning) return;
        g_isScanning = true;
        g_lastScanTick = millis();
        g_scanner->start(SCAN_DURATION_SEC, nullptr, false);
    }

    static void stopScan() {
        if (!g_isScanning) return;
        if (g_scanner->isScanning()) g_scanner->stop();
        g_isScanning = false;
    }

    static void cleanUp() {
        stopScan();
        BLEDevice::deinit();
        g_scanner = nullptr;
        g_bleInitialized = false;

        g_devices.clear();
        delete callback;
        callback = nullptr;
    }

    // -----------------------------------------------------------------------------
    // Setup / Loop
    // -----------------------------------------------------------------------------
    void setup() {
        drawSplashScreen("Sync Project");

        if (callback == nullptr) {
            callback = new ArkRadarDeviceCallbacks();
        }

        if (!g_bleInitialized) {
            BLEDevice::init("");
            g_scanner = BLEDevice::getScan();
            g_scanner->setAdvertisedDeviceCallbacks(callback, false);
            g_scanner->setActiveScan(true);
            g_scanner->setInterval(100);
            g_scanner->setWindow(99);
            g_bleInitialized = true;
        }

        g_prevScanning = false;
        g_prevDevCount = (size_t)-1;
        g_prevSelected = -1;
        g_prevScrollOffset = -1;

        M5Cardputer.Display.fillScreen(C_BG);
        drawHeader();
        clearListArea();
        drawDeviceList();
        drawStatusBar();
    }

    void loop() {
        M5Cardputer.update();

        // auto-stop after scan duration
        if (g_isScanning && (millis() - g_lastScanTick > (uint32_t)SCAN_DURATION_SEC * 1000 + 500)) {
            stopScan();
        }

        if (M5Cardputer.BtnA.isPressed()) {
            cleanUp();
            App::switchToLauncher();
            return;
        }

        // keyboard handling
        if (M5Cardputer.Keyboard.isChange()) {
            if (M5Cardputer.Keyboard.isPressed()) {
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

                auto keyPressed = [&](char c) -> bool {
                    for (char k : status.word) {
                        if (k == c) return true;
                    }
                    return false;
                };

                if (status.enter || keyPressed('s') || keyPressed('S') || status.fn) {
                    if (g_isScanning) stopScan();
                    else startScan();
                    g_prevScanning = !g_isScanning;
                }

                // Cardputer arrow keys map to ; , . / on the default layer
                if (keyPressed(';') || keyPressed(',')) {    // UP
                    g_selected--;
                }
                if (keyPressed('.') || keyPressed('/')) {    // DOWN
                    g_selected++;
                }

                if (status.del || keyPressed('c') || keyPressed('C')) {
                    g_devices.clear();
                    g_selected = 0;
                    g_scrollOffset = 0;
                    clearListArea();
                    g_prevDevCount = (size_t)-1;
                }
            }
        }

        // status bar: only redraw on scan-state change
        if (g_isScanning != g_prevScanning) {
            drawStatusBar();
            g_prevScanning = g_isScanning;
        }

        // list: redraw only when data / scroll changed, otherwise just selection highlight
        if (g_devices.empty()) {
            if (g_prevDevCount != 0) {
                clearListArea();
                drawDeviceList();
                g_prevDevCount = 0;
                g_prevSelected = -1;
                g_prevScrollOffset = -1;
            }
        } else {
            // clamp and compute scroll first
            if (g_selected < 0) g_selected = 0;
            if (g_selected >= (int)g_devices.size()) g_selected = g_devices.size() - 1;
            if (g_selected < g_scrollOffset) g_scrollOffset = g_selected;
            if (g_selected >= g_scrollOffset + LIST_VISIBLE) g_scrollOffset = g_selected - LIST_VISIBLE + 1;

            bool countChanged = (g_devices.size() != g_prevDevCount);
            bool scrollChanged = (g_scrollOffset != g_prevScrollOffset);

            if (countChanged || scrollChanged) {
                clearListArea();
                drawDeviceList();
                g_prevDevCount = g_devices.size();
                g_prevSelected = g_selected;
                g_prevScrollOffset = g_scrollOffset;
            } else if (g_selected != g_prevSelected) {
                // only repaint previous and current rows (same scroll)
                if (g_prevSelected >= g_scrollOffset && g_prevSelected < g_scrollOffset + LIST_VISIBLE) {
                    drawDeviceRow(g_prevSelected, g_prevSelected - g_scrollOffset, false);
                }
                if (g_selected >= g_scrollOffset && g_selected < g_scrollOffset + LIST_VISIBLE) {
                    drawDeviceRow(g_selected, g_selected - g_scrollOffset, true);
                }
                g_prevSelected = g_selected;
            }
        }

        delay(50);
    }
}

#endif // APP_SYNC_PROJECT
