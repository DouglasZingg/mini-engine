#include "game/Game.h"
#include "platform/SdlPlatform.h"
#include "game/Pathfinding.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <algorithm>
#include <array>
#include <random>
#include <queue>
#include <engine/Assets.h>
#include "engine/Paths.h"
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

static void DrawCenteredOverlay(SdlPlatform& platform, const SdlTexture& font,
	const char* title, const char* bodyA, const char* bodyB)
{
	int w = 0, h = 0;
	platform.GetWindowSize(w, h);

	// background
	platform.DrawFilledRect(0, 0, w, h, 10, 10, 10);

	// panel
	const int pw = 760;
	const int ph = 240;
	const int px = (w - pw) / 2;
	const int py = (h - ph) / 2;
	platform.DrawFilledRect(px, py, pw, ph, 30, 30, 30);

	// text
	const int glyphW = 8, glyphH = 8, cols = 16, scale = 3;

	int tx = px + 24;
	int ty = py + 24;
	platform.DrawTextBMP(font, tx, ty, title, glyphW, glyphH, cols, 32, scale);

	ty += glyphH * scale * 2;
	platform.DrawTextBMP(font, tx, ty, bodyA, glyphW, glyphH, cols, 32, 2);

	ty += glyphH * 2 * 2;
	platform.DrawTextBMP(font, tx, ty, bodyB, glyphW, glyphH, cols, 32, 2);
}


static void DrawMenuOverlay(SdlPlatform& platform, const SdlTexture& font,
    const char* title,
    const char* const* items, int itemCount, int selectedIndex,
    const char* footerA = nullptr, const char* footerB = nullptr)
{
    int w = 0, h = 0;
    platform.GetWindowSize(w, h);

    // background
    platform.DrawFilledRect(0, 0, w, h, 10, 10, 10);

    // panel (a bit taller than the generic overlay)
    const int pw = 760;
    const int ph = 360;
    const int px = (w - pw) / 2;
    const int py = (h - ph) / 2;
    platform.DrawFilledRect(px, py, pw, ph, 30, 30, 30);

    const int glyphW = 8, glyphH = 8, cols = 16;

    int tx = px + 24;
    int ty = py + 24;

    // title
    platform.DrawTextBMP(font, tx, ty, title, glyphW, glyphH, cols, 32, 3);

    // items
    ty += glyphH * 3 * 2;
    for (int i = 0; i < itemCount; ++i) {
        char line[128];
        if (i == selectedIndex) {
            std::snprintf(line, sizeof(line), "> %s", items[i]);
        } else {
            std::snprintf(line, sizeof(line), "  %s", items[i]);
        }
        platform.DrawTextBMP(font, tx, ty, line, glyphW, glyphH, cols, 32, 2);
        ty += glyphH * 2 * 2;
    }

    // footer hints
    if (footerA) {
        ty = py + ph - 72;
        platform.DrawTextBMP(font, tx, ty, footerA, glyphW, glyphH, cols, 32, 2);
        if (footerB) {
            ty += glyphH * 2 * 2;
            platform.DrawTextBMP(font, tx, ty, footerB, glyphW, glyphH, cols, 32, 2);
        }
    }
}



static void DrawPulseDots(SdlPlatform& platform, const Camera2D& camera, const Vec2& center, float radius, int dotSize, int r, int g, int b) {
    constexpr int kDotCount = 24;
    const float twoPi = 6.28318530718f;
    for (int i = 0; i < kDotCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kDotCount);
        const float ang = t * twoPi;
        const Vec2 worldPos{
            center.x + std::cos(ang) * radius,
            center.y + std::sin(ang) * radius
        };
        const Vec2 screenPos = camera.WorldToScreen(worldPos);
        const int drawX = static_cast<int>(screenPos.x) - dotSize / 2;
        const int drawY = static_cast<int>(screenPos.y) - dotSize / 2;
        platform.DrawFilledRect(drawX, drawY, dotSize, dotSize,
            static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b));
    }
}

static bool IsWalkableProcTile(int v) {
    return v != 1;
}

static int CountReachableFloorTiles(const std::vector<int>& tiles, int w, int h, int sx, int sy) {
    if (w <= 0 || h <= 0) return 0;
    if (sx < 0 || sy < 0 || sx >= w || sy >= h) return 0;
    auto idx = [w](int x, int y) { return y * w + x; };
    if (!IsWalkableProcTile(tiles[idx(sx, sy)])) return 0;

    std::vector<unsigned char> seen((size_t)w * (size_t)h, 0);
    std::queue<std::pair<int,int>> q;
    q.push({sx, sy});
    seen[idx(sx, sy)] = 1;
    int count = 0;

    const int dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();
        ++count;
        for (const auto& d : dirs) {
            int nx = x + d[0];
            int ny = y + d[1];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            int id = idx(nx, ny);
            if (seen[id]) continue;
            if (!IsWalkableProcTile(tiles[id])) continue;
            seen[id] = 1;
            q.push({nx, ny});
        }
    }
    return count;
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

	GenerateProceduralLevel(m_currentLevel);
	ValidateAndSanitizeMap();
	UpdateWorldSizeFromMap();

	// Load config (speeds, world size, etc.)
	LoadGameConfig(AssetPath("assets/config.json").c_str(), m_cfg);

	// Camera defaults
	m_camera.SetZoom(1.0f);
	m_camera.SetShakeOffset({ 0.0f, 0.0f });

	// Hot-reload timestamp init
	try {
		m_cfgTimestamp = std::filesystem::last_write_time(AssetPath("assets/config.json"));
	}
	catch (...) {}
	m_cfgPollTimer = 0.0f;

	// Build entities (player/enemies/pickups) from the CSV markers
	RestartGame();

	// Center camera on player after spawn
	int winW = 0, winH = 0;
	platform.GetWindowSize(winW, winH);
	const Entity& player = m_entities[m_playerIndex];
	m_camera.SetPosition(player.pos - Vec2{ (winW * 0.5f), (winH * 0.5f) });

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

