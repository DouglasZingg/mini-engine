#pragma once
#include "platform/SdlTexture.h"

class SdlPlatform;

// Small asset container for textures used across the game.
class Assets {
public:
    bool Init(SdlPlatform& platform);
    void Shutdown();

    const SdlTexture& Player() const { return m_player; }
    const SdlTexture& Font() const { return m_font; }

private:
    SdlTexture m_font;
    SdlTexture m_player;
};
