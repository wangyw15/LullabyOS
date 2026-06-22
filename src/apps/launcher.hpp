#ifndef APP_LAUNCHER
#define APP_LAUNCHER

#include "app.hpp"
#include "theme.h"
#include "util.hpp"

namespace Launcher {
    size_t appCount = 0;
    int selectedApp = 1; // 0 is launcher, so begin with 1

    void drawLauncher() {
        auto& dsp = M5Cardputer.Display;

        dsp.fillScreen(C_BG);
        dsp.fillRect(0, 26, dsp.width(), 2, C_ACCENT);

        dsp.setTextColor(C_TEXT);
        dsp.setTextSize(1);
        dsp.setTextDatum(top_left);
        dsp.setFont(UI_FONT);
        dsp.drawString("LAUNCHER", 6, 4);

        const int rowY0 = 36;
        const int rowH  = 26;
        for (int i = 1; i < appCount; i++) {  // 0 is launcher
            int y = rowY0 + (i - 1) * rowH;
            bool selected = (i == selectedApp);
            dsp.fillRoundRect(4, y, dsp.width() - 8, rowH - 2, 4, selected ? C_ACCENT : C_DARK_PANEL);
            dsp.setTextColor(selected ? C_BG : C_TEXT);
            dsp.setTextDatum(middle_left);
            dsp.setFont(UI_FONT);
            dsp.setTextSize(1);
            dsp.drawString(App::apps[i].name, 12, y + rowH / 2 - 1);
        }

        dsp.setTextColor(C_GRAY);
        dsp.setTextDatum(bottom_center);
        dsp.setFont(UI_FONT);
        dsp.drawString("ENTER=launch  Btn=back", dsp.width() / 2, dsp.height() - 4);

        Serial.print("[Launcher] Select: ");
        Serial.println(App::apps[selectedApp].name);
    }

    void setup() {
        appCount = App::apps.size();
        selectedApp = selectedApp == 0 ? 1 : selectedApp;
        drawLauncher();
    }

    void loop() {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            auto status = M5Cardputer.Keyboard.keysState();

            bool changed = false;
            if (keyPressed(status, ';') || keyPressed(status, ',')) {
                selectedApp--;
                if (selectedApp < 1) selectedApp = appCount - 1;
                changed = true;
            }
            if (keyPressed(status, '.') || keyPressed(status, '/')) {
                selectedApp++;
                if (selectedApp >= appCount) selectedApp = 1;
                changed = true;
            }
            if (changed) {
                drawLauncher();
            }
            if (status.enter) {
                Serial.print("[Launcher] select app: ");
                Serial.println(App::apps[selectedApp].name);

                App::selected = selectedApp;
                App::apps[App::selected].setup();
            }
        }
    }
}

#endif // APP_LAUNCHER
