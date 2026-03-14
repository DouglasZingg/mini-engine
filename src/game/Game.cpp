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


static bool FindPatrolGoalNearEnemy(const Tilemap& map, const Entity& enemy, int minRadiusTiles, int maxRadiusTiles, int attemptSeed, TileCoord& outGoal) {
    const TileCoord origin = map.WorldToTile(enemy.homePos);

    std::mt19937 rng((uint32_t)(enemy.id * 9781u + attemptSeed * 131u + origin.x * 17 + origin.y * 31));
    std::uniform_int_distribution<int> distOffset(-maxRadiusTiles, maxRadiusTiles);

    for (int attempt = 0; attempt < 24; ++attempt) {
        const int ox = distOffset(rng);
        const int oy = distOffset(rng);
        const int manhattan = std::abs(ox) + std::abs(oy);
        if (manhattan < minRadiusTiles || manhattan > maxRadiusTiles) continue;

        const int tx = origin.x + ox;
        const int ty = origin.y + oy;
        if (tx <= 0 || ty <= 0 || tx >= map.Width() - 1 || ty >= map.Height() - 1) continue;
        if (map.IsSolidTile(tx, ty)) continue;

        outGoal = TileCoord{ tx, ty };
        return true;
    }

    return false;
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
	e.homePos = pos;
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


void Game::Update(SdlPlatform& platform, const Input& input, float fixedDt, DebugState& dbg) {
	Entity& player = m_entities[m_playerIndex];

	auto TickCombatTimers = [&](Entity& e) {
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
		TickCombatTimers(e);
	}

	// --------------------
// AUTHORITATIVE FLOW INPUT (edge-triggered)
// --------------------
// All flow/menu screens use action presses (not held) so a key doesn't repeat every tick.
const bool upPressed      = input.Pressed(Action::Up);
const bool downPressed    = input.Pressed(Action::Down);
const bool leftPressed    = input.Pressed(Action::Left);
const bool rightPressed   = input.Pressed(Action::Right);
const bool confirmPressed = input.Pressed(Action::Confirm);
const bool cancelPressed  = input.Pressed(Action::Cancel);
const bool restartPressed = input.Pressed(Action::Restart);
const bool debugPressed   = input.Pressed(Action::ToggleDebug);
const bool stunPressed    = input.Pressed(Action::Stun);
	// --------------------
	// FLOW STATE HANDLING
	// --------------------
	// Update() owns state transitions only. Render() owns drawing for these screens.
	if (m_flowState == FlowState::Title) {
        // ---- Title Menu ----
        const int itemCount = 3; // Start / Controls / Quit

        if (upPressed)   m_titleMenuIndex = (m_titleMenuIndex + itemCount - 1) % itemCount;
        if (downPressed) m_titleMenuIndex = (m_titleMenuIndex + 1) % itemCount;

        if (confirmPressed) {
            if (m_titleMenuIndex == 0) {          // Start
                m_currentLevel = 1;
                GenerateProceduralLevel(m_currentLevel);
                ValidateAndSanitizeMap();
                UpdateWorldSizeFromMap();
                RestartGame();
                m_flowState = FlowState::Playing;
            } else if (m_titleMenuIndex == 1) {   // Controls
                m_flowState = FlowState::Controls;
            } else {                               // Quit
                m_requestQuit = true;
            }
        }

        if (cancelPressed) {
            m_requestQuit = true;
        }

        dbg.playerPos = (m_playerIndex >= 0) ? m_entities[m_playerIndex].pos : Vec2{0,0};
        dbg.cameraPos = m_camera.Position();
        return;
    }

    if (m_flowState == FlowState::Controls) {
        // Any confirm/cancel goes back to title.
        if (confirmPressed || cancelPressed) {
            m_flowState = FlowState::Title;
        }
        dbg.playerPos = (m_playerIndex >= 0) ? m_entities[m_playerIndex].pos : Vec2{0,0};
        dbg.cameraPos = m_camera.Position();
        return;
    }

    if (m_flowState == FlowState::Paused) {
        // ---- Pause Menu ----
        const int itemCount = 4; // Resume / Restart / Quit to Title / Quit Game

        if (upPressed)   m_pauseMenuIndex = (m_pauseMenuIndex + itemCount - 1) % itemCount;
        if (downPressed) m_pauseMenuIndex = (m_pauseMenuIndex + 1) % itemCount;

        if (confirmPressed) {
            if (m_pauseMenuIndex == 0) {          // Resume
                m_flowState = FlowState::Playing;
            } else if (m_pauseMenuIndex == 1) {   // Restart
                RestartGame();
                m_flowState = FlowState::Playing;
            } else if (m_pauseMenuIndex == 2) {   // Quit to Title
                RestartGame(); // reset the world so Title is clean next time
                m_flowState = FlowState::Title;
            } else {                               // Quit Game
                m_quitMenuIndex = 0;               // default to "No"
                m_quitReturnState = FlowState::Paused;
                m_flowState = FlowState::QuitConfirm;
            }
        }

        if (cancelPressed) {
            m_flowState = FlowState::Playing;
        }

        dbg.playerPos = (m_playerIndex >= 0) ? m_entities[m_playerIndex].pos : Vec2{0,0};
        dbg.cameraPos = m_camera.Position();
        return;
    }

    if (m_flowState == FlowState::QuitConfirm) {
        // ---- Quit Confirm ----
        const int itemCount = 2; // No / Yes

        if (upPressed || downPressed) m_quitMenuIndex = (m_quitMenuIndex + 1) % itemCount;

        if (confirmPressed) {
            if (m_quitMenuIndex == 0) {           // No
                m_flowState = m_quitReturnState;
            } else {                               // Yes
                m_requestQuit = true;
            }
        }

        if (cancelPressed) {
            m_flowState = m_quitReturnState;
        }

        dbg.playerPos = (m_playerIndex >= 0) ? m_entities[m_playerIndex].pos : Vec2{0,0};
        dbg.cameraPos = m_camera.Position();
        return;
    }
if (m_flowState == FlowState::Win) {
        // Win: Confirm goes next level, Restart restarts, Cancel quits
        if (confirmPressed) {
            m_currentLevel++;
            if (m_currentLevel > 10) m_currentLevel = 1;

            GenerateProceduralLevel(m_currentLevel);
            ValidateAndSanitizeMap();
            UpdateWorldSizeFromMap();

            RestartGame();
            m_flowState = FlowState::Playing;
        }

        if (restartPressed) {
            RestartGame();
            m_flowState = FlowState::Playing;
        }

        if (cancelPressed) {
            m_requestQuit = true;
        }

        dbg.playerPos = (m_playerIndex >= 0) ? m_entities[m_playerIndex].pos : Vec2{0,0};
        dbg.cameraPos = m_camera.Position();
        return;
    }


	if (m_flowState == FlowState::Lose) {
        // Lose: Restart restarts, Cancel quits, Confirm also restarts (feels nice for controllers later)
        if (confirmPressed || restartPressed) {
            RestartGame();
            m_flowState = FlowState::Playing;
        }

        if (cancelPressed) {
            m_requestQuit = true;
        }

        dbg.playerPos = (m_playerIndex >= 0) ? m_entities[m_playerIndex].pos : Vec2{0,0};
        dbg.cameraPos = m_camera.Position();
        return;
    }


	// Playing: Esc pauses
	if (cancelPressed) {
		m_flowState = FlowState::Paused;
		dbg.playerPos = (m_playerIndex >= 0) ? m_entities[m_playerIndex].pos : Vec2{0,0};
		dbg.cameraPos = m_camera.Position();
		return;
	}

// HOT-RELOAD POLLING (runs even if paused)
	// --------------------
	m_cfgPollTimer += fixedDt;
	if (m_cfgPollTimer >= 1.0f) {
		m_cfgPollTimer = 0.0f;
		try {
			auto t = std::filesystem::last_write_time(AssetPath("assets/config.json"));
			if (t != m_cfgTimestamp) {
				m_cfgTimestamp = t;
				ReloadConfig(AssetPath("assets/config.json").c_str());
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
		ReloadConfig(AssetPath("assets/config.json").c_str());
	}

	// --------------------
	// Toggle debug UI with Tab (edge-triggered)
	// --------------------
	// Toggle debug UI (Tab) - action based so it only flips once per press.
	if (debugPressed) {
		if (!dbg.imguiWantsKeyboard) {
			dbg.showUI = !dbg.showUI;
		}
	}


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
	// Combat tuning (from debug UI)
	// --------------------
	if (dbg.playerMaxHealth < 1) dbg.playerMaxHealth = 1;
	if (dbg.playerMaxHealth > 10) dbg.playerMaxHealth = 10;
	if (dbg.invulnSeconds < 0.05f) dbg.invulnSeconds = 0.05f;
	if (dbg.invulnSeconds > 3.0f) dbg.invulnSeconds = 3.0f;
	if (dbg.hitKnockback < 0.0f) dbg.hitKnockback = 0.0f;
	if (dbg.hitKnockback > 2000.0f) dbg.hitKnockback = 2000.0f;

	m_playerMaxHealth = dbg.playerMaxHealth;
	m_invulnSeconds = dbg.invulnSeconds;
	m_knockbackStrength = dbg.hitKnockback;

	// Keep player state sane if tuning changed at runtime
	if (player.health > m_playerMaxHealth) player.health = m_playerMaxHealth;
	player.invulnDuration = m_invulnSeconds;

	if (player.invulnTimer > 0.0f) {
		player.invulnTimer -= fixedDt;
		if (player.invulnTimer < 0.0f) player.invulnTimer = 0.0f;
	}

	// --------------------
	// Apply zoom from UI
	// --------------------
	if (dbg.zoom < 0.5f) dbg.zoom = 0.5f;
	if (dbg.zoom > 2.0f) dbg.zoom = 2.0f;
	m_camera.SetZoom(dbg.zoom);

	// --------------------
	
	// --------------------
	// Power-up timers
	// --------------------
	if (m_speedBuffTimer > 0.0f) {
		m_speedBuffTimer -= fixedDt;
		if (m_speedBuffTimer < 0.0f) m_speedBuffTimer = 0.0f;
	}
	if (m_shieldTimer > 0.0f) {
		m_shieldTimer -= fixedDt;
		if (m_shieldTimer < 0.0f) m_shieldTimer = 0.0f;
	}
	if (m_stunCooldownTimer > 0.0f) {
		m_stunCooldownTimer -= fixedDt;
		if (m_stunCooldownTimer < 0.0f) m_stunCooldownTimer = 0.0f;
	}
	if (m_stunPulseTimer > 0.0f) {
		m_stunPulseTimer -= fixedDt;
		if (m_stunPulseTimer < 0.0f) m_stunPulseTimer = 0.0f;
	}

	// --------------------
	// HUD feedback timers (toast + damage flash)
	// --------------------
	if (m_toastTimer > 0.0f) {
		m_toastTimer -= fixedDt;
		if (m_toastTimer < 0.0f) m_toastTimer = 0.0f;
	}
	if (m_damageFlashTimer > 0.0f) {
		m_damageFlashTimer -= fixedDt;
		if (m_damageFlashTimer < 0.0f) m_damageFlashTimer = 0.0f;
	}


// INPUT SYSTEM (player)
	// --------------------
	player.prevPos = player.pos;

	if (player.hitstun <= 0.0f) {
		Vec2 move{ 0,0 };
		if (input.Down(Action::Up)) move.y -= 1.0f;
		if (input.Down(Action::Down)) move.y += 1.0f;
		if (input.Down(Action::Left)) move.x -= 1.0f;
		if (input.Down(Action::Right)) move.x += 1.0f;

		// normalize if moving diagonally
		float lenSq = move.x * move.x + move.y * move.y;
		if (lenSq > 0.0001f) {
			float invLen = 1.0f / std::sqrt(lenSq);
			move.x *= invLen; move.y *= invLen;
		}

		// desired velocity from input
		Vec2 desired = move * m_playerMoveSpeed;

		// blend toward desired (keeps knockback snappy but controllable)
		const float accel = 18.0f;
		player.vel = player.vel + (desired - player.vel) * (accel * fixedDt);
	}


// Player utility move: short-range stun pulse.
if (stunPressed && m_stunCooldownTimer <= 0.0f) {
	m_stunCooldownTimer = m_stunCooldown;
	m_stunPulseTimer = m_stunPulseDuration;
	m_toastText = "STUN USED";
	m_toastTimer = m_toastDuration;

	for (Entity& e : m_entities) {
		if (!e.active || e.type != EntityType::Enemy) continue;
		Vec2 d = e.pos - player.pos;
		float distSq = d.x * d.x + d.y * d.y;
		if (distSq <= m_stunRadius * m_stunRadius) {
			float stunScale = (e.enemyKind == EnemyKind::Tank) ? 0.65f : 1.0f;
			e.stunTimer = std::max(e.stunTimer, m_stunDuration * stunScale);
			e.path.waypoints.clear();
			e.path.index = 0;
			e.path.repathTimer = 0.0f;
			e.vel = { 0.0f, 0.0f };
		}
	}
}

	// knockback damping (always runs)
	player.vel = player.vel * (1.0f / (1.0f + m_knockbackDamping * fixedDt));

	player.pos = player.pos + player.vel * fixedDt;

	// keep existing
	ClampPlayerToWorld(player);
	m_map.ResolveCircleCollision(player.pos, player.radius);

	
// --------------------
// AI SYSTEM (Idle patrol -> Seek)
// --------------------

for (size_t i = 0; i < m_entities.size(); ++i) {
	Entity& e = m_entities[i];
	if (e.type != EntityType::Enemy) continue;

	e.prevPos = e.pos;
	if (e.stunTimer > 0.0f) {
		e.stunTimer -= fixedDt;
		if (e.stunTimer < 0.0f) e.stunTimer = 0.0f;
		e.vel = { 0.0f, 0.0f };
		continue;
	}

	Vec2 toPlayer = player.pos - e.pos;
	float distSq = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y;
	float aggroSq = e.aggroRadius * e.aggroRadius;

	if (e.ai == AIState::Idle && distSq <= aggroSq) {
		e.ai = AIState::Seek;
		e.path.waypoints.clear();
		e.path.index = 0;
		e.path.repathTimer = 0.0f;
	}
	else if (e.ai == AIState::Seek && distSq > aggroSq * 1.35f) {
		e.ai = AIState::Idle;
		e.path.waypoints.clear();
		e.path.index = 0;
		e.path.repathTimer = 0.0f;
	}

	const float baseSpeed = (e.moveSpeed > 0.0f) ? e.moveSpeed : m_enemySpeed;
	const float waypointReach = 8.0f;

	if (e.ai == AIState::Seek) {
		const float repathInterval =
			(e.enemyKind == EnemyKind::Fast) ? 0.18f :
			(e.enemyKind == EnemyKind::Tank) ? 0.40f : 0.28f;

		e.path.repathTimer -= fixedDt;
		TileCoord goalT = m_map.WorldToTile(player.pos);
		bool goalChanged = (goalT.x != e.path.lastGoalTX || goalT.y != e.path.lastGoalTY);
		bool needPath = e.path.waypoints.empty() || e.path.index >= (int)e.path.waypoints.size();

		// At very close range just move directly instead of over-pathing.
		if (distSq <= 110.0f * 110.0f) {
			if (distSq > 0.0001f) {
				float invLen = 1.0f / std::sqrt(distSq);
				Vec2 dir{ toPlayer.x * invLen, toPlayer.y * invLen };
				e.pos = Vec2{
					e.pos.x + dir.x * (baseSpeed * fixedDt),
					e.pos.y + dir.y * (baseSpeed * fixedDt)
				};
			}
		}
		else {
			if (e.path.repathTimer <= 0.0f && (goalChanged || needPath)) {
				TileCoord startT = m_map.WorldToTile(e.pos);
				auto tiles = Pathfinding::AStar(m_map, startT, goalT);
				e.path.waypoints.clear();
				e.path.index = 0;

				for (size_t j = 0; j < tiles.size(); ++j) {
					e.path.waypoints.push_back(m_map.TileToWorldCenter(tiles[j].x, tiles[j].y));
				}
				if (e.path.waypoints.size() > 1) {
					e.path.index = 1;
				}

				const float jitter = (float)((e.id % 5u) * 0.03f);
				e.path.repathTimer = repathInterval + jitter;
				e.path.lastGoalTX = goalT.x;
				e.path.lastGoalTY = goalT.y;
			}

			if (!e.path.waypoints.empty() && e.path.index < (int)e.path.waypoints.size()) {
				Vec2 target = e.path.waypoints[e.path.index];
				Vec2 to{ target.x - e.pos.x, target.y - e.pos.y };
				float distSq2 = to.x * to.x + to.y * to.y;
				if (distSq2 < waypointReach * waypointReach) {
					e.path.index++;
				}
				else if (distSq2 > 0.0001f) {
					float invLen = 1.0f / std::sqrt(distSq2);
					Vec2 dir{ to.x * invLen, to.y * invLen };
					e.pos = Vec2{
						e.pos.x + dir.x * (baseSpeed * fixedDt),
						e.pos.y + dir.y * (baseSpeed * fixedDt)
					};
				}
			}
		}
	}
	else {
		// Idle state becomes a light patrol / wander so the level feels alive.
		e.path.repathTimer -= fixedDt;
		bool needPatrolPath = e.path.waypoints.empty() || e.path.index >= (int)e.path.waypoints.size();
		if (e.path.repathTimer <= 0.0f || needPatrolPath) {
			TileCoord patrolGoal{};
			if (FindPatrolGoalNearEnemy(m_map, e, 3, 8, (int)(m_currentLevel * 100 + i), patrolGoal)) {
				TileCoord startT = m_map.WorldToTile(e.pos);
				auto tiles = Pathfinding::AStar(m_map, startT, patrolGoal);
				e.path.waypoints.clear();
				e.path.index = 0;
				for (size_t j = 0; j < tiles.size(); ++j) {
					e.path.waypoints.push_back(m_map.TileToWorldCenter(tiles[j].x, tiles[j].y));
				}
				if (e.path.waypoints.size() > 1) {
					e.path.index = 1;
				}
			}
			const float idlePause = 1.0f + (float)((e.id + i) % 4) * 0.25f;
			e.path.repathTimer = idlePause;
		}

		if (!e.path.waypoints.empty() && e.path.index < (int)e.path.waypoints.size()) {
			Vec2 target = e.path.waypoints[e.path.index];
			Vec2 to{ target.x - e.pos.x, target.y - e.pos.y };
			float distSq2 = to.x * to.x + to.y * to.y;
			if (distSq2 < waypointReach * waypointReach) {
				e.path.index++;
			}
			else if (distSq2 > 0.0001f) {
				float invLen = 1.0f / std::sqrt(distSq2);
				Vec2 dir{ to.x * invLen, to.y * invLen };
				const float patrolSpeedScale = (e.enemyKind == EnemyKind::Fast) ? 0.85f :
					(e.enemyKind == EnemyKind::Tank) ? 0.45f : 0.60f;
				e.pos = Vec2{
					e.pos.x + dir.x * (baseSpeed * patrolSpeedScale * fixedDt),
					e.pos.y + dir.y * (baseSpeed * patrolSpeedScale * fixedDt)
				};
			}
		}
	}

	m_map.ResolveCircleCollision(e.pos, e.radius);
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
		if (e.stunTimer > 0.0f) continue;

		if (CheckCollision(player, e)) {
			// Separate both bodies to avoid "sticky" overlap.
			SeparateEntities(player, e);

			// Compute a stable hit normal (from enemy -> player).
			Vec2 d = player.pos - e.pos;
			float distSq = d.x * d.x + d.y * d.y;
			float dist = std::sqrt(std::max(distSq, 0.0001f));
			Vec2 n = d * (1.0f / dist);

			// DAMAGE (only if not invulnerable)
			if (player.invulnTimer <= 0.0f) {
				player.health -= 1;
				m_damageFlashTimer = m_damageFlashDuration;
				m_toastText = "HIT!";
				m_toastTimer = m_toastDuration;
				player.invulnTimer = m_iframesSeconds;
				player.hitstun = m_hitstunSeconds;

				// knockback direction: enemy -> player
				d = player.pos - e.pos;
				distSq = d.x * d.x + d.y * d.y;
				if (distSq < 0.0001f) distSq = 0.0001f;
				float invLen = 1.0f / std::sqrt(distSq);
				Vec2 n1{ d.x * invLen, d.y * invLen };

				// impulse
				player.vel = player.vel + n1 * m_knockbackStrength;

				// Camera shake
				m_shakeDuration = 0.20f;
				m_shakeTime = m_shakeDuration;
				m_shakeStrength = dbg.shakeStrength;
				
			}

			// Keep player valid after collision pushes
			ClampPlayerToWorld(player);
			m_map.ResolveCircleCollision(player.pos, player.radius);
		}


	}

	// --------------------
	// --------------------
	// PICKUPS (player vs pickups)
	// --------------------
	for (Entity& e : m_entities) {
		if (!e.active) continue;
		if (e.type != EntityType::Pickup) continue;

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
			// +1 heart (clamped)
			if (player.health < m_playerMaxHealth) {
				player.health += 1;
				m_toastText = "+HEALTH";
			} else {
				m_toastText = "HEALTH FULL";
			}
			m_toastTimer = m_toastDuration;
			break;
		case PickupKind::Speed:
			// Temporary movement boost
			m_speedBuffTimer = m_speedBuffDuration;
			m_toastText = "+SPEED";
			m_toastTimer = m_toastDuration;
			break;
		case PickupKind::Shield:
			// One-hit protection (timer also useful for UI)
			m_shieldTimer = m_shieldDuration;
			m_toastText = "+SHIELD";
			m_toastTimer = m_toastDuration;
			break;
		default:
			break;
		}
	}

if (player.health <= 0) {
		m_flowState = FlowState::Lose;
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
	dbg.enemyCount = std::max(0, dbg.entityCount - 1);
	dbg.playerPos = player.pos;
	dbg.cameraPos = m_camera.Position();
	dbg.playerHealth = player.health;
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
	// Render() owns drawing for flow screens. Update() owns state transitions.
	if (m_flowState == FlowState::Title) {
        const char* items[] = { "START", "CONTROLS", "QUIT" };
        DrawMenuOverlay(platform, m_assets.Font(),
            "MINI ENGINE",
            items, 3, m_titleMenuIndex,
            "W/S: Move Cursor   ENTER: Select",
            "ESC: Quit"
        );
        return;
    }

    if (m_flowState == FlowState::Controls) {
        // Simple controls page (keep it readable, no fancy layout yet)
        DrawCenteredOverlay(platform, m_assets.Font(),
            "CONTROLS",
            "WASD: Move   SPACE: Stun",
            "TAB: Debug UI   ENTER/ESC: Back"
        );
        return;
    }

    if (m_flowState == FlowState::Paused) {
        const char* items[] = { "RESUME", "RESTART LEVEL", "QUIT TO TITLE", "QUIT GAME" };
        DrawMenuOverlay(platform, m_assets.Font(),
            "PAUSED",
            items, 4, m_pauseMenuIndex,
            "W/S: Move Cursor   ENTER: Select",
            "ESC: Resume"
        );
        return;
    }

    if (m_flowState == FlowState::QuitConfirm) {
        const char* items[] = { "NO", "YES - QUIT" };
        DrawMenuOverlay(platform, m_assets.Font(),
            "QUIT GAME?",
            items, 2, m_quitMenuIndex,
            "W/S: Change   ENTER: Select",
            "ESC: Back"
        );
        return;
    }

    if (m_flowState == FlowState::Win) {
        DrawCenteredOverlay(platform, m_assets.Font(),
            "YOU WIN!",
            "ENTER: Next Level",
            "ESC: Quit   R: Restart Level"
        );
        return;
    }

    if (m_flowState == FlowState::Lose) {
        DrawCenteredOverlay(platform, m_assets.Font(),
            "YOU LOSE!",
            "ENTER/R: Restart Level",
            "ESC: Quit"
        );
        return;
    }

if (m_playerIndex < 0 || m_playerIndex >= (int)m_entities.size())
		return;

	if (m_requestQuit)
		return;

	const Entity& player = m_entities[m_playerIndex];
	const auto& playerTex = m_assets.Player();

	if (dbg.showGrid) {
		DrawWorldGrid(platform);
	}

	// World (tilemap first, then entities)
	m_map.Render(platform, m_camera);

	for (const Entity& e : m_entities) {
		if (!e.active) continue;
		const Vec2 worldPos = e.prevPos + (e.pos - e.prevPos) * alpha;
		const Vec2 screenPos = m_camera.WorldToScreen(worldPos);

		if (e.type == EntityType::Player) {
			// Blink while invulnerable
			if (e.invulnTimer > 0.0f) {
				const int phase = (int)(e.invulnTimer * 20.0f);
				if ((phase & 1) == 0) {
					continue;
				}
			}

			const int drawX = (int)(screenPos.x - playerTex.Width() * 0.5f);
			const int drawY = (int)(screenPos.y - playerTex.Height() * 0.5f);
			platform.DrawSprite(playerTex, drawX, drawY);
		}
		else if (e.type == EntityType::Pickup) {
			Vec2 screen = m_camera.WorldToScreen(e.pos);
			// Color by pickup kind
			switch (e.pickupKind) {
			case PickupKind::Token:  platform.DrawFilledRect((int)screen.x - 8, (int)screen.y - 8, 16, 16, 255, 255, 0); break; // yellow
			case PickupKind::Health: platform.DrawFilledRect((int)screen.x - 8, (int)screen.y - 8, 16, 16,  80, 220, 80); break; // green
			case PickupKind::Speed:  platform.DrawFilledRect((int)screen.x - 8, (int)screen.y - 8, 16, 16,  80, 160, 255); break; // blue
			case PickupKind::Shield: platform.DrawFilledRect((int)screen.x - 8, (int)screen.y - 8, 16, 16, 180,  80, 220); break; // purple
			default:                platform.DrawFilledRect((int)screen.x - 8, (int)screen.y - 8, 16, 16, 120, 120, 120); break;
			}
		}
		else {
			if (dbg.showUI && dbg.showPaths && e.type == EntityType::Enemy) {
				for (int i = e.path.index; i + 1 < (int)e.path.waypoints.size(); ++i) {
					Vec2 a = m_camera.WorldToScreen(e.path.waypoints[i]);
					Vec2 b = m_camera.WorldToScreen(e.path.waypoints[i + 1]);
					platform.DrawLine((int)a.x, (int)a.y, (int)b.x, (int)b.y);
				}
			}

			const int size = (int)(e.radius * 2.0f);
			const int drawX = (int)(screenPos.x - size * 0.5f);
			const int drawY = (int)(screenPos.y - size * 0.5f);
			int r = 200, g = 80, b = 80; // chaser default
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
	}




// Stun pulse visual so the player can read the ability clearly.
if (m_stunPulseTimer > 0.0f) {
	const float t = 1.0f - (m_stunPulseTimer / m_stunPulseDuration);
	const float radius = std::max(8.0f, m_stunRadius * t);
	DrawPulseDots(platform, m_camera, player.pos, radius, 6, 80, 220, 255);
}

	// --------------------
	// HUD (screen-space)
	// --------------------
	const int maxH = std::max(1, dbg.playerMaxHealth);
	const int curH = std::max(0, std::min(player.health, maxH));

	int x = 16, y = 16;
	for (int i = 0; i < curH; ++i) {
		platform.DrawFilledRect(x + i * 22, y, 18, 18, 220, 60, 60);
	}
	for (int i = curH; i < maxH; ++i) {
		platform.DrawFilledRect(x + i * 22, y, 18, 18, 60, 60, 60);
	}

	// Tokens row (below hearts)
	// Shows collected (yellow) and remaining (gray) tokens.
	const int tokenSize = 18;     // same as hearts
	const int tokenStep = 22;     // same spacing as hearts
	const int tokenX = 16;
	const int tokenY = 16 + tokenStep; // one row below hearts

	const int totalTokens = std::max(0, m_tokensTotal);
	const int collected = std::max(0, std::min(m_tokensCollected, totalTokens));

	for (int i = 0; i < totalTokens; ++i) {
		if (i < collected) {
			platform.DrawFilledRect(tokenX + i * tokenStep, tokenY, tokenSize, tokenSize, 255, 255, 0);
		} else {
			platform.DrawFilledRect(tokenX + i * tokenStep, tokenY, tokenSize, tokenSize, 60, 60, 60);
		}
	}

	// Buff indicators (same size as hearts/tokens)
	const int buffY = tokenY + tokenStep;
	// Speed
	if (m_speedBuffTimer > 0.0f) platform.DrawFilledRect(tokenX, buffY, tokenSize, tokenSize, 80, 160, 255);
	else                         platform.DrawFilledRect(tokenX, buffY, tokenSize, tokenSize, 40, 40, 40);
	// Shield
	if (m_shieldTimer > 0.0f)    platform.DrawFilledRect(tokenX + tokenStep, buffY, tokenSize, tokenSize, 180, 80, 220);
	else                         platform.DrawFilledRect(tokenX + tokenStep, buffY, tokenSize, tokenSize, 40, 40, 40);

	if (m_flowState == FlowState::QuitConfirm) {
		int w = 0, h = 0;
		platform.GetWindowSize(w, h);

		// Dim background
		platform.DrawFilledRect(0, 0, w, h, 10, 10, 10);

		// Center box
		const int bw = 560;
		const int bh = 120;
		platform.DrawFilledRect((w - bw) / 2, (h - bh) / 2, bw, bh, 40, 40, 40);

		// Two hint bars (no text renderer yet)
		platform.DrawFilledRect((w - 380) / 2, (h - bh) / 2 + 20, 380, 24, 70, 70, 70);   // "Quit? Enter"
		platform.DrawFilledRect((w - 380) / 2, (h - bh) / 2 + 60, 380, 24, 70, 70, 70);   // "Esc to cancel"
		return;
		}

// --------------------
	// Game Over overlay (no text renderer yet)
	// --------------------
	if (m_flowState == FlowState::Lose) {
		int w = 0, h = 0;
		platform.GetWindowSize(w, h);

		// Dim background
		platform.DrawFilledRect(0, 0, w, h, 20, 20, 20);

		// Big red banner
		const int bw = 520;
		const int bh = 90;
		platform.DrawFilledRect((w - bw) / 2, (h - bh) / 2, bw, bh, 180, 40, 40);

		// "Press R" hint bar
		const int hw = 320;
		const int hh = 22;
		platform.DrawFilledRect((w - hw) / 2, (h - bh) / 2 + bh + 18, hw, hh, 80, 80, 80);
	}
	if (m_flowState == FlowState::Win) {
		int w = 0, h = 0;
		platform.GetWindowSize(w, h);

		platform.DrawFilledRect(0, 0, w, h, 60, 60, 60); // darken if your draw supports color/alpha
		platform.DrawFilledRect(w / 2 - 220, h / 2 - 40, 440, 80, 60, 60, 60);
	}


	// --------------------
	// HUD + feedback (only during gameplay)
	// --------------------
	DrawHUD(platform);
	DrawToast(platform);
	DrawDamageFlash(platform);


	// If the map had issues, show a small hint so you notice immediately.
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

	m_playerMoveSpeed = m_cfg.playerSpeed;
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
	const int width = 44;
	const int height = 28;
	std::vector<int> tiles((size_t)width * (size_t)height, 1);
	auto idx = [width](int x, int y) { return y * width + x; };

	const uint32_t seed = 0xD06E1001u + (uint32_t)levelIndex * 977u;
	std::mt19937 rng(seed);

	auto carveFloor = [&](int x, int y) {
		if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1) return;
		tiles[idx(x, y)] = 0;
		};

	struct ProcRoom {
		int x, y, w, h;
	};

	auto roomCenterX = [](const ProcRoom& r) { return r.x + r.w / 2; };
	auto roomCenterY = [](const ProcRoom& r) { return r.y + r.h / 2; };

	auto roomsOverlapPadded = [](const ProcRoom& a, const ProcRoom& b, int pad) {
		return !(a.x + a.w + pad <= b.x || b.x + b.w + pad <= a.x ||
			a.y + a.h + pad <= b.y || b.y + b.h + pad <= a.y);
		};

	auto carveRoom = [&](const ProcRoom& r) {
		for (int y = r.y; y < r.y + r.h; ++y) {
			for (int x = r.x; x < r.x + r.w; ++x) {
				carveFloor(x, y);
			}
		}
		};

	auto carveDoor = [&](int x, int y) {
		carveFloor(x, y);
		if (x > 1) carveFloor(x - 1, y);
		if (x < width - 2) carveFloor(x + 1, y);
		if (y > 1) carveFloor(x, y - 1);
		if (y < height - 2) carveFloor(x, y + 1);
		};

	auto carveCorridor = [&](int x0, int y0, int x1, int y1) {
		int x = x0;
		int y = y0;
		carveFloor(x, y);

		const bool horizFirst = ((x0 + y0 + x1 + y1) & 1) == 0;

		if (horizFirst) {
			while (x != x1) {
				x += (x1 > x) ? 1 : -1;
				carveFloor(x, y);
			}
			while (y != y1) {
				y += (y1 > y) ? 1 : -1;
				carveFloor(x, y);
			}
		}
		else {
			while (y != y1) {
				y += (y1 > y) ? 1 : -1;
				carveFloor(x, y);
			}
			while (x != x1) {
				x += (x1 > x) ? 1 : -1;
				carveFloor(x, y);
			}
		}
		};

	auto placeTileInRoom = [&](const ProcRoom& room, int tileValue,
		std::vector<std::pair<int, int>>& used,
		int minDistFromStart) -> bool
		{
			std::vector<std::pair<int, int>> candidates;
			const int startCx = width / 2;
			const int startCy = height / 2;

			for (int y = room.y + 1; y < room.y + room.h - 1; ++y) {
				for (int x = room.x + 1; x < room.x + room.w - 1; ++x) {
					if (!IsWalkableProcTile(tiles[idx(x, y)])) continue;

					bool taken = false;
					for (const auto& p : used) {
						if (p.first == x && p.second == y) {
							taken = true;
							break;
						}
					}
					if (taken) continue;

					const int manhattan = std::abs(x - startCx) + std::abs(y - startCy);
					if (manhattan < minDistFromStart) continue;

					candidates.push_back({ x, y });
				}
			}

			if (candidates.empty()) return false;

			std::uniform_int_distribution<int> pickDist(0, (int)candidates.size() - 1);
			const auto p = candidates[pickDist(rng)];
			tiles[idx(p.first, p.second)] = tileValue;
			used.push_back(p);
			return true;
		};

	std::vector<ProcRoom> rooms;

	// Central start room. This is your safe "party enters the dungeon floor" room.
	ProcRoom startRoom{ width / 2 - 4, height / 2 - 3, 9, 7 };
	rooms.push_back(startRoom);
	carveRoom(startRoom);

	// Build a dungeon floor out of square/rectangular rooms.
	std::uniform_int_distribution<int> wDist(6, 10);
	std::uniform_int_distribution<int> hDist(5, 9);
	std::uniform_int_distribution<int> xDist(2, width - 12);
	std::uniform_int_distribution<int> yDist(2, height - 11);

	for (int attempt = 0; attempt < 90 && rooms.size() < 11; ++attempt) {
		ProcRoom r;
		r.w = wDist(rng);
		r.h = hDist(rng);
		r.x = xDist(rng);
		r.y = yDist(rng);

		bool overlaps = false;
		for (const auto& other : rooms) {
			if (roomsOverlapPadded(r, other, 1)) {
				overlaps = true;
				break;
			}
		}
		if (overlaps) continue;

		rooms.push_back(r);
		carveRoom(r);
	}

	// Connect every room to the nearest already-placed room.
	// This keeps the layout readable and dungeon-like instead of maze-like.
	for (size_t i = 1; i < rooms.size(); ++i) {
		int bestIndex = 0;
		int bestDist = 1 << 30;

		for (size_t j = 0; j < i; ++j) {
			const int dx = roomCenterX(rooms[i]) - roomCenterX(rooms[j]);
			const int dy = roomCenterY(rooms[i]) - roomCenterY(rooms[j]);
			const int dist = dx * dx + dy * dy;
			if (dist < bestDist) {
				bestDist = dist;
				bestIndex = (int)j;
			}
		}

		const int ax = roomCenterX(rooms[i]);
		const int ay = roomCenterY(rooms[i]);
		const int bx = roomCenterX(rooms[bestIndex]);
		const int by = roomCenterY(rooms[bestIndex]);

		carveDoor(ax, ay);
		carveDoor(bx, by);
		carveCorridor(ax, ay, bx, by);
	}

	// Add a few loops so the floor feels more like a dungeon level
	// and can support later hidden doors / side connections.
	if (rooms.size() >= 5) {
		std::uniform_int_distribution<int> roomPick(0, (int)rooms.size() - 1);
		for (int i = 0; i < 3; ++i) {
			int a = roomPick(rng);
			int b = roomPick(rng);
			if (a == b) continue;

			carveCorridor(
				roomCenterX(rooms[a]), roomCenterY(rooms[a]),
				roomCenterX(rooms[b]), roomCenterY(rooms[b]));
		}
	}

	// Player start
	const int playerX = roomCenterX(startRoom);
	const int playerY = roomCenterY(startRoom);
	tiles[idx(playerX, playerY)] = 4;

	auto classifyRoom = [&](size_t roomIndex) {
		if (roomIndex == 0) return 0; // start
		std::uniform_int_distribution<int> rollDist(0, 99);
		int roll = rollDist(rng);

		if (roll < 20) return 1; // empty
		if (roll < 40) return 2; // reward
		if (roll < 75) return 3; // mixed
		return 4;                // combat
		};

	int basicEnemies = 2 + levelIndex;
	int basicPickups = 3 + levelIndex * 2;

	int healthCount = 0;
	int speedCount = 0;
	int shieldCount = 0;
	int fastCount = 0;
	int tankCount = 0;

	if (levelIndex >= 4) {
		healthCount = 1 + (levelIndex - 4);
		speedCount = 1 + (levelIndex - 4) / 2;
	}

	if (levelIndex >= 7) {
		shieldCount = 1 + (levelIndex - 7);
		fastCount = 1 + (levelIndex - 7);
		tankCount = 1 + (levelIndex - 7) / 2;
	}

	auto useCount = [](int& total, int want) {
		int n = std::min(total, want);
		total -= n;
		return n;
		};

	// Populate rooms by room role.
	for (size_t ri = 1; ri < rooms.size(); ++ri) {
		const ProcRoom& room = rooms[ri];
		std::vector<std::pair<int, int>> used;
		const int roomType = classifyRoom(ri);

		if (roomType == 2 || roomType == 3) {
			const int tokensHere = (roomType == 2) ? 1 : 1 + ((int)ri & 1);
			for (int i = 0; i < useCount(basicPickups, tokensHere); ++i) {
				placeTileInRoom(room, 2, used, 4);
			}

			if (healthCount > 0) {
				for (int i = 0; i < useCount(healthCount, 1); ++i) {
					placeTileInRoom(room, 5, used, 6);
				}
			}

			if (speedCount > 0) {
				for (int i = 0; i < useCount(speedCount, 1); ++i) {
					placeTileInRoom(room, 6, used, 6);
				}
			}

			if (shieldCount > 0 && roomType == 2) {
				for (int i = 0; i < useCount(shieldCount, 1); ++i) {
					placeTileInRoom(room, 7, used, 8);
				}
			}
		}

		if (roomType == 3 || roomType == 4) {
			const int baseEnemiesHere = (roomType == 4) ? 2 : 1;

			for (int i = 0; i < useCount(basicEnemies, baseEnemiesHere); ++i) {
				placeTileInRoom(room, 3, used, 8);
			}

			if (fastCount > 0) {
				for (int i = 0; i < useCount(fastCount, 1); ++i) {
					placeTileInRoom(room, 8, used, 10);
				}
			}

			if (tankCount > 0 && room.w >= 7 && room.h >= 6) {
				for (int i = 0; i < useCount(tankCount, 1); ++i) {
					placeTileInRoom(room, 9, used, 10);
				}
			}
		}
	}

	// Spill leftovers into non-start rooms if the random floor rolled too many empty rooms.
	for (size_t ri = 1; ri < rooms.size() &&
		(basicPickups > 0 || basicEnemies > 0 || healthCount > 0 ||
			speedCount > 0 || shieldCount > 0 || fastCount > 0 || tankCount > 0); ++ri)
	{
		std::vector<std::pair<int, int>> used;
		const ProcRoom& room = rooms[ri];

		if (basicPickups > 0) for (int i = 0; i < useCount(basicPickups, 1); ++i) placeTileInRoom(room, 2, used, 4);
		if (basicEnemies > 0) for (int i = 0; i < useCount(basicEnemies, 1); ++i) placeTileInRoom(room, 3, used, 8);
		if (healthCount > 0)  for (int i = 0; i < useCount(healthCount, 1); ++i)  placeTileInRoom(room, 5, used, 6);
		if (speedCount > 0)   for (int i = 0; i < useCount(speedCount, 1); ++i)   placeTileInRoom(room, 6, used, 6);
		if (shieldCount > 0)  for (int i = 0; i < useCount(shieldCount, 1); ++i)  placeTileInRoom(room, 7, used, 8);
		if (fastCount > 0)    for (int i = 0; i < useCount(fastCount, 1); ++i)    placeTileInRoom(room, 8, used, 10);
		if (tankCount > 0)    for (int i = 0; i < useCount(tankCount, 1); ++i)    placeTileInRoom(room, 9, used, 10);
	}

	const int reachable = CountReachableFloorTiles(tiles, width, height, playerX, playerY);
	if (reachable < 140) {
		m_levelValidationMsg = "Generated dungeon was too cramped. Using fallback floor.";

		std::fill(tiles.begin(), tiles.end(), 1);

		ProcRoom a{ 4, 5, 8, 6 };
		ProcRoom b{ width / 2 - 4, height / 2 - 3, 9, 7 };
		ProcRoom c{ width - 12, height - 10, 8, 6 };

		carveRoom(a);
		carveRoom(b);
		carveRoom(c);

		carveCorridor(roomCenterX(a), roomCenterY(a), roomCenterX(b), roomCenterY(b));
		carveCorridor(roomCenterX(b), roomCenterY(b), roomCenterX(c), roomCenterY(c));

		tiles[idx(roomCenterX(b), roomCenterY(b))] = 4;
		tiles[idx(roomCenterX(a), roomCenterY(a))] = 2;
		tiles[idx(roomCenterX(c), roomCenterY(c))] = 3;

		if (levelIndex >= 4) tiles[idx(roomCenterX(c), roomCenterY(c) - 1)] = 5;
		if (levelIndex >= 7) tiles[idx(roomCenterX(c) + 1, roomCenterY(c))] = 8;
	}
	else {
		m_levelValidationMsg = "Dungeon floor generated from seed " + std::to_string(seed) + ".";
	}

	return m_map.LoadFromData(width, height, tiles);
}


void Game::RestartGame() {
	// Reset runtime counters/state
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
				enemy.homePos = center;
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

