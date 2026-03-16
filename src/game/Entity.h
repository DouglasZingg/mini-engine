#pragma once
#include <cstdint>
#include <vector>

#include "engine/Math.h"

// Minimal gameplay entity types used by the current project.
enum class EntityType { Player, Enemy, Pickup };

enum class PickupKind {
    Token = 0,
    Health,
    Speed,
    Shield
};

enum class AIState { Idle, Patrol, Alert, Seek, ReturnHome, Stunned };
enum class EnemyKind : uint8_t { Chaser = 0, Fast = 1, Tank = 2 };

using EntityId = uint32_t;

// Cached pathfinding state for enemies.
struct PathState {
    std::vector<Vec2> waypoints;   // world-space waypoint list
    int index = 0;                 // current waypoint
    float repathTimer = 0.0f;      // seconds until next repath
    int lastGoalTX = 999999;
    int lastGoalTY = 999999;
};

struct Entity {
    EntityId id = 0;
    EntityType type = EntityType::Enemy;
    AIState ai = AIState::Idle;

    Vec2 pos{ 0.0f, 0.0f };
    Vec2 prevPos{ 0.0f, 0.0f };
    Vec2 homePos{ 0.0f, 0.0f };    // patrol anchor / room center
    Vec2 patrolTarget{ 0.0f, 0.0f }; // current idle/patrol destination
    Vec2 vel{ 0.0f, 0.0f };        // movement / knockback velocity

    float radius = 16.0f;
    float aggroRadius = 350.0f;
    float moveSpeed = 0.0f;        // 0 = use global enemy speed

    EnemyKind enemyKind = EnemyKind::Chaser;
    PathState path;

    int health = 3;
    float invulnTimer = 0.0f;
    float invulnDuration = 1.5f;
    float hitstun = 0.0f;
    float stunTimer = 0.0f;
    float aiTimer = 0.0f;
    int patrolStep = 0;            // increments whenever a new patrol target is chosen
    int patrolRadiusTiles = 3;     // how far enemy can wander from home          // generic state timer (idle wait / alert windup)

    bool active = true;

    // Pickup-only data
    PickupKind pickupKind = PickupKind::Token;
    int value = 1;
};
