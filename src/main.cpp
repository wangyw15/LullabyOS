#include <WiFi.h>
#include <M5Cardputer.h>
#include <M5GFX.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#include "app.hpp"
#include "config.h"
#include "apps/launcher.hpp"
#include "apps/now_playing.hpp"
#include "apps/ark_radar.hpp"

void setup() {
    auto& display = M5Cardputer.Display;

    // initialize
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    Serial.begin(9600);
    Serial.println("[LullabyOS] Boot");

    // splash screen
    drawSplashScreen("LullabyOS");

    // set log font
    display.setTextSize(1);
    display.setTextColor(C_TEXT);
    display.setFont(UI_FONT);
    display.setCursor(0, 30);

    // register apps
    App::apps.push_back({"Launcher", Launcher::setup, Launcher::loop});
    App::apps.push_back({"ArkRadar", ArkRadar::setup, ArkRadar::loop});
    App::apps.push_back({"Now Playing", NowPlaying::setup, NowPlaying::loop});

    // log apps
    Serial.println("App list:");
    for (auto app: App::apps) {
        Serial.println(app.name);
    }

    delay(500);

    App::apps[App::selected].setup();
}

void loop() {
    App::apps[App::selected].loop();
}
