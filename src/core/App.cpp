#include "core/App.h"

#include <cstdio>

#include "engine/DebugState.h"
#include "engine/DebugUI.h"
#include "game/Game.h"
#include "platform/SdlPlatform.h"

namespace {
SdlPlatform g_platform;
DebugState g_debug;
}

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
        std::printf("[ERROR] Game::Init failed. Check assets and config paths.\n");
        m_running = false;
        return;
    }

    DebugUI debugUI;
    if (!debugUI.Init(g_platform)) {
        std::printf("[ERROR] DebugUI init failed\n");
        m_running = false;
        return;
    }

    g_platform.SetEventCallback(&DebugUI::OnSdlEvent, &debugUI);

    const float fixedDt = 1.0f / 60.0f;
    float accumulator = 0.0f;

    while (m_running) {
        SdlFrameData frame{};
        if (!g_platform.Pump(frame)) {
            break;
        }

        g_debug.dt = frame.dtSeconds;
        g_debug.fps = (frame.dtSeconds > 0.0f) ? (1.0f / frame.dtSeconds) : 0.0f;

        accumulator += frame.dtSeconds;
        if (accumulator > 0.25f) {
            accumulator = 0.25f;
        }

        while (accumulator >= fixedDt) {
            game.Update(g_platform, frame.input, fixedDt, g_debug);
            accumulator -= fixedDt;
        }

        if (game.RequestedQuit()) {
            break;
        }

        const float alpha = (fixedDt > 0.0f) ? (accumulator / fixedDt) : 0.0f;

        g_platform.BeginFrame();

        debugUI.BeginFrame();
        game.Render(g_platform, alpha, g_debug);
        debugUI.Draw(g_debug);
        debugUI.EndFrame(g_platform);

        g_platform.EndFrame();
    }

    debugUI.Shutdown();
}

void App::Shutdown() {
    g_platform.Shutdown();
    std::printf("[INFO] Clean shutdown\n");
}
