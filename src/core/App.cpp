#include "core/App.h"
#include "platform/SdlPlatform.h"
#include <cstdio>

static SdlPlatform g_platform;

bool App::Init(const AppConfig& cfg) {
    if (!g_platform.Init(cfg.windowWidth, cfg.windowHeight, cfg.title)) {
        std::printf("[ERROR] Platform init failed\n");
        return false;
    }
    m_running = true;
    return true;
}

void App::Run() {
    std::printf("[INFO] Entering main loop\n");

    while (m_running) {
        SdlFrameData frame{};
        if (!g_platform.Pump(frame)) {
            m_running = false;
            break;
        }

        // Simple “render something” test: clear + draw a moving rect
        g_platform.BeginFrame();

        // frame.dtSeconds is validated by tests below
        g_platform.DrawTestRect(frame.timeSeconds);

        g_platform.EndFrame();
    }
}

void App::Shutdown() {
    g_platform.Shutdown();
    std::printf("[INFO] Clean shutdown\n");
}
