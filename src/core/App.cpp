#include "core/App.h"
#include "game/Game.h"
#include "engine/DebugUI.h"
#include "platform/SdlPlatform.h"
#include "engine/DebugState.h"
#include <cstdio>

static SdlPlatform g_platform;
DebugState dbg;

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
        std::printf("[ERROR] Game::Init failed. Check assets path (assets/player.bmp) and config.\n");
        m_running = false;
        return;
    }


    DebugUI debugUI;
    if (!debugUI.Init(g_platform)) {
        std::printf("[ERROR] DebugUI init failed\n");
        return;
    }

    g_platform.SetEventCallback(&DebugUI::OnSdlEvent, &debugUI);

    // Fixed timestep simulation parameters
    const float fixedDt = 1.0f / 60.0f;
    float accumulator = 0.0f;

    while (m_running) {
        // ---- Poll platform ----
        SdlFrameData frame{};
        if (!g_platform.Pump(frame))
            break;

        dbg.dt = frame.dtSeconds;
        dbg.fps = (frame.dtSeconds > 0.0f) ? (1.0f / frame.dtSeconds) : 0.0f;

        // ---- Fixed timestep update ----
        accumulator += frame.dtSeconds;

        // Prevent spiral of death if the app hitches
        if (accumulator > 0.25f) accumulator = 0.25f;

        while (accumulator >= fixedDt) {
            game.Update(g_platform, frame.input, fixedDt, dbg);
            accumulator -= fixedDt;
        }

        if (game.RequestedQuit())
            break;

        const float alpha = (fixedDt > 0.0f) ? (accumulator / fixedDt) : 0.0f;

        // ---- Render ----
        g_platform.BeginFrame();

        debugUI.BeginFrame();
        debugUI.Draw(dbg);     // NEW
        game.Render(g_platform, alpha, dbg); // we’ll pass dbg into Render
        debugUI.EndFrame(g_platform);


        g_platform.EndFrame();
    }

    debugUI.Shutdown();
}

void App::Shutdown() {
    g_platform.Shutdown();
    std::printf("[INFO] Clean shutdown\n");
}