void Game::UpdateCameraFollow(SdlPlatform& platform, const Entity& player)
{
	int winW = 0, winH = 0;
	platform.GetWindowSize(winW, winH);

	// World size in pixels (or world units that match your render units)
	const float worldW = m_map.Width() * (float)m_map.TileSize();
	const float worldH = m_map.Height() * (float)m_map.TileSize();

	// Desired camera position: center player
	Vec2 camPos = player.pos - Vec2{ winW * 0.5f, winH * 0.5f };

	// Compute max scroll range (never negative)
	const float maxX = std::max(0.0f, worldW - (float)winW);
	const float maxY = std::max(0.0f, worldH - (float)winH);

	if (maxX <= 0.0f) {
		// World narrower than screen -> center world horizontally
		camPos.x = (worldW - (float)winW) * 0.5f; // negative is OK here; it centers
	}
	else {
		camPos.x = std::clamp(camPos.x, 0.0f, maxX);
	}

	if (maxY <= 0.0f) {
		// World shorter than screen -> center world vertically
		camPos.y = (worldH - (float)winH) * 0.5f;
	}
	else {
		camPos.y = std::clamp(camPos.y, 0.0f, maxY);
	}

	m_camera.SetPosition(camPos);
}



void Game::SyncDebugSnapshot(DebugState& dbg) const {
    dbg.entityCount = (int)m_entities.size();

    int enemyCount = 0;
    for (const Entity& e : m_entities) {
        if (e.active && e.type == EntityType::Enemy) enemyCount++;
    }
    dbg.enemyCount = enemyCount;

    if (m_playerIndex >= 0 && m_playerIndex < (int)m_entities.size()) {
        dbg.playerPos = m_entities[m_playerIndex].pos;
        dbg.playerHealth = m_entities[m_playerIndex].health;
    } else {
        dbg.playerPos = Vec2{ 0.0f, 0.0f };
        dbg.playerHealth = 0;
    }

    dbg.cameraPos = m_camera.Position();
    dbg.gameOver = (m_flowState == FlowState::Lose);

    dbg.debugEntityCount = 0;
    for (const Entity& e : m_entities) {
        if (dbg.debugEntityCount >= DebugState::kMaxDebugEntities) break;
        auto& row = dbg.debugEntities[dbg.debugEntityCount++];

        row.id = e.id;
        if (e.type == EntityType::Player) row.type = 0;
        else if (e.type == EntityType::Enemy) row.type = 1;
        else if (e.type == EntityType::Pickup) row.type = 2;
        else row.type = 3;

        row.x = e.pos.x;
        row.y = e.pos.y;
        row.radius = e.radius;
        row.ai = (e.type == EntityType::Enemy && e.ai == AIState::Seek) ? 1 : 0;
    }
}

bool Game::UpdateFlowScreens(const Input& input, DebugState& dbg) {
    const bool upPressed = input.Pressed(Action::Up);
    const bool downPressed = input.Pressed(Action::Down);
    const bool confirmPressed = input.Pressed(Action::Confirm);
    const bool cancelPressed = input.Pressed(Action::Cancel);
    const bool restartPressed = input.Pressed(Action::Restart);

    m_gameWin = (m_flowState == FlowState::Win);
    m_gameOver = (m_flowState == FlowState::Lose);

    switch (m_flowState) {
    case FlowState::Title: {
        const int itemCount = 3;
        if (upPressed)   m_titleMenuIndex = (m_titleMenuIndex + itemCount - 1) % itemCount;
        if (downPressed) m_titleMenuIndex = (m_titleMenuIndex + 1) % itemCount;

        if (confirmPressed) {
            if (m_titleMenuIndex == 0) m_flowState = FlowState::Playing;
            else if (m_titleMenuIndex == 1) m_flowState = FlowState::Controls;
            else m_requestQuit = true;
        }
        if (cancelPressed) m_requestQuit = true;

        SyncDebugSnapshot(dbg);
        return true;
    }

    case FlowState::Controls:
        if (confirmPressed || cancelPressed) m_flowState = FlowState::Title;
        SyncDebugSnapshot(dbg);
        return true;

    case FlowState::Paused: {
        const int itemCount = 4;
        if (upPressed)   m_pauseMenuIndex = (m_pauseMenuIndex + itemCount - 1) % itemCount;
        if (downPressed) m_pauseMenuIndex = (m_pauseMenuIndex + 1) % itemCount;

        if (confirmPressed) {
            if (m_pauseMenuIndex == 0) {
                m_flowState = FlowState::Playing;
            } else if (m_pauseMenuIndex == 1) {
                RestartGame();
                m_flowState = FlowState::Playing;
            } else if (m_pauseMenuIndex == 2) {
                RestartGame();
                m_flowState = FlowState::Title;
            } else {
                m_quitMenuIndex = 0;
                m_quitReturnState = FlowState::Paused;
                m_flowState = FlowState::QuitConfirm;
            }
        }

        if (cancelPressed) m_flowState = FlowState::Playing;

        SyncDebugSnapshot(dbg);
        return true;
    }

    case FlowState::QuitConfirm:
        if (upPressed || downPressed) m_quitMenuIndex = (m_quitMenuIndex + 1) % 2;
        if (confirmPressed) {
            if (m_quitMenuIndex == 0) m_flowState = m_quitReturnState;
            else m_requestQuit = true;
        }
        if (cancelPressed) m_flowState = m_quitReturnState;

        SyncDebugSnapshot(dbg);
        return true;

    case FlowState::Win:
        if (confirmPressed) {
            ++m_currentLevel;
            GenerateProceduralLevel(m_currentLevel);
            ValidateAndSanitizeMap();
            UpdateWorldSizeFromMap();
            RestartGame();
            m_flowState = FlowState::Playing;
        }
        else if (restartPressed) {
            RestartGame();
            m_flowState = FlowState::Playing;
        }
        if (cancelPressed) m_requestQuit = true;

        SyncDebugSnapshot(dbg);
        return true;

    case FlowState::Lose:
        if (confirmPressed || restartPressed) {
            RestartGame();
            m_flowState = FlowState::Playing;
        }
        if (cancelPressed) m_requestQuit = true;

        SyncDebugSnapshot(dbg);
        return true;

    case FlowState::Playing:
    default:
        break;
    }

    if (cancelPressed) {
        m_flowState = FlowState::Paused;
        SyncDebugSnapshot(dbg);
        return true;
    }

    return false;
}

