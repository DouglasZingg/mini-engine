#include "engine/Paths.h"

#include <SDL.h>
#include <filesystem>
#include <string>
#include <vector>

std::string AssetPath(const char* rel)
{
    namespace fs = std::filesystem;

    char* baseC = SDL_GetBasePath();
    fs::path exeDir = baseC ? fs::path(baseC) : fs::current_path();
    SDL_free(baseC);

    std::vector<fs::path> roots;
    roots.push_back(exeDir);              
    roots.push_back(exeDir / ".." / "..");  
    roots.push_back(fs::current_path());  

    fs::path relPath(rel);

    for (const fs::path& root : roots)
    {
        fs::path candidate = root / relPath;
        std::error_code ec;
        if (fs::exists(candidate, ec))
        {
            return candidate.lexically_normal().string();
        }
    }

    return (exeDir / relPath).lexically_normal().string();
}
