#include "platform/SdlPlatform.h"
#include <SDL.h>
#include <cstdio>
#include <algorithm>
#include <cmath>

bool SdlPlatform::Init(int w, int h, const char* title) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        std::printf("[ERROR] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    m_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h, SDL_WINDOW_SHOWN);
    if (!m_window) {
        std::printf("[ERROR] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer) {
        std::printf("[ERROR] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    m_perfFreq = static_cast<uint64_t>(SDL_GetPerformanceFrequency());
    m_prevCounter = static_cast<uint64_t>(SDL_GetPerformanceCounter());

    std::printf("[INFO] SDL init OK\n");
    return true;
}

void SdlPlatform::Shutdown() {
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
    SDL_Quit();
}

bool SdlPlatform::Pump(SdlFrameData& outFrame) {
    // timing
    const uint64_t now = static_cast<uint64_t>(SDL_GetPerformanceCounter());
    const uint64_t delta = now - m_prevCounter;
    m_prevCounter = now;

    float dt = (m_perfFreq > 0) ? (static_cast<float>(delta) / static_cast<float>(m_perfFreq)) : 0.0f;

    // compatibility clamp: avoid crazy dt on debugger pause
    dt = std::clamp(dt, 0.0f, 0.1f);
    m_timeSeconds += dt;

    outFrame.dtSeconds = dt;
    outFrame.timeSeconds = m_timeSeconds;

    // input state
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return false;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) return false;
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    outFrame.keyW = keys[SDL_SCANCODE_W] != 0;
    outFrame.keyA = keys[SDL_SCANCODE_A] != 0;
    outFrame.keyS = keys[SDL_SCANCODE_S] != 0;
    outFrame.keyD = keys[SDL_SCANCODE_D] != 0;

    // lightweight verification logging (only occasionally)
    static float logTimer = 0.0f;
    logTimer += dt;
    if (logTimer > 1.0f) {
        logTimer = 0.0f;
        std::printf("[INFO] dt=%.4f  t=%.2f  WASD=%d%d%d%d\n",
            outFrame.dtSeconds, outFrame.timeSeconds,
            (int)outFrame.keyW, (int)outFrame.keyA, (int)outFrame.keyS, (int)outFrame.keyD);
    }

    return true;
}

void SdlPlatform::BeginFrame() {
    SDL_SetRenderDrawColor(m_renderer, 15, 15, 18, 255);
    SDL_RenderClear(m_renderer);
}

void SdlPlatform::DrawTestRect(float timeSeconds) {
    // simple animated rectangle to prove render loop + vsync are working
    int w = 0, h = 0;
    SDL_GetWindowSize(m_window, &w, &h);

    const int rectW = 80;
    const int rectH = 80;

    const float t = timeSeconds;
    const int x = static_cast<int>((w - rectW) * (0.5f + 0.5f * std::sin(t)));
    const int y = h / 2 - rectH / 2;

    SDL_Rect r{ x, y, rectW, rectH };
    SDL_SetRenderDrawColor(m_renderer, 220, 220, 230, 255);
    SDL_RenderFillRect(m_renderer, &r);
}

void SdlPlatform::EndFrame() {
    SDL_RenderPresent(m_renderer);
}