void Game::UpdateRuntimeTimers(float fixedDt) {
    auto tickToZero = [fixedDt](float& value) {
        if (value > 0.0f) {
            value -= fixedDt;
            if (value < 0.0f) value = 0.0f;
        }
    };

    tickToZero(m_speedBuffTimer);
    tickToZero(m_shieldTimer);
    tickToZero(m_toastTimer);
    tickToZero(m_damageFlashTimer);
    tickToZero(m_stunCooldownTimer);
    tickToZero(m_stunPulseTimer);

    for (Entity& e : m_entities) {
        if (!e.active) continue;
        tickToZero(e.stunTimer);
    }
}

void Game::UpdatePlayer(Entity& player, const Input& input, float fixedDt) {
    player.prevPos = player.pos;

    if (player.hitstun <= 0.0f) {
        Vec2 move{ 0.0f, 0.0f };
        if (input.Down(Action::Up)) move.y -= 1.0f;
        if (input.Down(Action::Down)) move.y += 1.0f;
        if (input.Down(Action::Left)) move.x -= 1.0f;
        if (input.Down(Action::Right)) move.x += 1.0f;

        float lenSq = move.x * move.x + move.y * move.y;
        if (lenSq > 0.0001f) {
            float invLen = 1.0f / std::sqrt(lenSq);
            move.x *= invLen;
            move.y *= invLen;
        }

        Vec2 desired = move * m_playerMoveSpeed;
        const float accel = 18.0f;
        player.vel = player.vel + (desired - player.vel) * (accel * fixedDt);
    }

    player.vel = player.vel * (1.0f / (1.0f + m_knockbackDamping * fixedDt));
    player.pos = player.pos + player.vel * fixedDt;

    ClampPlayerToWorld(player);
    m_map.ResolveCircleCollision(player.pos, player.radius);
}

void Game::UpdateEnemies(const Entity& player, float fixedDt) {
    for (Entity& e : m_entities) {
        if (!e.active || e.type != EntityType::Enemy) continue;
        if (e.stunTimer > 0.0f) continue;

        e.prevPos = e.pos;

        if (e.stunTimer > 0.0f) {
            e.vel = { 0.0f, 0.0f };
            continue;
        }

        Vec2 toPlayer = player.pos - e.pos;
        float distSq = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y;
        float aggroSq = e.aggroRadius * e.aggroRadius;

        if (e.ai == AIState::Idle && distSq <= aggroSq) e.ai = AIState::Seek;
        else if (e.ai == AIState::Seek && distSq > aggroSq * 1.2f) e.ai = AIState::Idle;

        if (e.ai != AIState::Seek || distSq <= 0.0001f) continue;

        const float repathInterval = 0.25f;
        const float waypointReach = 8.0f;
        const float enemySpeed = (e.moveSpeed > 0.0f) ? e.moveSpeed : m_enemySpeed;

        TileCoord goalT = m_map.WorldToTile(player.pos);
        e.path.repathTimer -= fixedDt;

        bool goalChanged = (goalT.x != e.path.lastGoalTX || goalT.y != e.path.lastGoalTY);
        bool needPath = e.path.waypoints.empty() || e.path.index >= (int)e.path.waypoints.size();

        if (e.path.repathTimer <= 0.0f && (goalChanged || needPath)) {
            TileCoord startT = m_map.WorldToTile(e.pos);
            auto tiles = Pathfinding::AStar(m_map, startT, goalT);

            e.path.waypoints.clear();
            e.path.index = 0;

            for (const TileCoord& tile : tiles) {
                e.path.waypoints.push_back(m_map.TileToWorldCenter(tile.x, tile.y));
            }
            if (e.path.waypoints.size() > 1) {
                e.path.index = 1;
            }

            e.path.repathTimer = repathInterval;
            e.path.lastGoalTX = goalT.x;
            e.path.lastGoalTY = goalT.y;
        }

        if (!e.path.waypoints.empty() && e.path.index < (int)e.path.waypoints.size()) {
            Vec2 target = e.path.waypoints[e.path.index];
            Vec2 to{ target.x - e.pos.x, target.y - e.pos.y };

            float pathDistSq = to.x * to.x + to.y * to.y;
            if (pathDistSq < waypointReach * waypointReach) {
                e.path.index++;
            }
            else if (pathDistSq > 0.0001f) {
                float invLen = 1.0f / std::sqrt(pathDistSq);
                Vec2 dir{ to.x * invLen, to.y * invLen };

                e.pos = Vec2{
                    e.pos.x + dir.x * (enemySpeed * fixedDt),
                    e.pos.y + dir.y * (enemySpeed * fixedDt)
                };
            }
        }

        m_map.ResolveCircleCollision(e.pos, e.radius);
    }
}

void Game::ResolveEnemySeparation() {
    for (size_t i = 0; i < m_entities.size(); ++i) {
        if (!m_entities[i].active || m_entities[i].type != EntityType::Enemy) continue;

        for (size_t j = i + 1; j < m_entities.size(); ++j) {
            if (!m_entities[j].active || m_entities[j].type != EntityType::Enemy) continue;
            SeparateEntities(m_entities[i], m_entities[j]);
        }
    }
}

