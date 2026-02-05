#pragma once
#include "engine/Math.h"

enum class EntityType {
    Player,
    Enemy
};

struct Entity {
    EntityType type;
    Vec2 pos;
    Vec2 prevPos;
    float radius = 16.0f; // collision radius
};
