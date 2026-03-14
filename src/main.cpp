#include <cstdio>

#include "core/App.h"

int main() {
    std::printf("Mini Engine\n");

    App app;
    AppConfig config{};

    if (!app.Init(config)) {
        std::printf("[FATAL] Init failed\n");
        return 1;
    }

    app.Run();
    app.Shutdown();
    return 0;
}
