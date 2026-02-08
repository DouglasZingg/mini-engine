#pragma once
#include <cstdint>
#include "engine/Math.h"

struct DebugState {
    bool showUI = true;
    bool showGrid = true;
    bool showColliders = false;
    bool pause = false;

    float zoom = 1.0f;
    float shakeStrength = 6.0f;

    // Performance
    float dt = 0.0f;
    float fps = 0.0f;

    // Read-only stats
    Vec2 playerPos{ 0,0 };
    Vec2 cameraPos{ 0,0 };
    int entityCount = 0;
    int enemyCount = 0;

    // UI capture flags (set by DebugUI each frame)
    bool imguiWantsKeyboard = false;
    bool imguiWantsMouse = false;

    // Actions
    bool requestReloadConfig = false;

    // Entity inspector
    uint32_t selectedEntityId = 0;

    struct EntityDebugRow {
        uint32_t id = 0;
        int type = 0;    // 0=player, 1=enemy
        float x = 0.0f;
        float y = 0.0f;
        float radius = 0.0f;
        int ai = 0;      // 0=idle, 1=seek (enemy only)
    };

    static constexpr int kMaxDebugEntities = 256;
    int debugEntityCount = 0;
    EntityDebugRow debugEntities[kMaxDebugEntities];
};
