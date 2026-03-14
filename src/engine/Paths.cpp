#include "engine/Paths.h"

#include <filesystem>
#include <string>
#include <vector>

#include <SDL.h>

std::string AssetPath(const char* relPath) {
    namespace fs = std::filesystem;

    char* basePathRaw = SDL_GetBasePath();
    const fs::path exeDir = basePathRaw ? fs::path(basePathRaw) : fs::current_path();
    SDL_free(basePathRaw);

    const fs::path relativePath(relPath);
    const std::vector<fs::path> roots = {
        exeDir,
        exeDir / ".." / "..",
        fs::current_path()
    };

    for (const fs::path& root : roots) {
        const fs::path candidate = root / relativePath;
        std::error_code ec;
        if (fs::exists(candidate, ec)) {
            return candidate.lexically_normal().string();
        }
    }

    return (exeDir / relativePath).lexically_normal().string();
}
