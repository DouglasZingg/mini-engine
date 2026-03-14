#include "platform/SdlTexture.h"

#include <cstdio>

#include <SDL.h>

#include "platform/SdlPlatform.h"

bool SdlTexture::LoadBMP(SdlPlatform& platform, const char* path) {
    Destroy();

    SDL_Surface* surface = SDL_LoadBMP(path);
    if (!surface) {
        std::printf("[ERROR] SDL_LoadBMP failed (%s): %s\n", path, SDL_GetError());
        return false;
    }

    m_w = surface->w;
    m_h = surface->h;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(platform.RendererRaw(), surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        std::printf("[ERROR] SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
        return false;
    }

    m_tex = texture;
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
