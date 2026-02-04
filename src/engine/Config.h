#pragma once
#include <string>

struct GameConfig {
    float playerSpeed = 220.0f;
    float worldWidth = 2000.0f;
    float worldHeight = 2000.0f;
};

bool LoadGameConfig(const char* path, GameConfig& outCfg);
