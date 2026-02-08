#include "game/Game.h"
#include "platform/SdlPlatform.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <algorithm>

// -----------------------------
// Collision (circle vs circle)
// -----------------------------
static bool CheckCollision(const Entity& a, const Entity& b) {
    Vec2 d = a.pos - b.pos;
    float distSq = d.x * d.x + d.y * d.y;
    float r = a.radius + b.radius;
    return distSq <= r * r;
}

static void SeparateEntities(Entity& a, Entity& b) {
    Vec2 d = a.pos - b.pos;
    float distSq = d.x * d.x + d.y * d.y;
    float r = a.radius + b.radius;
    if (distSq >= r * r) return;

    float dist = std::sqrt(std::max(distSq, 0.0001f));
    Vec2 n = d * (1.0f / dist);
    float penetration = r - dist;

    a.pos = a.pos + n * (penetration * 0.5f);
    b.pos = b.pos - n * (penetration * 0.5f);
}

// -----------------------------
// ECS-lite: entity creation
// -----------------------------
Entity& Game::CreateEntity(EntityType type, Vec2 pos, float radius) {
    Entity e{};
    e.id = m_nextEntityId++;
    e.type = type;
    e.pos = pos;
    e.prevPos = pos;
    e.radius = radius;
    e.ai = AIState::Idle;
    e.aggroRadius = 350.0f;
    m_entities.push_back(e);
    return m_entities.back();
}

bool Game::Init(SdlPlatform& platform) {
    // Assets must init first or sprites won't render
    if (!m_assets.Init(platform))
        return false;

    m_map.LoadCSV("assets/maps/level01.csv");

    // Load config (speeds, world size, spawns, enemies)
    ReloadConfig("assets/config.json");

    // Create player (use config spawn ONCE on init)
    m_entities.clear();
    m_playerIndex = 0;
    CreateEntity(EntityType::Player, m_cfg.playerSpawn, 20.0f);

    // Spawn enemies from config
    RespawnEnemiesFromConfig();

    // Camera init (center on player)
    int winW = 0, winH = 0;
    platform.GetWindowSize(winW, winH);
    m_camera.SetZoom(1.0f);
    m_camera.SetShakeOffset({ 0.0f, 0.0f });

    const Entity& player = m_entities[m_playerIndex];
    m_camera.SetPosition(player.pos - Vec2{ (winW * 0.5f), (winH * 0.5f) });

    // Hot-reload timestamp init
    try {
        m_cfgTimestamp = std::filesystem::last_write_time("assets/config.json");
    }
    catch (...) {}

    m_cfgPollTimer = 0.0f;
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

    const float zoom = m_camera.Zoom();
    const Vec2 halfScreenPx{ winW * 0.5f, winH * 0.5f };
    const Vec2 halfScreenWorld = halfScreenPx * (1.0f / zoom);

    // Center camera on player in world space
    m_camera.SetPosition(player.pos - halfScreenWorld);

    // Clamp camera so we never show outside the world.
    Vec2 cam = m_camera.Position();
    const float viewW = (float)winW * (1.0f / zoom);
    const float viewH = (float)winH * (1.0f / zoom);

    float maxCamX = m_worldSize.x - viewW;
    float maxCamY = m_worldSize.y - viewH;

    if (maxCamX < 0.0f) maxCamX = 0.0f;
    if (maxCamY < 0.0f) maxCamY = 0.0f;

    if (cam.x < 0.0f) cam.x = 0.0f;
    if (cam.y < 0.0f) cam.y = 0.0f;
    if (cam.x > maxCamX) cam.x = maxCamX;
    if (cam.y > maxCamY) cam.y = maxCamY;

    m_camera.SetPosition(cam);
}

