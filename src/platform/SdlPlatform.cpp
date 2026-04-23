#include "platform/SdlPlatform.h"
#include "platform/SdlTexture.h"

#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <vector>

bool SdlPlatform::Init(int windowW, int windowH, const char* title) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0) {
        std::printf("[ERROR] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    m_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        windowW,
        windowH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED
    );

    if (!m_window) {
        std::printf("[ERROR] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    // Start maximized (still windowed; respects taskbar, etc.).
    SDL_MaximizeWindow(m_window);

    // Accelerated + vsync renderer (good default for a tiny engine).
    m_renderer = SDL_CreateRenderer(m_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!m_renderer) {
        std::printf("[ERROR] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want{};
    SDL_AudioSpec have{};
    want.freq = 48000;
    want.format = AUDIO_F32SYS;
    want.channels = 1;
    want.samples = 2048;

    m_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (m_audioDevice != 0) {
        m_audioSampleRate = (have.freq > 0) ? have.freq : 48000;
        SDL_PauseAudioDevice(m_audioDevice, 0);
    } else {
        std::printf("[WARN] SDL audio init failed: %s\n", SDL_GetError());
    }

    m_perfFreq = static_cast<std::uint64_t>(SDL_GetPerformanceFrequency());
    m_prevCounter = static_cast<std::uint64_t>(SDL_GetPerformanceCounter());

    SDL_RendererInfo info{};
    SDL_GetRendererInfo(m_renderer, &info);
    std::printf("[INFO] SDL init OK (renderer: %s)\n", info.name ? info.name : "unknown");
    return true;
}

void SdlPlatform::Shutdown() {
    if (m_audioDevice != 0) {
        SDL_ClearQueuedAudio(m_audioDevice);
        SDL_CloseAudioDevice(m_audioDevice);
        m_audioDevice = 0;
    }
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

bool SdlPlatform::Pump(SdlFrameData& outFrame) {
    // Keep input history inside the platform so Pressed/Released works even though App creates SdlFrameData each loop.
    m_input.BeginFrame();
    m_prevMouseLeftDown = m_mouseLeftDown;

    // ---- Timing ----
    const std::uint64_t now = static_cast<std::uint64_t>(SDL_GetPerformanceCounter());
    const std::uint64_t delta = now - m_prevCounter;
    m_prevCounter = now;

    float dt = (m_perfFreq > 0)
        ? (static_cast<float>(delta) / static_cast<float>(m_perfFreq))
        : 0.0f;

    // Clamp dt to avoid extreme simulation steps after a debugger pause.
    dt = std::clamp(dt, 0.0f, 0.1f);

    m_timeSeconds += dt;

    outFrame.dtSeconds = dt;
    outFrame.timeSeconds = m_timeSeconds;

    // ---- Events ----
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            return false;
        }

        // Fullscreen toggles belong here so they react to actual key-down events (not held state).
        if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
            const SDL_Keycode key = e.key.keysym.sym;
            const SDL_Keymod   mod = (SDL_Keymod)e.key.keysym.mod;

            if (key == SDLK_F11) {
                ToggleFullscreen();
            }
            else if (key == SDLK_RETURN && (mod & KMOD_ALT)) {
                ToggleFullscreen();
            }
        }

        if (m_eventCb) {
            m_eventCb(m_eventUser, &e);
        }
    }

    // ---- Input snapshot ----
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    m_input.SetKey(Key::W, keys[SDL_SCANCODE_W] != 0);
    m_input.SetKey(Key::A, keys[SDL_SCANCODE_A] != 0);
    m_input.SetKey(Key::S, keys[SDL_SCANCODE_S] != 0);
    m_input.SetKey(Key::D, keys[SDL_SCANCODE_D] != 0);
    m_input.SetKey(Key::Escape, keys[SDL_SCANCODE_ESCAPE] != 0);
    m_input.SetKey(Key::Tab, keys[SDL_SCANCODE_TAB] != 0);
    m_input.SetKey(Key::Return, keys[SDL_SCANCODE_RETURN] != 0);
    m_input.SetKey(Key::R, keys[SDL_SCANCODE_R] != 0);
    m_input.SetKey(Key::Space, keys[SDL_SCANCODE_SPACE] != 0);
    m_input.SetKey(Key::F11, keys[SDL_SCANCODE_F11] != 0);

    int mouseButtons = SDL_GetMouseState(&m_mouseX, &m_mouseY);
    m_mouseLeftDown = (mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;

    // Output a copy for this frame.
    outFrame.input = m_input;

    return true;
}

void SdlPlatform::BeginFrame() {
    SDL_SetRenderDrawColor(m_renderer, 15, 15, 18, 255);
    SDL_RenderClear(m_renderer);
}

void SdlPlatform::EndFrame() {
    SDL_RenderPresent(m_renderer);
}

void SdlPlatform::GetWindowSize(int& outW, int& outH) const {
    outW = 0;
    outH = 0;
    if (m_window) {
        SDL_GetWindowSize(m_window, &outW, &outH);
    }
}

void SdlPlatform::GetMousePosition(int& outX, int& outY) const {
    outX = m_mouseX;
    outY = m_mouseY;
}

bool SdlPlatform::MousePressedLeft() const {
    return m_mouseLeftDown && !m_prevMouseLeftDown;
}

void SdlPlatform::DrawSprite(const SdlTexture& tex, int x, int y) {
    SDL_Texture* t = tex.Raw();
    if (!t) return;

    SDL_Rect dst{ x, y, tex.Width(), tex.Height() };
    SDL_RenderCopy(m_renderer, t, nullptr, &dst);
}

void SdlPlatform::DrawLine(int x1, int y1, int x2, int y2) {
    SDL_SetRenderDrawColor(m_renderer, 40, 40, 50, 255);
    SDL_RenderDrawLine(m_renderer, x1, y1, x2, y2);
}

void SdlPlatform::DrawFilledRect(int x, int y, int w, int h,
                                 std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);
    SDL_Rect rc{ x, y, w, h };
    SDL_RenderFillRect(m_renderer, &rc);
}

void SdlPlatform::DrawFilledRectAlpha(int x, int y, int w, int h,
                                      std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, r, g, b, a);
    SDL_Rect rc{ x, y, w, h };
    SDL_RenderFillRect(m_renderer, &rc);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);
}