void Game::ResolvePlayerEnemyCollisions(Entity& player, float fixedDt, DebugState& dbg) {
    (void)fixedDt;

    for (size_t i = 0; i < m_entities.size(); ++i) {
        if ((int)i == m_playerIndex) continue;

        Entity& e = m_entities[i];
        if (!e.active || e.type != EntityType::Enemy) continue;

        if (CheckCollision(player, e)) {
            SeparateEntities(player, e);

            if (player.invulnTimer <= 0.0f) {
                player.health -= 1;
                m_damageFlashTimer = m_damageFlashDuration;
                m_toastText = "HIT!";
                m_toastTimer = m_toastDuration;
                player.invulnTimer = m_iframesSeconds;
                player.hitstun = m_hitstunSeconds;

                Vec2 d = player.pos - e.pos;
                float distSq = d.x * d.x + d.y * d.y;
                if (distSq < 0.0001f) distSq = 0.0001f;
                float invLen = 1.0f / std::sqrt(distSq);
                Vec2 n{ d.x * invLen, d.y * invLen };

                player.vel = player.vel + n * m_knockbackStrength;

                m_shakeDuration = 0.20f;
                m_shakeTime = m_shakeDuration;
                m_shakeStrength = dbg.shakeStrength;
            }

            ClampPlayerToWorld(player);
            m_map.ResolveCircleCollision(player.pos, player.radius);
        }
    }
}

void Game::HandlePickupCollisions(Entity& player) {
    for (Entity& e : m_entities) {
        if (!e.active || e.type != EntityType::Pickup) continue;
        if (!CheckCollision(player, e)) continue;

        e.active = false;

        switch (e.pickupKind) {
        case PickupKind::Token:
            m_tokensCollected += 1;
            m_toastText = "+TOKEN";
            m_toastTimer = m_toastDuration;
            if (m_tokensCollected > m_tokensTotal) m_tokensCollected = m_tokensTotal;
            m_pickupsRemaining = std::max(0, m_tokensTotal - m_tokensCollected);
            if (m_tokensCollected >= m_tokensTotal && m_tokensTotal > 0) {
                m_flowState = FlowState::Win;
            }
            break;

        case PickupKind::Health:
            if (player.health < m_playerMaxHealth) {
                player.health += 1;
                m_toastText = "+HEALTH";
            }
            else {
                m_toastText = "HEALTH FULL";
            }
            m_toastTimer = m_toastDuration;
            break;

        case PickupKind::Speed:
            m_speedBuffTimer = m_speedBuffDuration;
            m_toastText = "+SPEED";
            m_toastTimer = m_toastDuration;
            break;

        case PickupKind::Shield:
            m_shieldTimer = m_shieldDuration;
            m_toastText = "+SHIELD";
            m_toastTimer = m_toastDuration;
            break;

        default:
            break;
        }
    }
}

bool Game::RenderFlowScreen(SdlPlatform& platform) const {
    switch (m_flowState) {
    case FlowState::Title: {
        const char* items[] = { "START", "CONTROLS", "QUIT" };
        DrawMenuOverlay(platform, m_assets.Font(),
            "MINI ENGINE",
            items, 3, m_titleMenuIndex,
            "W/S: Move Cursor   ENTER: Select",
            "ESC: Quit");
        return true;
    }

    case FlowState::Controls:
        DrawCenteredOverlay(platform, m_assets.Font(),
            "CONTROLS",
            "WASD: Move   TAB: Debug UI",
            "ENTER/ESC: Back");
        return true;

    case FlowState::Paused: {
        const char* items[] = { "RESUME", "RESTART LEVEL", "QUIT TO TITLE", "QUIT GAME" };
        DrawMenuOverlay(platform, m_assets.Font(),
            "PAUSED",
            items, 4, m_pauseMenuIndex,
            "W/S: Move Cursor   ENTER: Select",
            "ESC: Resume");
        return true;
    }

    case FlowState::QuitConfirm: {
        const char* items[] = { "NO", "YES - QUIT" };
        DrawMenuOverlay(platform, m_assets.Font(),
            "QUIT GAME?",
            items, 2, m_quitMenuIndex,
            "W/S: Change   ENTER: Select",
            "ESC: Back");
        return true;
    }

    case FlowState::Win:
        DrawCenteredOverlay(platform, m_assets.Font(),
            "YOU WIN!",
            "ENTER: Next Level",
            "ESC: Quit   R: Restart Level");
        return true;

    case FlowState::Lose:
        DrawCenteredOverlay(platform, m_assets.Font(),
            "YOU LOSE!",
            "ENTER/R: Restart Level",
            "ESC: Quit");
        return true;

    case FlowState::Playing:
    default:
        return false;
    }
}


