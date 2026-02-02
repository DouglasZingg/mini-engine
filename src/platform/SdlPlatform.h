#pragma once
#include <cstdint>

struct SDL_Window;
struct SDL_Renderer;

struct SdlFrameData {
    float dtSeconds = 0.0f;
    float timeSeconds = 0.0f;
    bool keyW = false;
    bool keyA = false;
    bool keyS = false;
    bool keyD = false;
};

class SdlPlatform {
public:
    bool Init(int w, int h, const char* title);
    void Shutdown();

    // returns false when quitting
    bool Pump(SdlFrameData& outFrame);

    void BeginFrame();
    void DrawTestRect(float timeSeconds);
    void EndFrame();

private:
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    uint64_t m_perfFreq = 0;
    uint64_t m_prevCounter = 0;
    float    m_timeSeconds = 0.0f;
};
