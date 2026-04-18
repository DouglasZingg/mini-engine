#include "game/Game.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <queue>
#include <random>
#include <string>
#include <utility>

namespace {

bool IsWalkableProcTile(int v) {
    return v != TileValue(TileType::Wall);
}

int CountReachableFloorTiles(const std::vector<int>& tiles, int w, int h, int sx, int sy) {
    if (w <= 0 || h <= 0) return 0;
    if (sx < 0 || sy < 0 || sx >= w || sy >= h) return 0;
    auto idx = [w](int x, int y) { return y * w + x; };
    if (!IsWalkableProcTile(tiles[idx(sx, sy)])) return 0;

    std::vector<unsigned char> seen((size_t)w * (size_t)h, 0);
    std::queue<std::pair<int, int>> q;
    q.push({sx, sy});
    seen[idx(sx, sy)] = 1;
    int count = 0;

    const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
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

} // namespace


bool TileInsideRoomInterior(int roomX, int roomY, int roomW, int roomH, int tx, int ty) {
    return tx >= roomX + 1 && tx < roomX + roomW - 1 && ty >= roomY + 1 && ty < roomY + roomH - 1;
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
// - Ensures tile values are in the known range (Floor..TrapSpent)
// - Ensures exactly one player spawn tile. Extra spawns are cleared to floor.
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

            if (v == TileValue(TileType::PlayerSpawn)) playerCount++;

            if (v < TileValue(TileType::Floor) || v > TileValue(TileType::TrapSpent)) {
                hadBadTiles = true;
                badTileCount++;
                m_map.SetAt(x, y, TileValue(TileType::Floor));
            }
        }
    }

    if (playerCount == 0) {
        m_levelValidationMsg = "No player spawn (PlayerSpawn tile) in this map. Using config fallback spawn.";
    }
    else if (playerCount > 1) {
        // Keep the first, clear the rest so restarts behave predictably.
        bool keptOne = false;
        for (int y = 0; y < m_map.Height(); ++y) {
            for (int x = 0; x < m_map.Width(); ++x) {
                if (m_map.At(x, y) == TileValue(TileType::PlayerSpawn)) {
                    if (!keptOne) keptOne = true;
                    else m_map.SetAt(x, y, TileValue(TileType::Floor));
                }
            }
        }

        m_levelValidationMsg = "Multiple player spawn tiles found. Keeping the first, clearing the rest.";
    }

    if (hadBadTiles) {
        if (!m_levelValidationMsg.empty()) m_levelValidationMsg += "  ";
        m_levelValidationMsg += "Found invalid tile values; replaced " + std::to_string(badTileCount) + " with floor (0).";
    }
}