void Game::Update(SdlPlatform& platform, const Input& input, float fixedDt, DebugState& dbg) {
    if (m_playerIndex < 0 || m_playerIndex >= (int)m_entities.size()) {
        SyncDebugSnapshot(dbg);
        return;
    }

    auto tickCombatTimers = [fixedDt](Entity& e) {
        if (e.hitstun > 0.0f) {
            e.hitstun -= fixedDt;
            if (e.hitstun < 0.0f) e.hitstun = 0.0f;
        }
        if (e.invulnTimer > 0.0f) {
            e.invulnTimer -= fixedDt;
            if (e.invulnTimer < 0.0f) e.invulnTimer = 0.0f;
        }
    };

    for (Entity& e : m_entities) {
        if (!e.active) continue;
        tickCombatTimers(e);
    }

    if (UpdateFlowScreens(input, dbg)) {
        return;
    }

    Entity& player = m_entities[m_playerIndex];

    // Hot reload
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

    if (dbg.requestReloadConfig) {
        dbg.requestReloadConfig = false;
        ReloadConfig("assets/config.json");
    }

    if (input.Pressed(Action::ToggleDebug) && !dbg.imguiWantsKeyboard) {
        dbg.showUI = !dbg.showUI;
    }

    if (dbg.pause) {
        SyncDebugSnapshot(dbg);
        return;
    }

    if (dbg.playerMaxHealth < 1) dbg.playerMaxHealth = 1;
    if (dbg.playerMaxHealth > 10) dbg.playerMaxHealth = 10;
    if (dbg.invulnSeconds < 0.05f) dbg.invulnSeconds = 0.05f;
    if (dbg.invulnSeconds > 3.0f) dbg.invulnSeconds = 3.0f;
    if (dbg.hitKnockback < 0.0f) dbg.hitKnockback = 0.0f;
    if (dbg.hitKnockback > 2000.0f) dbg.hitKnockback = 2000.0f;

    m_playerMaxHealth = dbg.playerMaxHealth;
    m_invulnSeconds = dbg.invulnSeconds;
    m_hitKnockback = dbg.hitKnockback;

    if (player.health > m_playerMaxHealth) player.health = m_playerMaxHealth;
    player.invulnDuration = m_invulnSeconds;

    if (dbg.zoom < 0.5f) dbg.zoom = 0.5f;
    if (dbg.zoom > 2.0f) dbg.zoom = 2.0f;
    m_camera.SetZoom(dbg.zoom);

    UpdateRuntimeTimers(fixedDt);
    UpdatePlayer(player, input, fixedDt);

    if (input.Pressed(Action::Stun) && m_stunCooldownTimer <= 0.0f) {
        m_stunCooldownTimer = m_stunCooldown;
        m_stunPulseTimer = m_stunPulseDuration;
        m_toastText = "STUN!";
        m_toastTimer = m_toastDuration;

        const float stunRadiusSq = m_stunRadius * m_stunRadius;
        for (Entity& e : m_entities) {
            if (!e.active || e.type != EntityType::Enemy) continue;

            Vec2 d = e.pos - player.pos;
            float distSq = d.x * d.x + d.y * d.y;
            if (distSq > stunRadiusSq) continue;

            float stunDuration = m_stunDuration;
            if (e.enemyKind == EnemyKind::Tank) {
                stunDuration *= 0.6f;
            }
            e.stunTimer = std::max(e.stunTimer, stunDuration);
            e.path.waypoints.clear();
            e.path.index = 0;
            e.path.repathTimer = 0.0f;
            e.vel = { 0.0f, 0.0f };
        }
    }
    UpdateEnemies(player, fixedDt);
    ResolveEnemySeparation();
    ResolvePlayerEnemyCollisions(player, fixedDt, dbg);
    HandlePickupCollisions(player);

    if (player.health <= 0) {
        m_flowState = FlowState::Lose;
    }

    UpdateCameraFollow(platform, player);

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

    SyncDebugSnapshot(dbg);
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
    if (RenderFlowScreen(platform)) {
        return;
    }

    if (m_playerIndex < 0 || m_playerIndex >= (int)m_entities.size() || m_requestQuit) {
        return;
    }

    const Entity& player = m_entities[m_playerIndex];
    const auto& playerTex = m_assets.Player();

    if (dbg.showGrid) {
        DrawWorldGrid(platform);
    }

    m_map.Render(platform, m_camera);

    if (m_stunPulseTimer > 0.0f) {
        const float pulseT = 1.0f - (m_stunPulseTimer / std::max(0.001f, m_stunPulseDuration));
        const float ringA = 20.0f + (m_stunRadius * pulseT);
        const float ringB = 10.0f + (m_stunRadius * std::min(1.0f, pulseT + 0.2f));
        DrawPulseDots(platform, m_camera, player.pos, ringA, 6, 80, 220, 255);
        DrawPulseDots(platform, m_camera, player.pos, ringB, 4, 180, 240, 255);
    }

    for (const Entity& e : m_entities) {
        if (!e.active) continue;

        const Vec2 worldPos = e.prevPos + (e.pos - e.prevPos) * alpha;
        const Vec2 screenPos = m_camera.WorldToScreen(worldPos);

        if (e.type == EntityType::Player) {
            if (e.invulnTimer > 0.0f) {
                const int phase = (int)(e.invulnTimer * 20.0f);
                if ((phase & 1) == 0) {
                    continue;
                }
            }

            const int drawX = (int)(screenPos.x - playerTex.Width() * 0.5f);
            const int drawY = (int)(screenPos.y - playerTex.Height() * 0.5f);
            platform.DrawSprite(playerTex, drawX, drawY);
            continue;
        }

        if (e.type == EntityType::Pickup) {
            Vec2 screen = m_camera.WorldToScreen(e.pos);
            switch (e.pickupKind) {
            case PickupKind::Token:  platform.DrawFilledRect((int)screen.x - 8, (int)screen.y - 8, 16, 16, 255, 255, 0); break;
            case PickupKind::Health: platform.DrawFilledRect((int)screen.x - 8, (int)screen.y - 8, 16, 16, 80, 220, 80); break;
            case PickupKind::Speed:  platform.DrawFilledRect((int)screen.x - 8, (int)screen.y - 8, 16, 16, 80, 160, 255); break;
            case PickupKind::Shield: platform.DrawFilledRect((int)screen.x - 8, (int)screen.y - 8, 16, 16, 180, 80, 220); break;
            default:                 platform.DrawFilledRect((int)screen.x - 8, (int)screen.y - 8, 16, 16, 120, 120, 120); break;
            }
            continue;
        }

        if (dbg.showPaths && e.type == EntityType::Enemy) {
            for (int i = e.path.index; i + 1 < (int)e.path.waypoints.size(); ++i) {
                Vec2 a = m_camera.WorldToScreen(e.path.waypoints[i]);
                Vec2 b = m_camera.WorldToScreen(e.path.waypoints[i + 1]);
                platform.DrawLine((int)a.x, (int)a.y, (int)b.x, (int)b.y);
            }
        }

        const int size = (int)(e.radius * 2.0f);
        const int drawX = (int)(screenPos.x - size * 0.5f);
        const int drawY = (int)(screenPos.y - size * 0.5f);

        int r = 200, g = 80, b = 80;
        switch (e.enemyKind) {
        case EnemyKind::Fast: r = 80; g = 200; b = 80; break;
        case EnemyKind::Tank: r = 80; g = 80; b = 200; break;
        default: break;
        }

        platform.DrawFilledRect(drawX, drawY, size, size, (uint8_t)r, (uint8_t)g, (uint8_t)b);

        if (e.stunTimer > 0.0f) {
            platform.DrawFilledRect(drawX + size / 2 - 4, drawY - 10, 8, 8, 80, 220, 255);
        }
    }

    DrawHUD(platform);
    DrawToast(platform);
    DrawDamageFlash(platform);

    if (!m_levelValidationMsg.empty()) {
        platform.DrawTextBMP(m_assets.Font(), 16, 16, m_levelValidationMsg.c_str(), 8, 8, 16, 32, 2);
    }
}

