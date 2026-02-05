#include "engine/Assets.h"
#include "platform/SdlPlatform.h"

bool Assets::Init(SdlPlatform& platform) {
    // Keep paths simple for now; we assume the working directory is repo root.
    return m_player.LoadBMP(platform, "assets/player.bmp");
}

void Assets::Shutdown() {
    m_player.Destroy();
}
