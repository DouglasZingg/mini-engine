#include "core/App.h"
#include "game/Game.h"
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

    Game game;
    if (!game.Init(g_platform)) {
        std::printf("[ERROR] Game init failed\n");
        return;
    }

    // Fixed timestep simulation parameters
    const float fixedDt = 1.0f / 60.0f;
    float accumulator = 0.0f;

    while (m_running) {
        // ---- Poll platform ----
        SdlFrameData frame{};
        if (!g_platform.Pump(frame)) {
            m_running = false;
            break;
        }

        // Allow App-level quit via input
        if (frame.input.Down(Key::Escape)) {
            m_running = false;
            break;
        }

        // ---- Fixed timestep update ----
        accumulator += frame.dtSeconds;

        // Prevent spiral of death if the app hitches
        if (accumulator > 0.25f) accumulator = 0.25f;

        while (accumulator >= fixedDt) {
            game.Update(g_platform, frame.input, fixedDt);
            accumulator -= fixedDt;
        }

        const float alpha = (fixedDt > 0.0f) ? (accumulator / fixedDt) : 0.0f;

        // ---- Render ----
        g_platform.BeginFrame();
        game.Render(g_platform, alpha);
        g_platform.EndFrame();
    }
}

void App::Shutdown() {
    g_platform.Shutdown();
    std::printf("[INFO] Clean shutdown\n");
}
