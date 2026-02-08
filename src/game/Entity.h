#pragma once
#include <cstdint>
#include "engine/Math.h"

enum class EntityType {
    Player,
    Enemy
};

using EntityId = uint32_t;   // <-- ADD THIS BEFORE struct Entity
enum class AIState { Idle, Seek };

struct Entity {
    EntityId id = 0;
    EntityType type = EntityType::Enemy;
    AIState ai = AIState::Idle;     // NEW

    Vec2 pos{ 0,0 };
    Vec2 prevPos{ 0,0 };
    float radius = 16.0f;

    float aggroRadius = 350.0f;     // NEW (world units)
};
