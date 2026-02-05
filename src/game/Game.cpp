#include "game/Game.h"
#include "platform/SdlPlatform.h"

#include <cstdio>

// -----------------------------
// Collision (circle vs circle)
// -----------------------------
static bool CheckCollision(const Entity& a, const Entity& b) {
    Vec2 d = a.pos - b.pos;
    float distSq = d.x * d.x + d.y * d.y;
    float r = a.radius + b.radius;
    return distSq <= r * r;
}

bool Game::Init(SdlPlatform& platform) {
    // Load assets
    if (!m_assets.Init(platform)) {
        return false;
    }

    // Load config (defaults apply if missing)
    LoadGameConfig("assets/config.json", m_cfg);
    m_playerSpeed = m_cfg.playerSpeed;
    m_worldSize = { m_cfg.worldWidth, m_cfg.worldHeight };

    // -----------------------------
    // Create entities
    // -----------------------------
    m_entities.clear();
    m_entities.reserve(16);

    // Player
    Entity player{};
    player.type = EntityType::Player;
    player.pos = { 500.0f, 500.0f };
    player.prevPos = player.pos;
    player.radius = 20.0f;

    m_playerIndex = 0;
    m_entities.push_back(player);

    // Enemies (static for now)
    for (int i = 0; i < 5; ++i) {
        Entity e{};
        e.type = EntityType::Enemy;
        e.pos = { 700.0f + i * 120.0f, 600.0f };
        e.prevPos = e.pos;
        e.radius = 18.0f;
        m_entities.push_back(e);
    }

    // Camera will be positioned in Update() (depends on current window size).
    return true;
}

void Game::ClampPlayerToWorld(Entity& player) const {
    // Keep the entire sprite inside world bounds by clamping using half extents.
    const auto& tex = m_assets.Player();
    const float halfW = tex.Width() * 0.5f;
    const float halfH = tex.Height() * 0.5f;

    if (player.pos.x < halfW) player.pos.x = halfW;
    if (player.pos.y < halfH) player.pos.y = halfH;
    if (player.pos.x > m_worldSize.x - halfW) player.pos.x = m_worldSize.x - halfW;
    if (player.pos.y > m_worldSize.y - halfH) player.pos.y = m_worldSize.y - halfH;
}

void Game::UpdateCameraFollow(SdlPlatform& platform, const Entity& player) {
    int winW = 0, winH = 0;
    platform.GetWindowSize(winW, winH);

    const Vec2 halfScreen{ winW * 0.5f, winH * 0.5f };

    // Center the camera on the player.
    m_camera.SetPosition(player.pos - halfScreen);

    // Clamp camera so we never show outside the world.
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
}

void Game::Update(SdlPlatform& platform, const Input& input, float fixedDt) {
    if (m_playerIndex < 0 || m_playerIndex >= (int)m_entities.size()) {
        return;
    }

    // -----------------------------
    // Player movement (fixed step)
    // -----------------------------
    Entity& player = m_entities[m_playerIndex];
    player.prevPos = player.pos;

    Vec2 dir{ 0.0f, 0.0f };
    if (input.Down(Key::W)) dir.y -= 1.0f;
    if (input.Down(Key::S)) dir.y += 1.0f;
    if (input.Down(Key::A)) dir.x -= 1.0f;
    if (input.Down(Key::D)) dir.x += 1.0f;

    // Integrate (no normalization: diagonal is faster; acceptable for prototype)
    player.pos = player.pos + dir * (m_playerSpeed * fixedDt);

    // World bounds
    ClampPlayerToWorld(player);

    // -----------------------------
    // Collision: player vs enemies
    // -----------------------------
    for (size_t i = 0; i < m_entities.size(); ++i) {
        if ((int)i == m_playerIndex) continue;
        if (CheckCollision(player, m_entities[i])) {
            // Simple response: revert to previous position.
            player.pos = player.prevPos;
            break;
        }
    }

    // Camera follow
    UpdateCameraFollow(platform, player);

    // Debug prints (once per second)
    m_debugTimer += fixedDt;
    if (m_debugTimer >= 1.0f) {
        m_debugTimer = 0.0f;
        const Vec2 cam = m_camera.Position();
        std::printf("[DEBUG] entities=%zu  player(%.1f,%.1f)  cam(%.1f,%.1f)\n",
            m_entities.size(), player.pos.x, player.pos.y, cam.x, cam.y);
    }
}

void Game::DrawWorldGrid(SdlPlatform& platform) const {
    int winW = 0, winH = 0;
    platform.GetWindowSize(winW, winH);

    const int step = 64;
    const Vec2 cam = m_camera.Position();

    // Vertical lines in world space
    const int startX = (int)(cam.x / step) * step - step;
    const int endX   = (int)(cam.x + winW) + step;

    for (int wx = startX; wx <= endX; wx += step) {
        const int sx = (int)(wx - cam.x);
        platform.DrawLine(sx, 0, sx, winH);
    }

    // Horizontal lines in world space
    const int startY = (int)(cam.y / step) * step - step;
    const int endY   = (int)(cam.y + winH) + step;

    for (int wy = startY; wy <= endY; wy += step) {
        const int sy = (int)(wy - cam.y);
        platform.DrawLine(0, sy, winW, sy);
    }
}

void Game::Render(SdlPlatform& platform, float alpha) {
    if (m_playerIndex < 0 || m_playerIndex >= (int)m_entities.size()) {
        return;
    }

    // Background reference
    DrawWorldGrid(platform);

    const auto& playerTex = m_assets.Player();

    for (const Entity& e : m_entities) {
        // Interpolated position for smooth rendering
        const Vec2 worldPos  = e.prevPos + (e.pos - e.prevPos) * alpha;
        const Vec2 screenPos = m_camera.WorldToScreen(worldPos);

        if (e.type == EntityType::Player) {
            const int drawX = (int)(screenPos.x - playerTex.Width() * 0.5f);
            const int drawY = (int)(screenPos.y - playerTex.Height() * 0.5f);
            platform.DrawSprite(playerTex, drawX, drawY);
        } else {
            const int size  = (int)(e.radius * 2.0f);
            const int drawX = (int)(screenPos.x - size * 0.5f);
            const int drawY = (int)(screenPos.y - size * 0.5f);
            platform.DrawFilledRect(drawX, drawY, size, size, 200, 80, 80);
        }
    }
}
