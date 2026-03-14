// Split out from Game.cpp so combat/update code is easier to scan.

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

        e.prevPos = e.pos;

        // Stunned enemies are frozen and do not chase.
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
        if (e.stunTimer > 0.0f) continue; // stunned enemies cannot damage the player

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
    m_knockbackStrength = dbg.hitKnockback;

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

