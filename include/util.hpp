#ifndef UTIL_H
#define UTIL_H

#include <WiFi.h>
#include <M5Cardputer.h>
#include <M5GFX.h>

#include "theme.h"

inline bool keyPressed(const Keyboard_Class::KeysState& status, char c) {
    for (char k : status.word) {
        if (k == c) return true;
    }
    return false;
}

inline void drawSplashScreen(const char *title) {
    auto& display = M5Cardputer.Display;

    auto prevColor = display.getTextStyle().fore_rgb888;
    auto prevDatum = display.getTextStyle().datum;
    auto prevFont = display.getFont();
    auto prevSizeX = display.getTextSizeX();
    auto prevSizeY = display.getTextSizeY();
    auto prevCursorX = display.getCursorX();
    auto prevCursorY = display.getCursorY();

    display.fillScreen(C_BG);
    display.setTextColor(C_ACCENT);
    display.setTextDatum(middle_center);
    display.setFont(UI_FONT_BIG);
    display.setTextSize(2);
    display.drawString(title, display.width() / 2, display.fontHeight(UI_FONT_BIG) + 2);

    display.setTextColor(prevColor);
    display.setTextDatum(prevDatum);
    display.setFont(prevFont);
    display.setTextSize(prevSizeX, prevSizeY);
    display.setCursor(prevCursorX, prevCursorY);
}

inline void connectWiFi(const char *ssid, const char *password) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
    }
}

#endif // UTIL_H
