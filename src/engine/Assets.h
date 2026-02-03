#pragma once
#include "platform/SdlTexture.h"

class SdlPlatform;

class Assets {
public:
    bool Init(SdlPlatform& platform);
    void Shutdown();

    const SdlTexture& Player() const { return m_player; }

private:
    SdlTexture m_player;
};
