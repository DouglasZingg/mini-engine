#include "engine/Config.h"
#include <fstream>
#include <string>

static bool ExtractFloat(const std::string& text, const char* key, float& out) {
    size_t pos = text.find(key);
    if (pos == std::string::npos) return false;
    pos = text.find(':', pos);
    if (pos == std::string::npos) return false;
    out = std::stof(text.substr(pos + 1));
    return true;
}

bool LoadGameConfig(const char* path, GameConfig& outCfg) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string txt(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>()
    );

    ExtractFloat(txt, "player_speed", outCfg.playerSpeed);
    ExtractFloat(txt, "world_width", outCfg.worldWidth);
    ExtractFloat(txt, "world_height", outCfg.worldHeight);
    return true;
}
