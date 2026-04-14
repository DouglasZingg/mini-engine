#include "game/Game.h"
#include "platform/SdlPlatform.h"
#include "game/Pathfinding.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <random>

namespace {

bool CheckCollision(const Entity& a, const Entity& b) {
    Vec2 d = a.pos - b.pos;
    float distSq = d.x * d.x + d.y * d.y;
    float r = a.radius + b.radius;
    return distSq <= r * r;
}

void SeparateEntities(Entity& a, Entity& b) {
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

bool FindPatrolGoalNearEnemy(const Tilemap& map, const Entity& enemy, int minRadiusTiles, int maxRadiusTiles, int attemptSeed, TileCoord& outGoal) {
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

} // namespace

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
// Keep legacy flags in sync for any old render paths
	m_gameWin = (m_flowState == FlowState::Win);
	m_gameOver = (m_flowState == FlowState::Lose);

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


