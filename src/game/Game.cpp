#include "game/Game.h"
#include "platform/SdlPlatform.h"
#include <cstdio>

bool Game::Init(SdlPlatform& platform) {
    if (!m_assets.Init(platform))
        return false;

    // Load config (defaults apply if missing)
    LoadGameConfig("assets/config.json", m_cfg);
    m_speed = m_cfg.playerSpeed;
    m_worldSize = { m_cfg.worldWidth, m_cfg.worldHeight };

    // Camera will be positioned in Update() based on current window size
    return true;
}

void Game::Update(SdlPlatform& platform, const Input& input, float fixedDt)
{
    m_prevPos = m_pos;

    // Movement input
    Vec2 dir{ 0,0 };
    if (input.Down(Key::W)) dir.y -= 1.0f;
    if (input.Down(Key::S)) dir.y += 1.0f;
    if (input.Down(Key::A)) dir.x -= 1.0f;
    if (input.Down(Key::D)) dir.x += 1.0f;

    // Integrate
    m_pos = m_pos + dir * (m_speed * fixedDt);

    const auto& playerTex = m_assets.Player();
    float halfW = playerTex.Width() * 0.25f * 0.5f;
    float halfH = playerTex.Height() * 0.25f * 0.5f;


    if (m_pos.x < halfW) m_pos.x = halfW;
    if (m_pos.y < halfH) m_pos.y = halfH;
    if (m_pos.x > m_worldSize.x - halfW) m_pos.x = m_worldSize.x - halfW;
    if (m_pos.y > m_worldSize.y - halfH) m_pos.y = m_worldSize.y - halfH;


    // Camera follow (center player on screen), then clamp camera to world
    int winW = 0, winH = 0;
    platform.GetWindowSize(winW, winH);

    Vec2 halfScreen{ winW * 0.5f, winH * 0.5f };
    m_camera.SetPosition(m_pos - halfScreen);

    Vec2 cam = m_camera.Position();
    float maxCamX = m_worldSize.x - (float)winW;
    float maxCamY = m_worldSize.y - (float)winH;

    if (maxCamX < 0.0f) maxCamX = 0.0f;
    if (maxCamY < 0.0f) maxCamY = 0.0f;

    if (cam.x < 0.0f) cam.x = 0.0f;
    if (cam.y < 0.0f) cam.y = 0.0f;
    if (cam.x > maxCamX) cam.x = maxCamX;
    if (cam.y > maxCamY) cam.y = maxCamY;

    m_camera.SetPosition(cam);

    // Debug print (once per second)
    static float t = 0.0f;
    t += fixedDt;
    if (t > 1.0f) {
        t = 0.0f;
        auto c = m_camera.Position();
        std::printf("[CAM] player(%.1f,%.1f) cam(%.1f,%.1f)\n", m_pos.x, m_pos.y, c.x, c.y);
    }
}

void Game::Render(SdlPlatform& platform, float alpha) {
    int winW = 0, winH = 0;
    platform.GetWindowSize(winW, winH);

    // World-space grid (moves with camera)
    const int step = 64;
    Vec2 cam = m_camera.Position();

    // Vertical lines
    int startX = (int)(cam.x / step) * step - step;
    int endX = (int)(cam.x + winW) + step;
    for (int wx = startX; wx <= endX; wx += step) {
        int sx = (int)(wx - cam.x);
        platform.DrawLine(sx, 0, sx, winH);
    }

    // Horizontal lines
    int startY = (int)(cam.y / step) * step - step;
    int endY = (int)(cam.y + winH) + step;
    for (int wy = startY; wy <= endY; wy += step) {
        int sy = (int)(wy - cam.y);
        platform.DrawLine(0, sy, winW, sy);
    }

    const auto& playerTex = m_assets.Player();

    Vec2 worldPos = m_prevPos + (m_pos - m_prevPos) * alpha;
    Vec2 screenPos = m_camera.WorldToScreen(worldPos);

    float scale = 0.25f; // try 0.25f, 0.5f, 1.0f

    int drawX = (int)(screenPos.x - playerTex.Width() * scale * 0.5f);
    int drawY = (int)(screenPos.y - playerTex.Height() * scale * 0.5f);

    platform.DrawSprite(playerTex, drawX, drawY, scale);

}
