#pragma once
#include "engine/Assets.h"
#include "engine/Camera2D.h"
#include "engine/Config.h"
#include "engine/Input.h"
#include "engine/Math.h"
#include "game/Entity.h"

#include <vector>

class SdlPlatform;

/**
 * Game layer (rules + world state).
 * Owns entities and drives simulation/rendering using platform services.
 */
class Game {
public:
    bool Init(SdlPlatform& platform);

    // Fixed-step simulation update.
    void Update(SdlPlatform& platform, const Input& input, float fixedDt);

    // Render using interpolation alpha [0..1] between fixed steps.
    void Render(SdlPlatform& platform, float alpha);

private:
    void ClampPlayerToWorld(Entity& player) const;
    void UpdateCameraFollow(SdlPlatform& platform, const Entity& player);
    void DrawWorldGrid(SdlPlatform& platform) const;

private:
    Assets     m_assets;
    GameConfig m_cfg;
    Camera2D   m_camera;

    Vec2  m_worldSize{ 2000.0f, 2000.0f };
    float m_playerSpeed = 220.0f; // pixels/sec for now

    std::vector<Entity> m_entities;
    int m_playerIndex = -1;

    float m_debugTimer = 0.0f;
};
