#pragma once

#include <WiFi.h>
#include <M5Cardputer.h>
#include <M5GFX.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#include "include/config.h"
#include "include/shared.hpp"
#include "include/app.hpp"

#define LYRIC_PLACEHOLDER ""

namespace NowPlaying {
    HTTPClient http;
    WebSocketsClient ws;

    auto nowPlayingFont = &fonts::lgfxJapanMincho_16;

    u_int8_t coverBuffer[128 * 1024];
    size_t coverBufferSize = 0;

    char author[32] = {0};
    char album[32] = {0};
    char title[32] = {0};
    char lyric[128] = {0};
    char coverURL[128] = {0};

    int downloadCover() {
        Serial.println("[Now Playing] Start download cover");

        coverBufferSize = 0;

        auto cover = std::string(coverURL);
        if (cover.empty()) {
            return -1;
        }
        if (cover.substr(0, 5).compare("https") == 0) {
            cover.replace(0, 5, "http");
        }
        Serial.print("[Now Playing] Cover URL: ");
        Serial.println(cover.c_str());

        http.begin(cover.c_str());
        auto statusCode = http.GET();

        Serial.print("[Now Playing] Status code: ");
        Serial.println(statusCode);

        if (statusCode == HTTP_CODE_OK) {
            auto size = http.getSize();
            auto stream = http.getStreamPtr();
            size_t total = 0;
            const size_t capacity = sizeof(coverBuffer);

            while (http.connected() && (size > 0 || size == -1)) {
                auto available = stream->available();
                if (available) {
                    size_t room = capacity - total;
                    if (room == 0) {
                        break;
                    }
                    size_t toRead = (available > room) ? room : available;
                    size_t read = stream->readBytes(coverBuffer + total, toRead);
                    total += read;
                    if (size > 0) {
                        size -= static_cast<int>(read);
                    }
                }
                delay(1);
            }
            coverBufferSize = total;
        }

        Serial.print("[Now Playing] Cover size: ");
        Serial.println(coverBufferSize);

        http.end();
        cover.clear();
        return statusCode;
    }

    void drawText(
        const char *text,
        int16_t x,
        int16_t y,
        int16_t maxWidth,
        float textSize,
        uint16_t color
    ) {
        auto& display = M5Cardputer.Display;
        display.setTextSize(textSize);
        display.setTextColor(color);

        auto textWidth = display.textWidth(text, nowPlayingFont);

        if (textWidth <= maxWidth) {
            display.setCursor(x, y);
            display.print(text);
        } else {
            char buf[32] = {0};
            strncpy(buf, text, sizeof(buf) - 1);
            const char *ellipsis = "...";
            auto ellipsisWidth = display.textWidth(ellipsis, nowPlayingFont);
            auto availWidth = maxWidth - ellipsisWidth;

            int len = strlen(buf);
            while (len > 0 && display.textWidth(buf, nowPlayingFont) > availWidth) {
                buf[--len] = '\0';
            }

            display.setCursor(x, y);
            display.print(buf);
            display.print(ellipsis);
        }
    }

    void drawCoverImage(int16_t x, int16_t y) {
        const int16_t coverSize = 90;
        if (coverBufferSize == 0) {
            return;
        }

        bool drawn = M5Cardputer.Display.drawJpg(
            coverBuffer, coverBufferSize,
            x, y, coverSize, coverSize,
            0, 0, 0.0f, 0.0f,
            lgfx::datum_t::middle_center
        );
        if (!drawn) {
            drawn = M5Cardputer.Display.drawPng(
                coverBuffer, coverBufferSize,
                x, y, coverSize, coverSize,
                0, 0, 0.0f, 0.0f,
                lgfx::datum_t::middle_center
            );
        }
    }

