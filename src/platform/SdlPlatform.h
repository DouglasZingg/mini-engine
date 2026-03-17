#pragma once
#include <cstdint>

#include "engine/Input.h"

struct SDL_Window;
struct SDL_Renderer;
class SdlTexture;

// Per-frame data produced by the platform layer.
struct SdlFrameData {
    float dtSeconds = 0.0f;
    float timeSeconds = 0.0f;
    Input input;
};

// Minimal SDL2 platform wrapper. Owns the window / renderer and exposes
// just enough drawing and input for the game layer.
class SdlPlatform {
public:
    bool Init(int windowW, int windowH, const char* title);
    void Shutdown();

    bool Pump(SdlFrameData& outFrame);

    void BeginFrame();
    void EndFrame();

    void GetWindowSize(int& outW, int& outH) const;
    void GetMousePosition(int& outX, int& outY) const;
    bool MouseDownLeft() const;
    bool MousePressedLeft() const;
    SDL_Renderer* RendererRaw() const { return m_renderer; }
    SDL_Window* WindowRaw() const { return m_window; }

    void DrawSprite(const SdlTexture& tex, int x, int y);
    void DrawLine(int x1, int y1, int x2, int y2);
    void DrawFilledRect(int x, int y, int w, int h,
                        std::uint8_t r, std::uint8_t g, std::uint8_t b);

    using SdlEventCallback = void(*)(void* userData, const void* sdlEvent);
    void SetEventCallback(SdlEventCallback cb, void* userData);

    void ToggleFullscreen();

    void DrawTextBMP(const SdlTexture& font, int x, int y, const char* text,
                     int glyphW, int glyphH, int cols, int firstChar = 32, int scale = 2);

    // Tiny built-in tone generator used for lightweight UI/gameplay feedback.
    void PlayTone(float frequencyHz, float durationSeconds, float volume = 0.18f);

private:
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    std::uint64_t m_perfFreq = 0;
    std::uint64_t m_prevCounter = 0;
    float m_timeSeconds = 0.0f;

    SdlEventCallback m_eventCb = nullptr;
    void* m_eventUser = nullptr;

    // Input lives here so Pressed/Released survives across frames.
    Input m_input{};

    bool m_isFullscreen = false;
    int m_mouseX = 0;
    int m_mouseY = 0;
    bool m_mouseLeftDown = false;
    bool m_prevMouseLeftDown = false;
    std::uint32_t m_audioDevice = 0;
    int m_audioSampleRate = 48000;
    int m_windowedX = 0;
    int m_windowedY = 0;
    int m_windowedW = 1280;
    int m_windowedH = 720;
};
