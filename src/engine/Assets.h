#pragma once
#include "platform/SdlTexture.h"

class SdlPlatform;

/**
 * Simple asset container.
 * For now this loads only the player BMP. Expand as the project grows.
 */
class Assets {
public:
    bool Init(SdlPlatform& platform);
    void Shutdown();

    const SdlTexture& Player() const { return m_player; }
    // Add:
    const SdlTexture& Font() const { return m_font; }

private:
    SdlTexture m_font;
    SdlTexture m_player;
};
