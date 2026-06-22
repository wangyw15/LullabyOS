#ifndef APP_HEADER
#define APP_HEADER

#include <vector>

namespace App {
    struct App {
        const char* name;
        void (*setup)();
        void (*loop)();
    };

    std::vector<App> apps;
    int selected = 0;

    void switchToLauncher() {
        selected = 0;
        apps[0].setup();
    }
}
#endif
