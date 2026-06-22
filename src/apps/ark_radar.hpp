#ifndef APP_SYNC_PROJECT
#define APP_SYNC_PROJECT

#include <M5Cardputer.h>
#include <M5GFX.h>
#include <NimBLEDevice.h>
#include <string>
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

namespace ArkRadar {
    struct ScannedDevice {
        std::string name;
        std::string addr;
        int rssi;
        uint32_t lastSeen;
    };

    std::vector<ScannedDevice> devices;
    NimBLEScan* scanner = nullptr;
    auto bleInitialized = false;
    auto isScanning = false;
    auto enableFilter = true;
    auto selectedIndex = 0;
    auto scrollOffset = 0;
    uint32_t lastScan = 0;
    size_t prevDeviceCount = 0;

    class ArkRadarDeviceCallbacks : public NimBLEScanCallbacks {
        void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
            auto name = advertisedDevice->getName();
            if (name.length() == 0) return;
            if (enableFilter && !name.starts_with(TARGET_PREFIX)) return;

            auto addr = advertisedDevice->getAddress().toString();
            auto rssi = advertisedDevice->getRSSI();
            if (devices.size() < MAX_DEVICES) {
                devices.push_back(ScannedDevice {
                    .name = name,
                    .addr = addr,
                    .rssi = rssi,
                    .lastSeen = millis(),
                });
            }

