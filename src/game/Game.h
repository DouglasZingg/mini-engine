#pragma once
#include "engine/Math.h"
#include "engine/Input.h"
#include "engine/Assets.h"
#include "engine/Camera2D.h"
#include "engine/Config.h"

class SdlPlatform;

class Game {
public:
    bool Init(SdlPlatform& platform);
    void Update(SdlPlatform& platform, const Input& input, float fixedDt);
    void Render(SdlPlatform& platform, float alpha);

private:
    Vec2 m_pos{ 200.0f, 200.0f };
    Vec2 m_prevPos{ 200.0f, 200.0f };
    Vec2 m_worldSize{ 2000.0f, 2000.0f };
    float m_speed = 220.0f; // pixels/sec
    Assets m_assets;
    Camera2D m_camera;
    GameConfig m_cfg;
};
