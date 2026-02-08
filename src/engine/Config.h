/**
 * Simple game config loaded from assets/config.json.
 * This stays intentionally small while the engine is a prototype.
 */
#pragma once
#include <vector>
#include "engine/Math.h"

struct SpawnPoint {
    Vec2 pos;
};

struct GameConfig {
    float playerSpeed = 220.0f;
    float enemySpeed = 120.0f;
    float worldWidth = 2000.0f;
    float worldHeight = 2000.0f;

    Vec2 playerSpawn{ 500.0f, 500.0f };
    std::vector<SpawnPoint> enemySpawns;
};


bool LoadGameConfig(const char* path, GameConfig& outCfg);