            Serial.print("[ArkRadar] Find radar: ");
            Serial.print(name.c_str());
            Serial.print(' ');
            Serial.print(addr.c_str());
            Serial.print(' ');
            Serial.println(rssi);
        }
    } scanCallbacks;

    void drawHeader() {
        auto& dsp = M5Cardputer.Display;
        auto height = 22;
        auto lineHeight = 2;

        dsp.fillRect(0, 0, dsp.width(), height + lineHeight, C_BG);
        dsp.fillRect(0, height, dsp.width(), lineHeight, C_ACCENT);

        dsp.setTextSize(1);
        dsp.setFont(UI_FONT);

        dsp.setTextColor(C_TEXT);
        dsp.setTextDatum(middle_left);
        dsp.drawString("ArkRadar", 6, height / 2);

        dsp.setTextDatum(middle_right);
        if (enableFilter) {
            dsp.setTextColor(C_ACCENT);
            dsp.drawString("RADAR ONLY", dsp.width() - 6, height / 2);
        }
        else {
            dsp.setTextColor(C_GRAY);
            dsp.drawString("ALL", dsp.width() - 6, height / 2);
        }
    }

    void drawStatusBar() {
        auto& dsp = M5Cardputer.Display;
        auto height = 18;
        auto y = dsp.height() - height;

        dsp.fillRect(0, y, dsp.width(), height, C_DARK_PANEL);

        dsp.setTextSize(1);
        dsp.setFont(UI_FONT);

        dsp.setTextColor(isScanning ? C_ACCENT : C_GRAY);
        dsp.setTextDatum(middle_left);
        dsp.drawString(isScanning ? "SCANNING..." : "IDLE", 6, y + height / 2);

        dsp.setTextColor(C_TEXT);
        dsp.setTextDatum(middle_right);
        dsp.drawString(std::string("FOUND: ").append(std::to_string(devices.size())).c_str(),
            dsp.width() - 6, y + height / 2);
    }

    void drawDeviceRow(int index, int row, bool selected) {
        auto& dsp = M5Cardputer.Display;
        const int rowY0 = 26;
        const int rowHeight = 18;
        const int listWidth = dsp.width();
        int y = rowY0 + row * rowHeight;

        dsp.fillRect(2, y, listWidth - 4, rowHeight - 1, selected ? C_ACCENT : C_DARK_PANEL);

        dsp.setTextColor(selected ? C_BG : C_TEXT);
        dsp.setTextSize(1);
        dsp.setFont(UI_FONT);

        dsp.setTextDatum(middle_left);
        dsp.drawString(std::to_string(index + 1).c_str(), 8, y + rowHeight / 2);

        auto name = devices[index].name;
        if (name.length() > 20) {
            name = name.substr(0, 20);
            name.append("...");
        }
        dsp.drawString(name.c_str(), 28, y + rowHeight / 2);

        dsp.setTextDatum(middle_right);
        dsp.drawString(String(devices[index].rssi) + " dBm", listWidth - 8, y + rowHeight / 2);
    }

    void drawDeviceList() {
        auto& dsp = M5Cardputer.Display;
        const int rowY0 = 26;
        const int rowHeight = 18;
        const int listWidth = dsp.width();

        if (devices.empty()) {
            dsp.fillRect(0, rowY0, listWidth, dsp.height() - rowY0 - 18, C_BG);
            dsp.setTextColor(C_GRAY);
            dsp.setTextDatum(middle_center);
            dsp.setFont(UI_FONT);
            dsp.drawString("[ press ENTER / S to scan ]", listWidth / 2, dsp.height() / 2);
            return;
        }

        if (selectedIndex < 0) selectedIndex = 0;
        if (selectedIndex >= devices.size()) selectedIndex = devices.size() - 1;
        if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
        if (selectedIndex >= scrollOffset + LIST_VISIBLE) scrollOffset = selectedIndex - LIST_VISIBLE + 1;

        auto endIdx = std::min((int)devices.size(), scrollOffset + LIST_VISIBLE);
        for (int i = scrollOffset; i < endIdx; ++i) {
            drawDeviceRow(i, i - scrollOffset, (i == selectedIndex));
        }

        // scroll bar
        int totalRows = devices.size();
        if (totalRows > LIST_VISIBLE) {
            dsp.fillRect(dsp.width() - 2, rowY0, 2, LIST_VISIBLE * rowHeight, C_BG);
            auto barH = (LIST_VISIBLE * rowHeight) / totalRows;
            auto barY = rowY0 + barH * selectedIndex;
            dsp.fillRect(dsp.width() - 2, barY, 2, barH, C_ACCENT);
        }
    }

    void clearListArea() {
        auto& dsp = M5Cardputer.Display;
        dsp.fillRect(0, 30, dsp.width(), dsp.height() - 30 - 18, C_BG);
    }

    void startScan() {
        if (isScanning) return;
        isScanning = true;
        lastScan = millis();
        devices.clear();
        scanner->start(SCAN_DURATION_SEC * 1000, false, false);
        drawStatusBar();
        Serial.println("[ArkRadar] Start scanning");
    }

    void stopScan() {
        if (!isScanning) return;
        if (scanner->isScanning()) scanner->stop();
        isScanning = false;
        drawStatusBar();
        Serial.println("[ArkRadar] Stop scanning");
    }

    void cleanUp() {
        stopScan();
        BLEDevice::deinit();
        scanner = nullptr;
        bleInitialized = false;
        devices.clear();
    }

    void setup() {
        drawSplashScreen("Ark Radar");

        if (!bleInitialized) {
            NimBLEDevice::init("");
            scanner = NimBLEDevice::getScan();
            scanner->setScanCallbacks(&scanCallbacks, false);
            scanner->setActiveScan(true);
            scanner->setMaxResults(0);
            bleInitialized = true;
            Serial.println("[ArkRadar] BLE initialized");
        }

        prevDeviceCount = (size_t)0;

        M5Cardputer.Display.fillScreen(C_BG);
        drawHeader();
        clearListArea();
        drawDeviceList();
        drawStatusBar();
    }

    void loop() {
        M5Cardputer.update();

        // auto-stop after scan duration
        if (isScanning && (millis() - lastScan > (uint32_t)SCAN_DURATION_SEC * 1000 + 500)) {
            stopScan();
        }

        if (M5Cardputer.BtnA.isPressed()) {
            cleanUp();
            App::switchToLauncher();
            return;
        }

        auto listUpdated = false;

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

                if (status.enter || keyPressed('s') || keyPressed('S')) {
                    if (isScanning) stopScan();
                    else startScan();
                }

                // Cardputer arrow keys map to ; , . / on the default layer
                if (keyPressed(';') || keyPressed(',')) {    // UP
                    if (selectedIndex > 0) {
                        selectedIndex--;
                        listUpdated = true;
                    }
                }
                if (keyPressed('.') || keyPressed('/')) {    // DOWN
                    if (selectedIndex < devices.size()) {
                        selectedIndex++;
                        listUpdated = true;
                    }
                }
                if (keyPressed('f')) {
                    enableFilter = !enableFilter;
                    drawHeader();
                }

                if (status.del || keyPressed('c') || keyPressed('C')) {
                    devices.clear();
                    selectedIndex = 0;
                    scrollOffset = 0;
                    prevDeviceCount = (size_t)0;
                }
            }
        }

        if (prevDeviceCount != devices.size()) {
            prevDeviceCount = devices.size();
            if (devices.size() <= scrollOffset + LIST_VISIBLE) {
                listUpdated = true;
            }
            drawStatusBar();
        }

        if (listUpdated) {
            clearListArea();
            drawDeviceList();
        }
        delay(50);
    }
}

#endif // APP_SYNC_PROJECT
