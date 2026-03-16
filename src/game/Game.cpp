#include "game/Game.h"
#include "platform/SdlPlatform.h"
#include "game/Pathfinding.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <queue>
#include <random>

#include <engine/Assets.h>
#include "engine/Paths.h"

// Game.cpp keeps the core "world bootstrap" work:
//
// - asset/config setup
// - map validation and procedural generation
// - restart/build-entity logic
//
// The high-churn gameplay sections live in companion .inl files:
// - GameFlow.inl   : title/pause/win/lose/menu flow
// - GameCombat.inl : timers, gameplay update, AI, collisions, pickups
// - GameRender.inl : world rendering, HUD, stun pulse, feedback
//
// This keeps the top-level file focused on the systems that are shared
// by all flows instead of mixing setup, gameplay, and rendering together.

Entity& Game::CreateEntity(EntityType type, Vec2 pos, float radius) {
	Entity e{};
	e.id = m_nextEntityId++;
	e.type = type;
	e.pos = pos;
	e.prevPos = pos;
	e.homePos = pos;
	e.patrolTarget = pos;
	e.radius = radius;
	e.ai = AIState::Idle;
	e.aiTimer = 0.6f;
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



// Flow, gameplay, and rendering are split out so this file stays small.
#include "game/GameFlow.inl"
#include "game/GameCombat.inl"
#include "game/GameRender.inl"

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

static bool IsWalkableProcTile(int tileValue)
{
	// 0 = floor
	// 2 = token
	// 4 = player start
	// 5 = health
	// 6 = speed
	// 7 = shield
	// 8 = fast enemy
	// 9 = tank enemy
	return tileValue == 0 ||
		tileValue == 2 ||
		tileValue == 4 ||
		tileValue == 5 ||
		tileValue == 6 ||
		tileValue == 7 ||
		tileValue == 8 ||
		tileValue == 9 ||
		tileValue == 3;
}

static int CountReachableFloorTiles(const std::vector<int>& tiles, int width, int height, int startX, int startY)
{
	if (width <= 0 || height <= 0) return 0;
	if (startX < 0 || startY < 0 || startX >= width || startY >= height) return 0;

	auto idx = [width](int x, int y) { return y * width + x; };

	if (!IsWalkableProcTile(tiles[idx(startX, startY)]))
		return 0;

	std::vector<unsigned char> visited((size_t)width * (size_t)height, 0);
	std::vector<std::pair<int, int>> open;
	open.push_back({ startX, startY });
	visited[idx(startX, startY)] = 1;

	int count = 0;
	size_t head = 0;

	while (head < open.size())
	{
		const int x = open[head].first;
		const int y = open[head].second;
		++head;
		++count;

		const int dirs[4][2] = {
			{ 1, 0 },
			{ -1, 0 },
			{ 0, 1 },
			{ 0, -1 }
		};

		for (int i = 0; i < 4; ++i)
		{
			const int nx = x + dirs[i][0];
			const int ny = y + dirs[i][1];

			if (nx < 0 || ny < 0 || nx >= width || ny >= height)
				continue;

			const int n = idx(nx, ny);
			if (visited[n])
				continue;

			if (!IsWalkableProcTile(tiles[n]))
				continue;

			visited[n] = 1;
			open.push_back({ nx, ny });
		}
	}

	return count;
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

