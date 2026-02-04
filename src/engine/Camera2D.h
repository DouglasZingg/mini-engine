#pragma once
#include "engine/Math.h"

class Camera2D {
public:
    void SetPosition(const Vec2& p) { m_pos = p; }
    const Vec2& Position() const { return m_pos; }

    Vec2 WorldToScreen(const Vec2& world) const {
        return world - m_pos;
    }

private:
    Vec2 m_pos{ 0.0f, 0.0f };
};
