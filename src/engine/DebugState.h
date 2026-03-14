#pragma once
#include <cstdint>

#include "engine/Math.h"

// Small POD shared between gameplay and the ImGui debug UI.
struct DebugState {
    bool showUI = false;
    bool showGrid = true;
    bool showColliders = false;
    bool showPaths = false;
    bool pause = false;

    float zoom = 1.0f;
    float shakeStrength = 6.0f;

    // Frame timing
    float dt = 0.0f;
    float fps = 0.0f;

    // Read-only runtime state
    Vec2 playerPos{ 0.0f, 0.0f };
    Vec2 cameraPos{ 0.0f, 0.0f };
    int entityCount = 0;
    int enemyCount = 0;

    // Combat tuning / readout
    int playerHealth = 3;
    int playerMaxHealth = 3;
    bool gameOver = false;
    float hitKnockback = 280.0f;
    float invulnSeconds = 0.75f;

    // ImGui capture flags
    bool imguiWantsKeyboard = false;
    bool imguiWantsMouse = false;

    // Commands
    bool requestReloadConfig = false;
    uint32_t selectedEntityId = 0;

    struct EntityDebugRow {
        uint32_t id = 0;
        int type = 0;   // 0 = player, 1 = enemy, 2 = pickup
        float x = 0.0f;
        float y = 0.0f;
        float radius = 0.0f;
        int ai = 0;     // 0 = idle, 1 = seek
    };

    static constexpr int kMaxDebugEntities = 256;
    int debugEntityCount = 0;
    EntityDebugRow debugEntities[kMaxDebugEntities]{};
};
