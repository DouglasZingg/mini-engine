#pragma once

struct SDL_Texture;
class SdlPlatform;

// Minimal BMP texture wrapper used by the SDL renderer path.
class SdlTexture {
public:
    bool LoadBMP(SdlPlatform& platform, const char* path);
    void Destroy();

    int Width() const { return m_w; }
    int Height() const { return m_h; }
    SDL_Texture* Raw() const { return m_tex; }

private:
    SDL_Texture* m_tex = nullptr;
    int m_w = 0;
    int m_h = 0;
};
