#include "core/App.h"
#include <cstdio>

int main() {
    std::printf("Mini Engine Day 1\n");

    App app;
    AppConfig cfg{};
    if (!app.Init(cfg)) {
        std::printf("[FATAL] Init failed\n");
        return 1;
    }

    app.Run();
    app.Shutdown();
    return 0;
}
