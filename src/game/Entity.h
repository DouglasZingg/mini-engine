#pragma once
#include <cstdint>
#include "engine/Math.h"
#include <vector>

enum class EntityType { Player, Enemy, Pickup };

using EntityId = uint32_t;  
enum class AIState { Idle, Seek };

struct PathState {
    std::vector<Vec2> waypoints; // world positions
    int index = 0;
    float repathTimer = 0.0f;
    int lastGoalTX = 999999;
    int lastGoalTY = 999999;
};

struct Entity {
    EntityId id = 0;
    EntityType type = EntityType::Enemy;
    AIState ai = AIState::Idle;    

    Vec2 pos{ 0,0 };
    Vec2 prevPos{ 0,0 };
    float radius = 16.0f;

    float aggroRadius = 350.0f;  

    // Combat (mostly for player)
    int health = 3;
    float invulnTimer = 0.0f;        // seconds remaining
    float invulnDuration = 0.75f;    // seconds
    Vec2 velocity{ 0,0 };            // for knockback / movement smoothing (optional)

    PathState path;

    bool active = true;
    int value = 1;      // for pickups
};