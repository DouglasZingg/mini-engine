#pragma once
#include <cstdint>
#include "engine/Input.h"

// Forward declarations to avoid pulling SDL headers into the public interface.
struct SDL_Window;
struct SDL_Renderer;
class SdlTexture;

/**
 * Per-frame data produced by the platform layer.
 */
struct SdlFrameData {
    float dtSeconds = 0.0f;     // time since last frame (seconds)
    float timeSeconds = 0.0f;   // running time since start (seconds)
    Input input;               // current input snapshot
};

/**
 * Minimal SDL2 platform wrapper.
 * Owns window + renderer and provides:
 *  - timing
 *  - input polling
 *  - basic 2D drawing helpers
 */
class SdlPlatform {
public:
    bool Init(int windowW, int windowH, const char* title);
    void Shutdown();

    // Returns false when the app should quit.
    bool Pump(SdlFrameData& outFrame);

    // Frame lifecycle
    void BeginFrame();
    void EndFrame();

    // Query helpers
    void GetWindowSize(int& outW, int& outH) const;
    SDL_Renderer* RendererRaw() const { return m_renderer; }

    // Drawing helpers (screen-space)
    void DrawSprite(const SdlTexture& tex, int x, int y);
    void DrawLine(int x1, int y1, int x2, int y2);
    void DrawFilledRect(int x, int y, int w, int h,
                        std::uint8_t r, std::uint8_t g, std::uint8_t b);

    // Debug helper: proves render loop + vsync are working.
    void DrawDebugTestRect(float timeSeconds);

private:
    SDL_Window*   m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    std::uint64_t m_perfFreq = 0;
    std::uint64_t m_prevCounter = 0;
    float         m_timeSeconds = 0.0f;
};
