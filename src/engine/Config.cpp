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

static bool ExtractVec2(const std::string& txt, const char* key, Vec2& out) {
    size_t pos = txt.find(key);
    if (pos == std::string::npos) return false;

    auto xPos = txt.find("\"x\"", pos);
    auto yPos = txt.find("\"y\"", pos);
    if (xPos == std::string::npos || yPos == std::string::npos) return false;

    out.x = std::stof(txt.substr(txt.find(':', xPos) + 1));
    out.y = std::stof(txt.substr(txt.find(':', yPos) + 1));
    return true;
}

static void ExtractEnemySpawns(const std::string& txt,
    std::vector<SpawnPoint>& out) {
    size_t pos = txt.find("\"enemies\"");
    if (pos == std::string::npos) return;

    size_t cur = pos;
    while (true) {
        auto xPos = txt.find("\"x\"", cur);
        auto yPos = txt.find("\"y\"", cur);
        if (xPos == std::string::npos || yPos == std::string::npos)
            break;

        SpawnPoint sp;
        sp.pos.x = std::stof(txt.substr(txt.find(':', xPos) + 1));
        sp.pos.y = std::stof(txt.substr(txt.find(':', yPos) + 1));
        out.push_back(sp);

        cur = yPos + 1;
    }
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
    ExtractFloat(txt, "enemy_speed", outCfg.enemySpeed);
    ExtractFloat(txt, "world_width", outCfg.worldWidth);
    ExtractFloat(txt, "world_height", outCfg.worldHeight);

    ExtractVec2(txt, "player_spawn", outCfg.playerSpawn);
    outCfg.enemySpawns.clear();
    ExtractEnemySpawns(txt, outCfg.enemySpawns);

    return true;
}