// --------------------
// HUD + feedback
// --------------------
void Game::DrawHUD(SdlPlatform& platform) const
{
    const int glyphW = 8, glyphH = 8, cols = 16, scale = 2;

    // Quick counts (only active enemies)
    int enemyAlive = 0;
    for (const Entity& e : m_entities) {
        if (!e.active) continue;
        if (e.type == EntityType::Enemy) enemyAlive++;
    }

    const Entity& player = m_entities[m_playerIndex];

    char line[128]{};

    int x = 16;
    int y = 16;

    std::snprintf(line, sizeof(line), "LEVEL: %d", m_currentLevel);
    platform.DrawTextBMP(m_assets.Font(), x, y, line, glyphW, glyphH, cols, 32, scale);
    y += glyphH * scale + 6;

    std::snprintf(line, sizeof(line), "HP: %d/%d", player.health, m_playerMaxHealth);
    platform.DrawTextBMP(m_assets.Font(), x, y, line, glyphW, glyphH, cols, 32, scale);
    y += glyphH * scale + 6;

    std::snprintf(line, sizeof(line), "TOKENS: %d/%d", m_tokensCollected, m_tokensTotal);
    platform.DrawTextBMP(m_assets.Font(), x, y, line, glyphW, glyphH, cols, 32, scale);
    y += glyphH * scale + 6;

    std::snprintf(line, sizeof(line), "ENEMIES: %d", enemyAlive);
    platform.DrawTextBMP(m_assets.Font(), x, y, line, glyphW, glyphH, cols, 32, scale);
    y += glyphH * scale + 10;

    // Buff status (keep it simple)
    if (m_speedBuffTimer > 0.0f) {
        std::snprintf(line, sizeof(line), "SPEED: %.1fs", m_speedBuffTimer);
        platform.DrawTextBMP(m_assets.Font(), x, y, line, glyphW, glyphH, cols, 32, scale);
        y += glyphH * scale + 6;
    }

    if (m_shieldTimer > 0.0f) {
        std::snprintf(line, sizeof(line), "SHIELD: %.1fs", m_shieldTimer);
        platform.DrawTextBMP(m_assets.Font(), x, y, line, glyphW, glyphH, cols, 32, scale);
        y += glyphH * scale + 6;
    }

    if (m_stunCooldownTimer <= 0.0f) {
        std::snprintf(line, sizeof(line), "STUN: READY");
    } else {
        std::snprintf(line, sizeof(line), "STUN: %.1fs", m_stunCooldownTimer);
    }
    platform.DrawTextBMP(m_assets.Font(), x, y, line, glyphW, glyphH, cols, 32, scale);
}

void Game::DrawToast(SdlPlatform& platform) const
{
    if (m_toastTimer <= 0.0f || m_toastText.empty())
        return;

    const int glyphW = 8, glyphH = 8, cols = 16, scale = 2;

    int w = 0, h = 0;
    platform.GetWindowSize(w, h);

    // Rough centering (monospace font)
    const int charW = glyphW * scale;
    const int textW = (int)m_toastText.size() * charW;

    const int x = std::max(12, (w - textW) / 2);
    const int y = h - (glyphH * scale) - 24;

    platform.DrawTextBMP(m_assets.Font(), x, y, m_toastText.c_str(), glyphW, glyphH, cols, 32, scale);
}

