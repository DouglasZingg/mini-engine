#pragma once
#include "DebugState.h"

class SdlPlatform;

/**
 * DebugUI wraps Dear ImGui lifecycle so the app/game doesn't need to know
 * backend details (SDL2 + SDL_Renderer).
 */
class DebugUI {
public:
    bool Init(SdlPlatform& platform);
    void Shutdown();

    void BeginFrame();
    void EndFrame(SdlPlatform& platform);

    bool Enabled() const { return m_enabled; }
    void SetEnabled(bool v) { m_enabled = v; }

    // Called from platform event hook (we keep SDL types out of public headers).
    static void OnSdlEvent(void* userData, const void* sdlEvent);

    void Draw(DebugState& dbg);

private:
    bool m_enabled = true;
    bool m_initialized = false;
};
