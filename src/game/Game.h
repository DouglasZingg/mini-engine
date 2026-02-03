#pragma once
#include "engine/Math.h"
#include "engine/Input.h"

class SdlPlatform;

class Game {
public:
    bool Init(SdlPlatform& platform);
    void Update(const Input& input, float fixedDt);
    void Render(SdlPlatform& platform, float alpha);

private:
    Vec2 m_pos{ 200.0f, 200.0f };
    Vec2 m_prevPos{ 200.0f, 200.0f };
    float m_speed = 220.0f; // pixels/sec
};
