#include "platform/SdlPlatform.h"
#include "platform/SdlTexture.h"
#include <SDL.h>
#include <cstdio>
#include <algorithm>
#include <cmath>

bool SdlPlatform::Init(int w, int h, const char* title) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        std::printf("[ERROR] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP;
    m_window = SDL_CreateWindow(title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h, flags);

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
    outFrame.input.SetKey(Key::W, keys[SDL_SCANCODE_W] != 0);
    outFrame.input.SetKey(Key::A, keys[SDL_SCANCODE_A] != 0);
    outFrame.input.SetKey(Key::S, keys[SDL_SCANCODE_S] != 0);
    outFrame.input.SetKey(Key::D, keys[SDL_SCANCODE_D] != 0);
    outFrame.input.SetKey(Key::Escape, keys[SDL_SCANCODE_ESCAPE] != 0);

    // lightweight verification logging (only occasionally)
    static float logTimer = 0.0f;
    logTimer += dt;
    if (logTimer > 1.0f) {
        logTimer = 0.0f;
        std::printf("[INFO] dt=%.4f  t=%.2f  WASD=%d%d%d%d\n",
            outFrame.dtSeconds, outFrame.timeSeconds,
            (int)outFrame.input.Down(Key::W),
            (int)outFrame.input.Down(Key::A),
            (int)outFrame.input.Down(Key::S),
            (int)outFrame.input.Down(Key::D));
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

void SdlPlatform::DrawPlayerRect(int x, int y) {
    SDL_Rect r{ x, y, 40, 40 };
    SDL_SetRenderDrawColor(m_renderer, 80, 220, 140, 255);
    SDL_RenderFillRect(m_renderer, &r);
}

void SdlPlatform::GetWindowSize(int& outW, int& outH) const {
    outW = 0; outH = 0;
    if (m_window) {
        SDL_GetWindowSize(m_window, &outW, &outH);
    }
}

void SdlPlatform::DrawSprite(const SdlTexture& tex, int x, int y, float scale) {
    SDL_Texture* t = tex.Raw();
    if (!t) return;

    SDL_Rect dst{
        x,
        y,
        (int)(tex.Width() * scale),
        (int)(tex.Height() * scale)
    };
    SDL_RenderCopy(m_renderer, t, nullptr, &dst);
}

void SdlPlatform::DrawLine(int x1, int y1, int x2, int y2) {
    SDL_SetRenderDrawColor(m_renderer, 40, 40, 50, 255);
    SDL_RenderDrawLine(m_renderer, x1, y1, x2, y2);
}
