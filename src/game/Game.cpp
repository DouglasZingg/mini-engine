#include "game/Game.h"
#include "platform/SdlPlatform.h"

bool Game::Init(SdlPlatform&) {
    return true;
}

void Game::Update(SdlPlatform& platform, const Input& input, float fixedDt)
{
    m_prevPos = m_pos;

    Vec2 dir{ 0,0 };
    if (input.Down(Key::W)) dir.y -= 1.0f;
    if (input.Down(Key::S)) dir.y += 1.0f;
    if (input.Down(Key::A)) dir.x -= 1.0f;
    if (input.Down(Key::D)) dir.x += 1.0f;

    // no normalization today (keep it simple)
    m_pos = m_pos + dir * (m_speed * fixedDt);

    int w = 0, h = 0;
    platform.GetWindowSize(w, h);

    const float size = 40.0f; // must match DrawPlayerRect size
    if (w > 0 && h > 0) {
        if (m_pos.x < 0.0f) m_pos.x = 0.0f;
        if (m_pos.y < 0.0f) m_pos.y = 0.0f;
        if (m_pos.x > (float)w - size) m_pos.x = (float)w - size;
        if (m_pos.y > (float)h - size) m_pos.y = (float)h - size;
    }
}

void Game::Render(SdlPlatform& platform, float alpha) {
    // interpolate for smooth rendering between fixed updates
    Vec2 renderPos = m_prevPos + (m_pos - m_prevPos) * alpha;
    platform.DrawPlayerRect((int)renderPos.x, (int)renderPos.y);
}