bool Game::GenerateProceduralLevel(int levelIndex) {
	const int width = 44;
	const int height = 28;
	std::vector<int> tiles((size_t)width * (size_t)height, TileValue(TileType::Wall));
	auto idx = [width](int x, int y) { return y * width + x; };

	const uint32_t seed = 0xD06E1001u + (uint32_t)levelIndex * 977u;
	std::mt19937 rng(seed);

	auto carveFloor = [&](int x, int y) {
		if (x <= 0 || y <= 0 || x >= width - 1 || y >= height - 1) return;
		tiles[idx(x, y)] = TileValue(TileType::Floor);
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
    m_generatedRooms.clear();

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
	tiles[idx(playerX, playerY)] = TileValue(TileType::PlayerSpawn);

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
				placeTileInRoom(room, TileValue(TileType::TokenPickup), used, 4);
			}

			if (healthCount > 0) {
				for (int i = 0; i < useCount(healthCount, 1); ++i) {
					placeTileInRoom(room, TileValue(TileType::HealthPickup), used, 6);
				}
			}

			if (speedCount > 0) {
				for (int i = 0; i < useCount(speedCount, 1); ++i) {
					placeTileInRoom(room, TileValue(TileType::SpeedPickup), used, 6);
				}
			}

			if (shieldCount > 0 && roomType == 2) {
				for (int i = 0; i < useCount(shieldCount, 1); ++i) {
					placeTileInRoom(room, TileValue(TileType::ShieldPickup), used, 8);
				}
			}
		}

		if (roomType == 3 || roomType == 4) {
			const int baseEnemiesHere = (roomType == 4) ? 2 : 1;

			for (int i = 0; i < useCount(basicEnemies, baseEnemiesHere); ++i) {
				placeTileInRoom(room, TileValue(TileType::EnemyChaserSpawn), used, 8);
			}

			if (fastCount > 0) {
				for (int i = 0; i < useCount(fastCount, 1); ++i) {
					placeTileInRoom(room, TileValue(TileType::EnemyFastSpawn), used, 10);
				}
			}

			if (tankCount > 0 && room.w >= 7 && room.h >= 6) {
				for (int i = 0; i < useCount(tankCount, 1); ++i) {
					placeTileInRoom(room, TileValue(TileType::EnemyTankSpawn), used, 10);
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

		if (basicPickups > 0) for (int i = 0; i < useCount(basicPickups, 1); ++i) placeTileInRoom(room, TileValue(TileType::TokenPickup), used, 4);
		if (basicEnemies > 0) for (int i = 0; i < useCount(basicEnemies, 1); ++i) placeTileInRoom(room, TileValue(TileType::EnemyChaserSpawn), used, 8);
		if (healthCount > 0)  for (int i = 0; i < useCount(healthCount, 1); ++i)  placeTileInRoom(room, TileValue(TileType::HealthPickup), used, 6);
		if (speedCount > 0)   for (int i = 0; i < useCount(speedCount, 1); ++i)   placeTileInRoom(room, TileValue(TileType::SpeedPickup), used, 6);
		if (shieldCount > 0)  for (int i = 0; i < useCount(shieldCount, 1); ++i)  placeTileInRoom(room, TileValue(TileType::ShieldPickup), used, 8);
		if (fastCount > 0)    for (int i = 0; i < useCount(fastCount, 1); ++i)    placeTileInRoom(room, TileValue(TileType::EnemyFastSpawn), used, 10);
		if (tankCount > 0)    for (int i = 0; i < useCount(tankCount, 1); ++i)    placeTileInRoom(room, TileValue(TileType::EnemyTankSpawn), used, 10);
	}

	const int reachable = CountReachableFloorTiles(tiles, width, height, playerX, playerY);
	if (reachable < 140) {
		m_levelValidationMsg = "Generated dungeon was too cramped. Using fallback floor.";

		std::fill(tiles.begin(), tiles.end(), TileValue(TileType::Wall));

		ProcRoom a{ 4, 5, 8, 6 };
		ProcRoom b{ width / 2 - 4, height / 2 - 3, 9, 7 };
		ProcRoom c{ width - 12, height - 10, 8, 6 };

		carveRoom(a);
		carveRoom(b);
		carveRoom(c);

		carveCorridor(roomCenterX(a), roomCenterY(a), roomCenterX(b), roomCenterY(b));
		carveCorridor(roomCenterX(b), roomCenterY(b), roomCenterX(c), roomCenterY(c));

		tiles[idx(roomCenterX(b), roomCenterY(b))] = TileValue(TileType::PlayerSpawn);
		tiles[idx(roomCenterX(a), roomCenterY(a))] = TileValue(TileType::TokenPickup);
		tiles[idx(roomCenterX(c), roomCenterY(c))] = TileValue(TileType::EnemyChaserSpawn);

		if (levelIndex >= 4) tiles[idx(roomCenterX(c), roomCenterY(c) - 1)] = TileValue(TileType::HealthPickup);
		if (levelIndex >= 7) tiles[idx(roomCenterX(c) + 1, roomCenterY(c))] = TileValue(TileType::EnemyFastSpawn);
	}
	else {
		m_levelValidationMsg = "Dungeon floor generated from seed " + std::to_string(seed) + ".";
	}

    m_generatedRooms.clear();
    if (reachable < 140) {
        m_generatedRooms.push_back({ 4, 5, 8, 6 });
        m_generatedRooms.push_back({ width / 2 - 4, height / 2 - 3, 9, 7 });
        m_generatedRooms.push_back({ width - 12, height - 10, 8, 6 });
    } else {
        for (const auto& room : rooms) {
            m_generatedRooms.push_back({ room.x, room.y, room.w, room.h });
        }
    }

    for (size_t i = 0; i < m_generatedRooms.size(); ++i) {
        DungeonRoom& room = m_generatedRooms[i];
        room.isStartRoom = (i == 0);
        room.state = room.isStartRoom ? RoomState::Cleared : RoomState::Hidden;
        room.hasCombatEncounter = false;
        room.hasPickupReward = false;
        room.assignedEnemyCount = 0;
        room.assignedPickupCount = 0;
    }
    m_currentRoomIndex = 0;

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

	constexpr TileType kTilePlayer = TileType::PlayerSpawn;

	// 1) Find player spawn from map (tile 4). Fallback to config if none.
	Vec2 playerSpawn = m_cfg.playerSpawn;
	bool foundPlayer = false;
	for (int ty = 0; ty < m_map.Height() && !foundPlayer; ++ty) {
		for (int tx = 0; tx < m_map.Width(); ++tx) {
			if (static_cast<TileType>(m_map.At(tx, ty)) == kTilePlayer) {
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
	constexpr TileType kTileToken  = TileType::TokenPickup;
	constexpr TileType kTileHealth = TileType::HealthPickup;
	constexpr TileType kTileSpeed  = TileType::SpeedPickup;
	constexpr TileType kTileShield = TileType::ShieldPickup;

	m_pickupsRemaining = 0;
	m_tokensCollected = 0;
	for (int ty = 0; ty < m_map.Height(); ++ty) {
		for (int tx = 0; tx < m_map.Width(); ++tx) {
			const TileType tile = static_cast<TileType>(m_map.At(tx, ty));
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

			if (tile == TileType::EnemyChaserSpawn || tile == TileType::EnemyFastSpawn || tile == TileType::EnemyTankSpawn) {
				Vec2 center = m_map.TileToWorldCenter(tx, ty);

				Entity& enemy = CreateEntity(EntityType::Enemy, center, 14.0f);
				enemy.homePos = center;
				enemy.enemyKind = EnemyKind::Chaser;
				enemy.moveSpeed = 0.0f; // uses m_enemySpeed
				enemy.health = 2;

				if (tile == TileType::EnemyFastSpawn) { // Fast
					enemy.enemyKind = EnemyKind::Fast;
					enemy.radius = 12.0f;
					enemy.moveSpeed = m_enemySpeed * 1.6f;
					enemy.health = 1;
				}
				else if (tile == TileType::EnemyTankSpawn) { // Tank
					enemy.enemyKind = EnemyKind::Tank;
					enemy.radius = 20.0f;
					enemy.moveSpeed = m_enemySpeed * 0.65f;
					enemy.health = 3;
				}
			}
		}
	}

	m_tokensTotal = m_pickupsRemaining;
    RefreshRoomAssignments();
    UpdateRoomStateForPlayer(player.pos);
}



int Game::FindRoomIndexForTile(int tx, int ty) const
{
    for (size_t i = 0; i < m_generatedRooms.size(); ++i) {
        if (TileInsideRoomInterior(m_generatedRooms[i].x, m_generatedRooms[i].y, m_generatedRooms[i].w, m_generatedRooms[i].h, tx, ty)) {
            return (int)i;
        }
    }
    return -1;
}

int Game::FindRoomIndexAtWorld(const Vec2& worldPos) const
{
    const TileCoord tc = m_map.WorldToTile(worldPos);
    return FindRoomIndexForTile(tc.x, tc.y);
}

void Game::RefreshRoomAssignments()
{
    for (DungeonRoom& room : m_generatedRooms) {
        room.assignedEnemyCount = 0;
        room.assignedPickupCount = 0;
        room.hasCombatEncounter = false;
        room.hasPickupReward = false;
        if (!room.isStartRoom && room.state != RoomState::Locked) {
            room.state = RoomState::Hidden;
        }
    }

    for (Entity& e : m_entities) {
        if (!e.active) continue;
        const int roomIndex = FindRoomIndexAtWorld(e.pos);
        e.homeRoomIndex = roomIndex;
        if (roomIndex < 0 || roomIndex >= (int)m_generatedRooms.size()) {
            continue;
        }

        DungeonRoom& room = m_generatedRooms[roomIndex];
        if (e.type == EntityType::Enemy) {
            room.assignedEnemyCount += 1;
            room.hasCombatEncounter = true;
            e.active = room.isStartRoom || room.state == RoomState::Revealed || room.state == RoomState::Cleared;
        } else if (e.type == EntityType::Pickup) {
            room.assignedPickupCount += 1;
            room.hasPickupReward = true;
            e.active = room.isStartRoom || room.state == RoomState::Revealed || room.state == RoomState::Cleared;
        }
    }
}

void Game::RevealRoom(int roomIndex)
{
    if (roomIndex < 0 || roomIndex >= (int)m_generatedRooms.size()) {
        return;
    }

    DungeonRoom& room = m_generatedRooms[roomIndex];
    if (room.state == RoomState::Locked || room.state == RoomState::Revealed || room.state == RoomState::Cleared) {
        return;
    }

    room.state = RoomState::Revealed;
    m_toastText = room.hasCombatEncounter ? "ROOM REVEALED" : "ROOM DISCOVERED";
    m_toastTimer = m_toastDuration;

    for (Entity& e : m_entities) {
        if (e.homeRoomIndex == roomIndex &&
            (e.type == EntityType::Enemy || e.type == EntityType::Pickup)) {
            e.active = true;
        }
    }
}

void Game::RefreshCurrentRoomState()
{
    if (m_currentRoomIndex < 0 || m_currentRoomIndex >= (int)m_generatedRooms.size()) {
        return;
    }

    DungeonRoom& room = m_generatedRooms[m_currentRoomIndex];
    if (room.state == RoomState::Locked) {
        return;
    }

    int activeEnemies = 0;
    int activePickups = 0;
    for (const Entity& e : m_entities) {
        if (!e.active) continue;
        if (e.homeRoomIndex != m_currentRoomIndex) continue;
        if (e.type == EntityType::Enemy) ++activeEnemies;
        else if (e.type == EntityType::Pickup) ++activePickups;
    }

    room.assignedEnemyCount = activeEnemies;
    room.assignedPickupCount = activePickups;

    if (activeEnemies == 0 && activePickups == 0) {
        room.state = RoomState::Cleared;
    } else {
        room.state = RoomState::Revealed;
    }
}

void Game::UpdateRoomStateForPlayer(const Vec2& playerPos)
{
    m_currentRoomIndex = FindRoomIndexAtWorld(playerPos);
    if (m_currentRoomIndex < 0 || m_currentRoomIndex >= (int)m_generatedRooms.size()) {
        return;
    }

    const DungeonRoom& room = m_generatedRooms[m_currentRoomIndex];
    if (room.state == RoomState::Locked) {
        return;
    }

    if (room.state == RoomState::Hidden) {
        RevealRoom(m_currentRoomIndex);
    }

    RefreshCurrentRoomState();
}



void Game::SpawnDebugEnemyNearPlayer(EnemyKind kind)
{
    if (m_playerIndex < 0 || m_playerIndex >= (int)m_entities.size()) return;

    const Entity& player = m_entities[m_playerIndex];
    Vec2 spawnPos = player.pos + Vec2{ 96.0f, 0.0f };

    Entity& enemy = CreateEntity(EntityType::Enemy, spawnPos, 14.0f);
    enemy.homePos = spawnPos;
    enemy.enemyKind = kind;
    enemy.moveSpeed = 0.0f;
    enemy.aggroRadius = 350.0f;

    enemy.health = 2;

    if (kind == EnemyKind::Fast) {
        enemy.radius = 12.0f;
        enemy.moveSpeed = m_enemySpeed * 1.6f;
        enemy.health = 1;
    } else if (kind == EnemyKind::Tank) {
        enemy.radius = 20.0f;
        enemy.moveSpeed = m_enemySpeed * 0.65f;
        enemy.health = 3;
    }
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


