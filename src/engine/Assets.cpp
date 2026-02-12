#include "engine/Assets.h"
#include <SDL.h>
#include <string>
#include "engine/Paths.h"

bool Assets::Init(SdlPlatform& platform)
{
    return m_player.LoadBMP(platform, AssetPath("assets/player.bmp").c_str());
}


void Assets::Shutdown()
{
    m_player.Destroy();
}
