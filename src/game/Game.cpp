#include "game/Game.h"
#include "platform/SdlPlatform.h"
#include "game/Pathfinding.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <algorithm>
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
	LoadGameConfig(AssetPath("assets/config.json").c_str(), m_cfg);


	// Create player (use config spawn ONCE on init)
	m_entities.clear();
	m_playerIndex = 0;
	CreateEntity(EntityType::Player, m_cfg.playerSpawn, 20.0f);

	RestartGame();

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
	Entity& player = m_entities[m_playerIndex];

	if (m_gameOver || m_gameWin) {
		static bool prevR = false;
		bool rNow = input.Down(Key::R);
		if (rNow && !prevR) {
			RestartGame();
		}
		prevR = rNow;

		dbg.playerPos = player.pos;
		dbg.cameraPos = m_camera.Position();
		return;
	}

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
	m_hitKnockback = dbg.hitKnockback;

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
	// INPUT SYSTEM (player)
	// --------------------
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
			const float repathInterval = 0.25f;  // 4x/sec
			const float waypointReach = 8.0f;
			const float enemySpeed = m_enemySpeed;

			TileCoord goalT = m_map.WorldToTile(player.pos);

			// timers
			e.path.repathTimer -= fixedDt;

			// repath conditions
			bool goalChanged = (goalT.x != e.path.lastGoalTX || goalT.y != e.path.lastGoalTY);
			bool needPath = e.path.waypoints.empty() || e.path.index >= (int)e.path.waypoints.size();

			if (e.path.repathTimer <= 0.0f && (goalChanged || needPath)) {
				TileCoord startT = m_map.WorldToTile(e.pos);

				auto tiles = Pathfinding::AStar(m_map, startT, goalT);
				e.path.waypoints.clear();
				e.path.index = 0;

				for (size_t j = 0; j < tiles.size(); ++j) {
					Vec2 wp = m_map.TileToWorldCenter(tiles[j].x, tiles[j].y);
					e.path.waypoints.push_back(wp);
				}
				if (e.path.waypoints.size() > 1) {
					e.path.index = 1; // skip start tile center
				}

				e.path.repathTimer = repathInterval;
				e.path.lastGoalTX = goalT.x;
				e.path.lastGoalTY = goalT.y;
			}

			// Follow path
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
						e.pos.x + dir.x * (enemySpeed * fixedDt),
						e.pos.y + dir.y * (enemySpeed * fixedDt)
					};
				}
			}

			// Wall collision (keep from sliding through)
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
				if (player.health < 0) player.health = 0;

				player.invulnTimer = m_invulnSeconds;

				// Knockback impulse (pos-based for now)
				player.pos = player.pos + n * (m_hitKnockback * fixedDt);

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
	// PICKUPS (player vs pickups)
	// --------------------
	for (Entity& e : m_entities) {
		if (!e.active) continue;
		if (e.type != EntityType::Pickup) continue;

		if (CheckCollision(player, e)) {
			e.active = false;
			m_score += e.value;

			if (m_pickupsRemaining > 0) {
				m_pickupsRemaining--;
			}

			if (m_pickupsRemaining <= 0) {
				m_gameWin = true;
			}
		}
	}

	//if (m_pickupsRemaining <= 0) {
	//	m_gameWin = true;
	//}
	if (player.health <= 0) {
		m_gameOver = true;
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
	dbg.gameOver = m_gameOver;
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
	if (m_playerIndex < 0 || m_playerIndex >= (int)m_entities.size())
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
			platform.DrawFilledRect((int)screen.x - 4, (int)screen.y - 4, 8, 8, 60, 60, 60);
		}
		else {
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
			platform.DrawFilledRect(drawX, drawY, size, size, 200, 80, 80);
		}
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

	// Score pips (top-left)
// Tokens row (below hearts) using m_score
	const int tokenSize = 18;     // same as hearts
	const int tokenStep = 22;     // same spacing as hearts
	const int tokenX = 16;
	const int tokenY = 16 + tokenStep; // one row below hearts

	for (int i = 0; i < m_score; ++i) {
		platform.DrawFilledRect(tokenX + i * tokenStep, tokenY, tokenSize, tokenSize, 255, 255, 0);
	}

	// --------------------
	// Game Over overlay (no text renderer yet)
	// --------------------
	if (m_gameOver) {
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
	if (m_gameWin) {
		int w = 0, h = 0;
		platform.GetWindowSize(w, h);

		platform.DrawFilledRect(0, 0, w, h, 60, 60, 60); // darken if your draw supports color/alpha
		platform.DrawFilledRect(w / 2 - 220, h / 2 - 40, 440, 80, 60, 60, 60);
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

void Game::RestartGame() {
	m_gameOver = false;

	Entity& player = m_entities[m_playerIndex];
	player.health = m_playerMaxHealth;
	player.invulnTimer = 0.0f;
	player.invulnDuration = m_invulnSeconds;

	player.pos = m_cfg.playerSpawn;
	player.prevPos = player.pos;

	// Reset camera shake
	m_shakeTime = 0.0f;
	m_shakeDuration = 0.0f;

	RespawnEnemiesFromConfig();

	m_score = 0;
	m_pickupsRemaining = 0;
	m_gameWin = false;

	// Create pickups from tile id 2
	for (int ty = 0; ty < m_map.Height(); ++ty) {
		for (int tx = 0; tx < m_map.Width(); ++tx) {
			if (m_map.At(tx, ty) == 2) {
				Vec2 center = m_map.TileToWorldCenter(tx, ty);
				SpawnPickupAt(center);
			}
		}
	}
}

void Game::SpawnPickupAt(const Vec2& worldPos)
{
	Entity& p = CreateEntity(EntityType::Pickup, worldPos, 12.0f);
	p.value = 1;
	p.active = true;

	m_pickupsRemaining++;
}