void Game::Update(SdlPlatform& platform, const Input& input, float fixedDt, DebugState& dbg) {
    // --------------------
    // HOT-RELOAD POLLING (runs even if paused)
    // --------------------
    m_cfgPollTimer += fixedDt;
    if (m_cfgPollTimer >= 1.0f) {
        m_cfgPollTimer = 0.0f;
        try {
            auto t = std::filesystem::last_write_time("assets/config.json");
            if (t != m_cfgTimestamp) {
                m_cfgTimestamp = t;
                ReloadConfig("assets/config.json");
                std::printf("[HOTRELOAD] config.json reloaded\n");
            }
        }
        catch (...) {}
    }

    // --------------------
    // MANUAL RELOAD (ImGui button)
    // --------------------
    if (dbg.requestReloadConfig) {
        dbg.requestReloadConfig = false;
        ReloadConfig("assets/config.json");
    }

    // --------------------
    // Toggle debug UI with Tab (edge-triggered)
    // --------------------
    static bool prevTab = false;
    bool tabNow = input.Down(Key::Tab);
    if (tabNow && !prevTab) {
        // If you keep imguiWantsKeyboard, this prevents fighting ImGui focus
        if (!dbg.imguiWantsKeyboard) {
            dbg.showUI = !dbg.showUI;
        }
    }
    prevTab = tabNow;

    // --------------------
    // PAUSE HANDLING
    // --------------------
    if (dbg.pause) {
        dbg.entityCount = (int)m_entities.size();
        dbg.playerPos = m_entities[m_playerIndex].pos;
        dbg.cameraPos = m_camera.Position();
        return;
    }

    // --------------------
    // Apply zoom from UI
    // --------------------
    if (dbg.zoom < 0.5f) dbg.zoom = 0.5f;
    if (dbg.zoom > 2.0f) dbg.zoom = 2.0f;
    m_camera.SetZoom(dbg.zoom);

    // --------------------
    // INPUT SYSTEM (player)
    // --------------------
    Entity& player = m_entities[m_playerIndex];
    player.prevPos = player.pos;

    Vec2 move{ 0, 0 };
    if (input.Down(Key::W)) move.y -= 1.0f;
    if (input.Down(Key::S)) move.y += 1.0f;
    if (input.Down(Key::A)) move.x -= 1.0f;
    if (input.Down(Key::D)) move.x += 1.0f;

    // (no normalization yet, intentionally simple)
    player.pos = player.pos + move * (m_playerSpeed * fixedDt);
    ClampPlayerToWorld(player);

    m_map.ResolveCircleCollision(player.pos, player.radius);

    // --------------------
    // AI SYSTEM (Idle -> Seek)
    // --------------------

    for (size_t i = 0; i < m_entities.size(); ++i) {
        Entity& e = m_entities[i];
        if (e.type != EntityType::Enemy) continue;

        e.prevPos = e.pos;

        Vec2 toPlayer = player.pos - e.pos;
        float distSq = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y;
        float aggroSq = e.aggroRadius * e.aggroRadius;

        // State transitions
        if (e.ai == AIState::Idle && distSq <= aggroSq) {
            e.ai = AIState::Seek;
        }
        else if (e.ai == AIState::Seek && distSq > aggroSq * 1.2f) {
            // hysteresis so it doesn't flicker
            e.ai = AIState::Idle;
        }

        // Behavior
        if (e.ai == AIState::Seek && distSq > 0.0001f) {
            float invLen = 1.0f / std::sqrt(distSq);
            Vec2 dir = toPlayer * invLen;
            e.pos = e.pos + dir * (m_enemySpeed * fixedDt);
            m_map.ResolveCircleCollision(e.pos, e.radius);
        }
    }


    // --------------------
    // SEPARATION SYSTEM (enemy vs enemy)
    // --------------------
    for (size_t i = 0; i < m_entities.size(); ++i) {
        if (m_entities[i].type != EntityType::Enemy) continue;

        for (size_t j = i + 1; j < m_entities.size(); ++j) {
            if (m_entities[j].type != EntityType::Enemy) continue;

            SeparateEntities(m_entities[i], m_entities[j]);
        }
    }

    // --------------------
    // COLLISION SYSTEM (player vs enemies)
    // --------------------
    for (size_t i = 0; i < m_entities.size(); ++i) {
        if ((int)i == m_playerIndex) continue;

        Entity& e = m_entities[i];
        if (e.type != EntityType::Enemy) continue;

        if (CheckCollision(player, e)) {
            // Simple response: revert player
            Vec2 d = player.pos - e.pos;
            float distSq = d.x * d.x + d.y * d.y;
            float r = player.radius + e.radius;

            if (distSq < r * r) {
                float dist = std::sqrt(std::max(distSq, 0.0001f));
                Vec2 n = d * (1.0f / dist);         // collision normal (away from enemy)

                float penetration = r - dist;
                player.pos = player.pos + n * penetration;  // push player out

                // Optional: small extra separation so it doesn't re-trigger instantly
                player.pos = player.pos + n * 0.5f;

                // Trigger shake once per frame of collision (fine for now)
                m_shakeDuration = 0.15f;
                m_shakeTime = m_shakeDuration;
                m_shakeStrength = dbg.shakeStrength;
            }

            break;
        }
    }

    // --------------------
    // CAMERA SYSTEM (follow + clamp)
    // --------------------
    UpdateCameraFollow(platform, player);

    // --------------------
    // CAMERA SHAKE (Step 3)
    // --------------------
    Vec2 shake{ 0.0f, 0.0f };
    if (m_shakeTime > 0.0f) {
        m_shakeTime -= fixedDt;
        if (m_shakeTime < 0.0f) m_shakeTime = 0.0f;

        const float t = (m_shakeDuration - m_shakeTime) * 60.0f;
        const float sx = std::sin(t * 12.9898f) * 43758.5453f;
        const float sy = std::sin(t * 78.233f) * 12345.6789f;

        auto frac = [](float v) { return v - std::floor(v); };
        float nx = frac(sx) * 2.0f - 1.0f;
        float ny = frac(sy) * 2.0f - 1.0f;

        float fade = (m_shakeDuration > 0.0f) ? (m_shakeTime / m_shakeDuration) : 0.0f;
        shake = Vec2{ nx, ny } * (m_shakeStrength * fade);
    }
    m_camera.SetShakeOffset(shake);

    // --------------------
    // DEBUG OUTPUT (for UI)
    // --------------------
    dbg.entityCount = (int)m_entities.size();
    dbg.playerPos = player.pos;
    dbg.cameraPos = m_camera.Position();
    dbg.entityCount = (int)m_entities.size();
    dbg.enemyCount = std::max(0, dbg.entityCount - 1);
    dbg.debugEntityCount = 0;
    for (const Entity& e : m_entities) {
        if (dbg.debugEntityCount >= DebugState::kMaxDebugEntities) break;
        auto& row = dbg.debugEntities[dbg.debugEntityCount++];

        row.id = e.id;
        row.type = (e.type == EntityType::Player) ? 0 : 1;
        row.x = e.pos.x;
        row.y = e.pos.y;
        row.radius = e.radius;
        row.ai = (e.type == EntityType::Enemy && e.ai == AIState::Seek) ? 1 : 0;
    }
}

