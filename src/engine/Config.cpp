#include "engine/Config.h"

#include <fstream>
#include <string>
#include <iterator>

/*
    Tiny JSON-ish parser for the config file.
    This is intentionally simple and only looks for the fields we use.
    It is not a full JSON parser, but it is good enough for this prototype.
*/

static bool ExtractFloat(const std::string& text, const char* key, float& out)
{
    size_t pos = text.find(key);
    if (pos == std::string::npos) return false;

    pos = text.find(':', pos);
    if (pos == std::string::npos) return false;

    out = std::stof(text.substr(pos + 1));
    return true;
}

static bool ExtractVec2(const std::string& text, const char* key, Vec2& out)
{
    size_t pos = text.find(key);
    if (pos == std::string::npos) return false;

    size_t xPos = text.find("\"x\"", pos);
    size_t yPos = text.find("\"y\"", pos);
    if (xPos == std::string::npos || yPos == std::string::npos) return false;

    size_t xColon = text.find(':', xPos);
    size_t yColon = text.find(':', yPos);
    if (xColon == std::string::npos || yColon == std::string::npos) return false;

    out.x = std::stof(text.substr(xColon + 1));
    out.y = std::stof(text.substr(yColon + 1));
    return true;
}

static void ExtractEnemySpawns(const std::string& text, std::vector<SpawnPoint>& outSpawns)
{
    size_t pos = text.find("\"enemies\"");
    if (pos == std::string::npos) return;

    size_t cur = pos;
    while (true)
    {
        size_t xPos = text.find("\"x\"", cur);
        size_t yPos = text.find("\"y\"", cur);
        if (xPos == std::string::npos || yPos == std::string::npos)
            break;

        size_t xColon = text.find(':', xPos);
        size_t yColon = text.find(':', yPos);
        if (xColon == std::string::npos || yColon == std::string::npos)
            break;

        SpawnPoint sp;
        sp.pos.x = std::stof(text.substr(xColon + 1));
        sp.pos.y = std::stof(text.substr(yColon + 1));
        outSpawns.push_back(sp);

        cur = yPos + 1;
    }
}

bool LoadGameConfig(const char* path, GameConfig& outCfg)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    std::string text(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    ExtractFloat(text, "player_speed", outCfg.playerSpeed);
    ExtractFloat(text, "enemy_speed", outCfg.enemySpeed);
    ExtractFloat(text, "world_width", outCfg.worldWidth);
    ExtractFloat(text, "world_height", outCfg.worldHeight);

    ExtractVec2(text, "player_spawn", outCfg.playerSpawn);

    outCfg.enemySpawns.clear();
    ExtractEnemySpawns(text, outCfg.enemySpawns);

    return true;
}
