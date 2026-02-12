#include "engine/Paths.h"
#include <SDL.h>

std::string AssetPath(const char* rel)
{
    char* base = SDL_GetBasePath();
    std::string p = base ? base : "";
    SDL_free(base);

    // your exe is usually build/Debug/, so go back to project root
    p += "../../";
    p += rel;
    return p;
}