// (Removed) legacy debug test rect helper.
void SdlPlatform::SetEventCallback(SdlEventCallback cb, void* userData) {
    m_eventCb = cb;
    m_eventUser = userData;
}

void SdlPlatform::ToggleFullscreen() {
    if (!m_window)
        return;

    m_isFullscreen = !m_isFullscreen;

    if (m_isFullscreen) {
        // True fullscreen (changes display mode)
        SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN);
    }
    else {
        // Back to windowed
        SDL_SetWindowFullscreen(m_window, 0);

        // Restore windowed maximized state
        SDL_MaximizeWindow(m_window);
    }
}

void SdlPlatform::DrawTextBMP(const SdlTexture& font, int x, int y, const char* text,
    int glyphW, int glyphH, int cols, int firstChar, int scale)
{
    if (!text) return;

    int penX = x;
    int penY = y;

    for (const char* p = text; *p; ++p)
    {
        char c = *p;

        if (c == '\n') { penX = x; penY += glyphH * scale; continue; }
        if (c == '\t') { penX += glyphW * scale * 4; continue; }

        int code = (unsigned char)c;
        int idx = code - firstChar;
        if (idx < 0) { penX += glyphW * scale; continue; }

        int sx = (idx % cols) * glyphW;
        int sy = (idx / cols) * glyphH;

        SDL_Rect src{ sx, sy, glyphW, glyphH };
        SDL_Rect dst{ penX, penY, glyphW * scale, glyphH * scale };

        SDL_Texture* t = font.Raw(); // however you expose SDL_Texture*
        if (t) SDL_RenderCopy(m_renderer, t, &src, &dst);

        penX += glyphW * scale;
    }
}

void SdlPlatform::PlayTone(float frequencyHz, float durationSeconds, float volume) {
    if (m_audioDevice == 0) return;

    frequencyHz = std::clamp(frequencyHz, 80.0f, 2400.0f);
    durationSeconds = std::clamp(durationSeconds, 0.02f, 0.35f);
    volume = std::clamp(volume, 0.0f, 0.4f);

    const int sampleCount = std::max(1, static_cast<int>(durationSeconds * static_cast<float>(m_audioSampleRate)));
    std::vector<float> samples(static_cast<size_t>(sampleCount), 0.0f);

    const float attack = 0.01f;
    const float release = 0.03f;
    const int attackSamples = std::max(1, static_cast<int>(attack * m_audioSampleRate));
    const int releaseSamples = std::max(1, static_cast<int>(release * m_audioSampleRate));

    const float step = 6.28318530718f * frequencyHz / static_cast<float>(m_audioSampleRate);
    float phase = 0.0f;

    for (int i = 0; i < sampleCount; ++i) {
        float env = 1.0f;
        if (i < attackSamples) {
            env = static_cast<float>(i) / static_cast<float>(attackSamples);
        } else if (i > sampleCount - releaseSamples) {
            env = static_cast<float>(sampleCount - i) / static_cast<float>(releaseSamples);
        }

        samples[static_cast<size_t>(i)] = std::sin(phase) * volume * env;
        phase += step;
    }

    SDL_QueueAudio(m_audioDevice, samples.data(), static_cast<Uint32>(samples.size() * sizeof(float)));
}
