#include "engine/Config.h"

#include <fstream>
#include <string>

/**
 * Tiny JSON-ish float extractor.
 * We keep it lightweight for now (no dependency). It expects keys like:
 *   "player_speed": 220.0
 *
 * NOTE: This is not a full JSON parser; it's good enough for a small prototype.
 */
static bool ExtractFloat(const std::string& text, const char* key, float& out) {
    size_t pos = text.find(key);
    if (pos == std::string::npos) return false;
    pos = text.find(':', pos);
    if (pos == std::string::npos) return false;

    // std::stof stops at the first non-number (comma/brace), which works for our format.
    out = std::stof(text.substr(pos + 1));
    return true;
}

bool LoadGameConfig(const char* path, GameConfig& outCfg) {
    std::ifstream f(path);
    if (!f.is_open()) {
        return false;
    }

    std::string txt(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>()
    );

    ExtractFloat(txt, "player_speed", outCfg.playerSpeed);
    ExtractFloat(txt, "world_width",  outCfg.worldWidth);
    ExtractFloat(txt, "world_height", outCfg.worldHeight);
    return true;
}
