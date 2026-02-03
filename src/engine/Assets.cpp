#include "engine/Assets.h"
#include "platform/SdlPlatform.h"

bool Assets::Init(SdlPlatform& platform) {
    // Keep paths simple for now; we’ll improve to a robust assets path later
    return m_player.LoadBMP(platform, "assets/player.bmp");
}

void Assets::Shutdown() {
    m_player.Destroy();
}
