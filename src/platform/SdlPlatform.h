#pragma once
#include <cstdint>
#include "engine/Input.h"

struct SDL_Window;
struct SDL_Renderer;
class SdlTexture;

struct SdlFrameData {
    float dtSeconds = 0.0f;
    float timeSeconds = 0.0f;
    Input input;
};

struct SDL_Renderer;

class SdlPlatform {
public:
    bool Init(int w, int h, const char* title);
    void Shutdown();

    // returns false when quitting
    bool Pump(SdlFrameData& outFrame);

    void BeginFrame();
    void DrawTestRect(float timeSeconds);
    void DrawPlayerRect(int x, int y);
    SDL_Renderer* RendererRaw() const { return m_renderer; }
    void GetWindowSize(int& outW, int& outH) const;
    void EndFrame();
    void DrawSprite(const SdlTexture& tex, int x, int y);

private:
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    uint64_t m_perfFreq = 0;
    uint64_t m_prevCounter = 0;
    float    m_timeSeconds = 0.0f;
};