void Game::DrawWorldGrid(SdlPlatform& platform) const {
    const int step = 64;

    int winW = 0, winH = 0;
    platform.GetWindowSize(winW, winH);

    Vec2 topLeft = m_camera.ScreenToWorld({ 0, 0 });
    Vec2 bottomRight = m_camera.ScreenToWorld({ (float)winW, (float)winH });

    int startX = (int)(topLeft.x / step) * step - step;
    int endX = (int)(bottomRight.x / step) * step + step;

    int startY = (int)(topLeft.y / step) * step - step;
    int endY = (int)(bottomRight.y / step) * step + step;

    for (int wx = startX; wx <= endX; wx += step) {
        Vec2 a = m_camera.WorldToScreen({ (float)wx, topLeft.y });
        Vec2 b = m_camera.WorldToScreen({ (float)wx, bottomRight.y });
        platform.DrawLine((int)a.x, (int)a.y, (int)b.x, (int)b.y);
    }

    for (int wy = startY; wy <= endY; wy += step) {
        Vec2 a = m_camera.WorldToScreen({ topLeft.x, (float)wy });
        Vec2 b = m_camera.WorldToScreen({ bottomRight.x, (float)wy });
        platform.DrawLine((int)a.x, (int)a.y, (int)b.x, (int)b.y);
    }
}

void Game::Render(SdlPlatform& platform, float alpha, const DebugState& dbg) {
    if (m_playerIndex < 0 || m_playerIndex >= (int)m_entities.size())
        return;

    if (dbg.showGrid) {
        DrawWorldGrid(platform);
    }

    const auto& playerTex = m_assets.Player();
    m_map.Render(platform, m_camera);

    for (const Entity& e : m_entities) {
        const Vec2 worldPos = e.prevPos + (e.pos - e.prevPos) * alpha;
        const Vec2 screenPos = m_camera.WorldToScreen(worldPos);

        if (e.type == EntityType::Player) {
            const int drawX = (int)(screenPos.x - playerTex.Width() * 0.5f);
            const int drawY = (int)(screenPos.y - playerTex.Height() * 0.5f);
            platform.DrawSprite(playerTex, drawX, drawY);
        }
        else {
            const int size = (int)(e.radius * 2.0f);
            const int drawX = (int)(screenPos.x - size * 0.5f);
            const int drawY = (int)(screenPos.y - size * 0.5f);
            platform.DrawFilledRect(drawX, drawY, size, size, 200, 80, 80);
        }
    }
}

bool Game::ReloadConfig(const char* path) {
    GameConfig newCfg = m_cfg;
    if (!LoadGameConfig(path, newCfg)) {
        return false;
    }

    // Update timestamp if file exists
    try {
        if (std::filesystem::exists(path)) {
            m_cfgTimestamp = std::filesystem::last_write_time(path);
        }
    }
    catch (...) {}

    // Apply values; respawn enemies; do NOT teleport player.
    ApplyConfig(newCfg, true);
    return true;
}

void Game::ApplyConfig(const GameConfig& cfg, bool respawnEnemies) {
    m_cfg = cfg;

    m_playerSpeed = m_cfg.playerSpeed;
    m_enemySpeed = m_cfg.enemySpeed;
    m_worldSize = { m_cfg.worldWidth, m_cfg.worldHeight };

    if (respawnEnemies) {
        RespawnEnemiesFromConfig();
    }
}

void Game::RespawnEnemiesFromConfig() {
    if (m_entities.empty())
        return;

    // Preserve current player state (pos/prevPos/id/etc.)
    Entity preservedPlayer = m_entities[m_playerIndex];

    // Rebuild entity list: player first, then enemies
    m_entities.clear();
    m_entities.push_back(preservedPlayer);
    m_playerIndex = 0;

    // Spawn enemies (ECS-lite)
    for (const auto& sp : m_cfg.enemySpawns) {
        CreateEntity(EntityType::Enemy, sp.pos, 18.0f);
    }
}