    void drawSongInfo() {
        auto& display = M5Cardputer.Display;

        const int16_t coverSize = 90;
        const int16_t coverX = 8;
        const int16_t coverY = (display.height() - coverSize) / 2;
        const int16_t textX = coverX + coverSize + 10;
        const int16_t maxWidth = display.width() - textX - 8;

        display.clear();
        display.setTextFont(nowPlayingFont);
        display.fillRect(coverX, coverY, coverSize, coverSize, 0x0A2A0A);

        drawText(title, textX, coverY, maxWidth, 1.3, 0x00FF00);
        drawText(author, textX, coverY + 30, maxWidth, 1.0, 0xFFFFFF);
        drawText(lyric, textX, coverY + 55, maxWidth, 1.0, 0xFFFFFF);

        downloadCover();
        drawCoverImage(coverX, coverY);
    }

    void wsEvent(WStype_t type, uint8_t * payload, size_t length) {
        if (type == WStype_t::WStype_CONNECTED) {
            M5Cardputer.Display.println("Connected to Now Playing");
            Serial.println("[Now Playing] Connected to Now Playing");
        }
        else if (type == WStype_t::WStype_TEXT) {
            JsonDocument doc;
            try {
                deserializeJson(doc, payload, length);
                if (strcmp(doc["event"].as<const char *>(), "Track") == 0) {
                    auto _author = doc["data"]["author"].as<const char *>();
                    auto _album = doc["data"]["album"].as<const char *>();
                    auto _title = doc["data"]["title"].as<const char *>();
                    auto _cover = doc["data"]["cover"].as<const char *>();

                    strncpy(author, _author, sizeof(author) - 1);
                    strncpy(album, _album, sizeof(album) - 1);
                    strncpy(title, _title, sizeof(title) - 1);
                    strncpy(coverURL, _cover, sizeof(coverURL) - 1);
                    strncpy(lyric, LYRIC_PLACEHOLDER, sizeof(lyric) - 1);

                    Serial.println("[Now Playing] New track");
                    Serial.print("[Now Playing] Author: ");
                    Serial.println(_author);
                    Serial.print("[Now Playing] Album: ");
                    Serial.println(_album);
                    Serial.print("[Now Playing] Title: ");
                    Serial.println(_title);
                    Serial.print("[Now Playing] Cover: ");
                    Serial.println(_cover);

                    drawSongInfo();
                }
            }
            catch (DeserializationError e) {
                M5Cardputer.Display.println(e.c_str());
            }
            doc.clear();
        }
    }

    void cleanUp() {
        ws.disconnect();
        WiFi.disconnect();
    }

    void setup() {
        auto& display = M5Cardputer.Display;

        drawSplashScreen("Now Playing");

        display.setCursor(0, 0);
        display.setTextDatum(top_left);
        display.setTextColor(C_TEXT);
        display.setFont(UI_FONT);
        display.setTextSize(1);

        display.print("Connecting to WiFi ");
        display.println(WIFI_SSID);
        Serial.print("[Now Playing] Connecting to WiFi ");
        Serial.println(WIFI_SSID);

        connectWiFi(WIFI_SSID, WIFI_PASSWORD);

        display.println("WiFi connected!");
        Serial.println("WiFi connected!");

        display.println("Connecting to Now Playing...");
        display.print("ws://");
        display.print(NOW_PLAYING_HOST);
        display.print(":");
        display.print(NOW_PLAYING_PORT);
        display.println("/api/ws/lyric");
        Serial.println("[Now Playing] Connecting to Now Playing...");
        Serial.print("[Now Playing] ws://");
        Serial.print(NOW_PLAYING_HOST);
        Serial.print(":");
        Serial.print(NOW_PLAYING_PORT);
        Serial.println("/api/ws/lyric");

        ws.begin(NOW_PLAYING_HOST, NOW_PLAYING_PORT, "/api/ws/lyric");
        ws.onEvent(wsEvent);
    }

    void loop() {
        M5Cardputer.update();
        if (M5Cardputer.BtnA.isPressed()) {
            Serial.println("[Now Playing] return to launcher");
            cleanUp();
            App::switchToLauncher();
            return;
        }

        ws.loop();
    }
};