void Game::DrawDamageFlash(SdlPlatform& platform) const
{
    if (m_damageFlashTimer <= 0.0f)
        return;

    int w = 0, h = 0;
    platform.GetWindowSize(w, h);

    // We don't have alpha in the current draw helper, so do a quick red border flash.
    const int t = 14; // thickness
    platform.DrawFilledRect(0, 0, w, t, 180, 40, 40);          // top
    platform.DrawFilledRect(0, h - t, w, t, 180, 40, 40);      // bottom
    platform.DrawFilledRect(0, 0, t, h, 180, 40, 40);          // left
    platform.DrawFilledRect(w - t, 0, t, h, 180, 40, 40);      // right
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
	// World size comes from the map so bigger/smaller CSVs "just work".
	// Config values are treated as a fallback only (used before a map is loaded).
	if (m_map.Width() <= 0 || m_map.Height() <= 0) {
		m_worldSize = { m_cfg.worldWidth, m_cfg.worldHeight };
	}

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


void Game::UpdateWorldSizeFromMap()
{
    if (m_map.Width() <= 0 || m_map.Height() <= 0) return;
    m_worldSize = {
        m_map.Width() * (float)m_map.TileSize(),
        m_map.Height() * (float)m_map.TileSize()
    };
}

// Quick sanity checks so broken CSVs don't soft-lock you.
// - Ensures tile values are in the known range (0..9)
// - Ensures exactly one player tile (4). Extra spawns are cleared to floor.
// - If no player tile exists, we keep config spawn fallback and warn.
void Game::ValidateAndSanitizeMap()
{
    m_levelValidationMsg.clear();

    if (m_map.Width() <= 0 || m_map.Height() <= 0) {
        m_levelValidationMsg = "Map load failed (0x0). Check file path + CSV format.";
        return;
    }

    int playerCount = 0;
    bool hadBadTiles = false;
    int badTileCount = 0;

    for (int y = 0; y < m_map.Height(); ++y) {
        for (int x = 0; x < m_map.Width(); ++x) {
            const int v = m_map.At(x, y);

            if (v == 4) playerCount++;

            // Known tiles: 0..9 (based on your legend).
            if (v < 0 || v > 9) {
                hadBadTiles = true;
                badTileCount++;
                m_map.SetAt(x, y, 0);
            }
        }
    }

    if (playerCount == 0) {
        m_levelValidationMsg = "No player spawn (4) in this map. Using config fallback spawn.";
    }
    else if (playerCount > 1) {
        // Keep the first, clear the rest so restarts behave predictably.
        bool keptOne = false;
        for (int y = 0; y < m_map.Height(); ++y) {
            for (int x = 0; x < m_map.Width(); ++x) {
                if (m_map.At(x, y) == 4) {
                    if (!keptOne) keptOne = true;
                    else m_map.SetAt(x, y, 0);
                }
            }
        }

        m_levelValidationMsg = "Multiple player spawns (4) found. Keeping the first, clearing the rest.";
    }

    if (hadBadTiles) {
        if (!m_levelValidationMsg.empty()) m_levelValidationMsg += "  ";
        m_levelValidationMsg += "Found invalid tile values; replaced " + std::to_string(badTileCount) + " with floor (0).";
    }
}


bool Game::GenerateProceduralLevel(int levelIndex) {
    const int width = 40;
    const int height = 24;
    std::vector<int> tiles((size_t)width * (size_t)height, 1);
    auto idx = [width](int x, int y) { return y * width + x; };

    std::mt19937 rng((uint32_t)(0xC0FFEEu + (uint32_t)levelIndex * 977u));

    auto carve = [&](int x, int y) {
        tiles[idx(x, y)] = 0;
    };

    // Start with a maze on odd cells so there is always a path structure.
    carve(1, 1);
    std::vector<std::pair<int,int>> stack;
    stack.push_back({1, 1});
    const int dirOrder[4][2] = { {2,0},{-2,0},{0,2},{0,-2} };

    while (!stack.empty()) {
        auto [cx, cy] = stack.back();
        std::array<int,4> order = {0,1,2,3};
        std::shuffle(order.begin(), order.end(), rng);

        bool moved = false;
        for (int oi = 0; oi < 4; ++oi) {
            const int* d = dirOrder[order[oi]];
            int nx = cx + d[0];
            int ny = cy + d[1];
            if (nx <= 0 || ny <= 0 || nx >= width - 1 || ny >= height - 1) continue;
            if (tiles[idx(nx, ny)] == 0) continue;
            carve(cx + d[0] / 2, cy + d[1] / 2);
            carve(nx, ny);
            stack.push_back({nx, ny});
            moved = true;
            break;
        }

        if (!moved) stack.pop_back();
    }

    // Put the player in a central safe room so every level starts readable.
    const int roomHalfW = 3;
    const int roomHalfH = 2;
    const int centerX = width / 2;
    const int centerY = height / 2;
    for (int y = centerY - roomHalfH; y <= centerY + roomHalfH; ++y) {
        for (int x = centerX - roomHalfW; x <= centerX + roomHalfW; ++x) {
            if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1) continue;
            carve(x, y);
        }
    }
    carve(centerX, centerY - roomHalfH - 1);
    carve(centerX, centerY + roomHalfH + 1);
    carve(centerX - roomHalfW - 1, centerY);
    carve(centerX + roomHalfW + 1, centerY);

    const int playerX = centerX;
    const int playerY = centerY;

    // Collect walkable cells and split them into safe / far buckets.
    std::vector<std::pair<int,int>> pickupCells;
    std::vector<std::pair<int,int>> enemyCells;
    std::vector<std::pair<int,int>> farCells;

    auto manhattan = [](int ax, int ay, int bx, int by) { return std::abs(ax - bx) + std::abs(ay - by); };

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            if (tiles[idx(x, y)] != 0) continue;
            if (x == playerX && y == playerY) continue;

            int d = manhattan(x, y, playerX, playerY);
            if (d >= 4) pickupCells.push_back({x, y});
            if (d >= 8) enemyCells.push_back({x, y});
            if (d >= 12) farCells.push_back({x, y});
        }
    }

    auto placeFrom = [&](std::vector<std::pair<int,int>>& cells, int value, int count) {
        for (int i = 0; i < count && !cells.empty(); ++i) {
            std::uniform_int_distribution<int> dist(0, (int)cells.size() - 1);
            int pick = dist(rng);
            auto [x, y] = cells[pick];
            std::swap(cells[pick], cells.back());
            cells.pop_back();
            tiles[idx(x, y)] = value;
        }
    };

    std::shuffle(pickupCells.begin(), pickupCells.end(), rng);
    std::shuffle(enemyCells.begin(), enemyCells.end(), rng);
    std::shuffle(farCells.begin(), farCells.end(), rng);

    // Difficulty ramp matches the earlier CSV rules, just generated now.
    const int basicPickups = std::min((int)pickupCells.size(), 3 + levelIndex * 2);
    const int basicEnemies = std::min((int)enemyCells.size(), 2 + levelIndex);

    int healthCount = 0, speedCount = 0, shieldCount = 0, fastCount = 0, tankCount = 0;
    if (levelIndex >= 4) {
        healthCount = 1 + (levelIndex - 4);
        speedCount = 1 + (levelIndex - 4) / 2;
    }
    if (levelIndex >= 7) {
        shieldCount = 1 + (levelIndex - 7);
        fastCount = 1 + (levelIndex - 7);
        tankCount = 1 + (levelIndex - 7) / 2;
    }

    placeFrom(pickupCells, 2, basicPickups);
    placeFrom(farCells, 5, healthCount);
    placeFrom(farCells, 6, speedCount);
    placeFrom(farCells, 7, shieldCount);

    placeFrom(enemyCells, 3, basicEnemies);
    placeFrom(enemyCells, 8, fastCount);
    placeFrom(enemyCells, 9, tankCount);

    tiles[idx(playerX, playerY)] = 4;

    // Safety net: make sure the level did not end up too cramped.
    const int reachable = CountReachableFloorTiles(tiles, width, height, playerX, playerY);
    if (reachable < 80) {
        m_levelValidationMsg = "Generated level was too cramped. Using a simpler open fallback layout.";
        std::fill(tiles.begin(), tiles.end(), 0);
        for (int x = 0; x < width; ++x) {
            tiles[idx(x, 0)] = 1;
            tiles[idx(x, height - 1)] = 1;
        }
        for (int y = 0; y < height; ++y) {
            tiles[idx(0, y)] = 1;
            tiles[idx(width - 1, y)] = 1;
        }
        // Add a few chunky walls so it is still a level and not just an empty box.
        for (int x = 6; x < width - 6; ++x) {
            if (x == centerX) continue;
            tiles[idx(x, 6)] = 1;
            tiles[idx(x, height - 7)] = 1;
        }
        for (int y = 5; y < height - 5; ++y) {
            if (y == centerY) continue;
            tiles[idx(8, y)] = 1;
            tiles[idx(width - 9, y)] = 1;
        }
        // Restore the safe room and simple content.
        for (int y = centerY - roomHalfH; y <= centerY + roomHalfH; ++y) {
            for (int x = centerX - roomHalfW; x <= centerX + roomHalfW; ++x) tiles[idx(x, y)] = 0;
        }
        tiles[idx(playerX, playerY)] = 4;
        if (centerX + 10 < width - 1) tiles[idx(centerX + 10, centerY)] = 3;
        if (centerX - 8 > 0) tiles[idx(centerX - 8, centerY + 3)] = 2;
        if (levelIndex >= 4 && centerX + 12 < width - 1) tiles[idx(centerX + 12, centerY + 4)] = 5;
        if (levelIndex >= 7 && centerX - 12 > 0) tiles[idx(centerX - 12, centerY - 4)] = 8;
    } else {
        m_levelValidationMsg = "Procedural level generated from seed " + std::to_string(0xC0FFEEu + (uint32_t)levelIndex * 977u) + ".";
    }

    return m_map.LoadFromData(width, height, tiles);
}

