#include "platform/SdlTexture.h"
#include "platform/SdlPlatform.h"
#include <SDL.h>
#include <cstdio>

bool SdlTexture::LoadBMP(SdlPlatform& platform, const char* path) {
    Destroy();

    SDL_Surface* surf = SDL_LoadBMP(path);
    if (!surf) {
        std::printf("[ERROR] SDL_LoadBMP failed (%s): %s\n", path, SDL_GetError());
        return false;
    }

    m_w = surf->w;
    m_h = surf->h;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(platform.RendererRaw(), surf);
    SDL_FreeSurface(surf);

    if (!tex) {
        std::printf("[ERROR] SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
        return false;
    }

    m_tex = tex;
    std::printf("[INFO] Loaded BMP: %s (%dx%d)\n", path, m_w, m_h);
    return true;
}

void SdlTexture::Destroy() {
    if (m_tex) {
        SDL_DestroyTexture(m_tex);
        m_tex = nullptr;
    }
    m_w = 0;
    m_h = 0;
}
