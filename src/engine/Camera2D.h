#pragma once
#include "engine/Math.h"

class Camera2D {
public:
    void SetPosition(const Vec2& p) { m_pos = p; }
    const Vec2& Position() const { return m_pos; }

    void SetZoom(float z) { m_zoom = z; }
    float Zoom() const { return m_zoom; }

    void SetShakeOffset(const Vec2& s) { m_shake = s; }
    const Vec2& ShakeOffset() const { return m_shake; }

    // Convert world position to screen position (pixels)
    Vec2 WorldToScreen(const Vec2& world) const {
        // World relative to camera origin
        Vec2 local = world - m_pos;

        // Apply zoom around camera origin
        local = local * m_zoom;

        // Apply screen-space shake
        return local + m_shake;
    }

    // Convert screen pixel position to world (useful later)
    Vec2 ScreenToWorld(const Vec2& screen) const {
        Vec2 unshaken = screen - m_shake;
        return (unshaken * (1.0f / m_zoom)) + m_pos;
    }

private:
    Vec2 m_pos{ 0.0f, 0.0f };     // top-left in world space
    Vec2 m_shake{ 0.0f, 0.0f };   // screen-space pixels
    float m_zoom = 1.0f;
};
