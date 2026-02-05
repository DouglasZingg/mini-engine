#pragma once

/**
 * Simple game config loaded from assets/config.json.
 * This stays intentionally small while the engine is a prototype.
 */
struct GameConfig {
    float playerSpeed = 220.0f;
    float worldWidth  = 2000.0f;
    float worldHeight = 2000.0f;
};

bool LoadGameConfig(const char* path, GameConfig& outCfg);