void Game::RestartGame() {
	m_gameOver = false;
	m_gameWin = false;

	// Reset runtime counters/state
	m_score = 0;
	m_pickupsRemaining = 0;
	m_tokensCollected = 0;
	m_tokensTotal = 0;

	// Reset camera shake
	m_shakeTime = 0.0f;
	m_shakeDuration = 0.0f;

	// Rebuild ALL entities from the CSV markers each restart.
	m_entities.clear();
	m_playerIndex = 0;
	m_nextEntityId = 1;

	constexpr int kTilePlayer = 4;

	// 1) Find player spawn from map (tile 4). Fallback to config if none.
	Vec2 playerSpawn = m_cfg.playerSpawn;
	bool foundPlayer = false;
	for (int ty = 0; ty < m_map.Height() && !foundPlayer; ++ty) {
		for (int tx = 0; tx < m_map.Width(); ++tx) {
			if (m_map.At(tx, ty) == kTilePlayer) {
				playerSpawn = m_map.TileToWorldCenter(tx, ty);
				foundPlayer = true;
				break;
			}
		}
	}

	// Create player
	CreateEntity(EntityType::Player, playerSpawn, 20.0f);
	Entity& player = m_entities[m_playerIndex];
	player.health = m_playerMaxHealth;
	player.invulnTimer = 0.0f;
	player.invulnDuration = m_invulnSeconds;
	player.pos = playerSpawn;
	player.prevPos = player.pos;

	// 3) Spawn pickups from map (multiple tile IDs)
	//    2 = Token (counts toward win)
	//    5 = Health (+1 heart)
	//    6 = Speed (temporary speed boost)
	//    7 = Shield (one-hit protection)
	constexpr int kTileToken  = 2;
	constexpr int kTileHealth = 5;
	constexpr int kTileSpeed  = 6;
	constexpr int kTileShield = 7;

	m_pickupsRemaining = 0;
	m_tokensCollected = 0;
	for (int ty = 0; ty < m_map.Height(); ++ty) {
		for (int tx = 0; tx < m_map.Width(); ++tx) {
			const int tile = m_map.At(tx, ty);
			if (tile == kTileToken || tile == kTileHealth || tile == kTileSpeed || tile == kTileShield) {
				Vec2 center = m_map.TileToWorldCenter(tx, ty);
				if (tile == kTileToken) {
					SpawnPickupAt(center, PickupKind::Token);
					m_pickupsRemaining++;
				}
				else if (tile == kTileHealth) {
					SpawnPickupAt(center, PickupKind::Health);
				}
				else if (tile == kTileSpeed) {
					SpawnPickupAt(center, PickupKind::Speed);
				}
				else if (tile == kTileShield) {
					SpawnPickupAt(center, PickupKind::Shield);
				}
			}

			if (tile == 3 || tile == 8 || tile == 9) {
				Vec2 center = m_map.TileToWorldCenter(tx, ty);

				Entity& enemy = CreateEntity(EntityType::Enemy, center, 14.0f);
				enemy.enemyKind = EnemyKind::Chaser;
				enemy.moveSpeed = 0.0f; // uses m_enemySpeed

				if (tile == 8) { // Fast
					enemy.enemyKind = EnemyKind::Fast;
					enemy.radius = 12.0f;
					enemy.moveSpeed = m_enemySpeed * 1.6f;
				}
				else if (tile == 9) { // Tank
					enemy.enemyKind = EnemyKind::Tank;
					enemy.radius = 20.0f;
					enemy.moveSpeed = m_enemySpeed * 0.65f;
				}
			}
		}
	}

	m_tokensTotal = m_pickupsRemaining;
}

void Game::SpawnPickupAt(const Vec2& worldPos, PickupKind kind)
{
    Entity& p = CreateEntity(EntityType::Pickup, worldPos, 12.0f);
    p.active = true;
    p.pickupKind = kind;

    // Optional per-kind value (only Token contributes to win/score)
    if (kind == PickupKind::Token) {
        p.value = 1;
    } else {
        p.value = 0;
    }
}

