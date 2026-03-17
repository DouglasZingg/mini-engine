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

void Game::SpawnFloatingText(const Vec2& worldPos, const char* text) {
    if (!text || !*text) return;
    if (m_floatingTexts.size() >= 32) {
        m_floatingTexts.erase(m_floatingTexts.begin());
    }

    FloatingText ft{};
    ft.worldPos = worldPos;
    ft.timer = ft.duration;
    ft.text = text;
    m_floatingTexts.push_back(ft);
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

    for (auto it = m_floatingTexts.begin(); it != m_floatingTexts.end();) {
        it->timer -= fixedDt;
        it->worldPos.y -= it->riseSpeed * fixedDt;
        if (it->timer <= 0.0f) {
            it = m_floatingTexts.erase(it);
        } else {
            ++it;
        }
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


static void ClearEnemyPath(Entity& e) {
    e.path.waypoints.clear();
    e.path.index = 0;
    e.path.repathTimer = 0.0f;
    e.path.lastGoalTX = 999999;
    e.path.lastGoalTY = 999999;
}

static float EnemyMoveSpeed(const Entity& e, float defaultSpeed) {
    return (e.moveSpeed > 0.0f) ? e.moveSpeed : defaultSpeed;
}

static float HashToUnitFloat(std::uint32_t seed) {
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return (seed & 0x00FFFFFFu) / 16777215.0f;
}

static float DistanceSq(const Vec2& a, const Vec2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}



static Vec2 PickPatrolTarget(const Entity& e, const Tilemap& map) {
    // If this enemy belongs to a generated room, keep patrol points inside that room.
    if (e.homeRoomIndex >= 0 &&
        e.patrolMinTX >= 0 && e.patrolMinTY >= 0 &&
        e.patrolMaxTX >= e.patrolMinTX && e.patrolMaxTY >= e.patrolMinTY) {
        for (int attempt = 0; attempt < 24; ++attempt) {
            const std::uint32_t seedX =
                e.id * 131u +
                std::uint32_t(e.patrolStep * 53) +
                std::uint32_t(attempt * 17);
            const std::uint32_t seedY =
                e.id * 193u +
                std::uint32_t(e.patrolStep * 97) +
                std::uint32_t(attempt * 29);

            const int tx = e.patrolMinTX +
                int(HashToUnitFloat(seedX) * float((e.patrolMaxTX - e.patrolMinTX) + 1));
            const int ty = e.patrolMinTY +
                int(HashToUnitFloat(seedY) * float((e.patrolMaxTY - e.patrolMinTY) + 1));

            if (!map.IsSolidTile(tx, ty)) {
                return map.TileToWorldCenter(tx, ty);
            }
        }
    }

    const TileCoord home = map.WorldToTile(e.homePos);
    for (int attempt = 0; attempt < 16; ++attempt) {
        const std::uint32_t seedA =
            e.id * 131u +
            std::uint32_t(e.patrolStep * 53) +
            std::uint32_t(attempt * 17);

        const std::uint32_t seedB =
            e.id * 193u +
            std::uint32_t(e.patrolStep * 97) +
            std::uint32_t(attempt * 29);

        const float angle01 = HashToUnitFloat(seedA);
        const float radius01 = HashToUnitFloat(seedB);

        const float angle = angle01 * 6.28318530718f;
        const int radiusTiles = 1 + int(radius01 * float(std::max(1, e.patrolRadiusTiles)));
        const int tx = home.x + int(std::round(std::cos(angle) * float(radiusTiles)));
        const int ty = home.y + int(std::round(std::sin(angle) * float(radiusTiles)));

        if (!map.IsSolidTile(tx, ty)) {
            return map.TileToWorldCenter(tx, ty);
        }
    }

    return e.homePos;
}


static void MoveEnemyDirect(Entity& e, const Vec2& target, float moveSpeed, float fixedDt) {
    Vec2 to{ target.x - e.pos.x, target.y - e.pos.y };
    const float distSq = to.x * to.x + to.y * to.y;
    if (distSq <= 0.0001f) {
        e.vel = { 0.0f, 0.0f };
        return;
    }

    const float invLen = 1.0f / std::sqrt(distSq);
    const Vec2 dir{ to.x * invLen, to.y * invLen };
    e.vel = dir * moveSpeed;
    e.pos = e.pos + e.vel * fixedDt;
}

static void RebuildEnemyPathTo(Entity& e, const Tilemap& map, const Vec2& targetWorld) {
    const TileCoord startT = map.WorldToTile(e.pos);
    const TileCoord goalT = map.WorldToTile(targetWorld);
    const auto tiles = Pathfinding::AStar(map, startT, goalT);

    e.path.waypoints.clear();
    e.path.index = 0;
    for (const TileCoord& tile : tiles) {
        e.path.waypoints.push_back(map.TileToWorldCenter(tile.x, tile.y));
    }
    if (e.path.waypoints.size() > 1) {
        e.path.index = 1;
    }

    e.path.lastGoalTX = goalT.x;
    e.path.lastGoalTY = goalT.y;
}

static void MoveEnemyOnPath(Entity& e, float moveSpeed, float fixedDt) {
    constexpr float kWaypointReach = 8.0f;

    if (e.path.waypoints.empty() || e.path.index >= (int)e.path.waypoints.size()) {
        e.vel = { 0.0f, 0.0f };
        return;
    }

    const Vec2 target = e.path.waypoints[e.path.index];
    const Vec2 to{ target.x - e.pos.x, target.y - e.pos.y };
    const float distSq = to.x * to.x + to.y * to.y;

    if (distSq < kWaypointReach * kWaypointReach) {
        e.path.index++;
        e.vel = { 0.0f, 0.0f };
        return;
    }

    const float invLen = 1.0f / std::sqrt(std::max(distSq, 0.0001f));
    const Vec2 dir{ to.x * invLen, to.y * invLen };
    e.vel = dir * moveSpeed;
    e.pos = e.pos + e.vel * fixedDt;
}

void Game::UpdateEnemies(const Entity& player, float fixedDt) {
    constexpr float kAlertDuration = 0.18f;
    constexpr float kReturnHomeReach = 18.0f;
    constexpr float kPatrolReach = 14.0f;
    constexpr float kDirectSeekRange = 120.0f;

    for (Entity& e : m_entities) {
        if (!e.active || e.type != EntityType::Enemy) continue;

        e.prevPos = e.pos;
        e.vel = { 0.0f, 0.0f };

        const float enemySpeed = EnemyMoveSpeed(e, m_enemySpeed);
        const float distToPlayerSq = DistanceSq(e.pos, player.pos);
        const float aggroSq = e.aggroRadius * e.aggroRadius;

        // Stun always wins over every other state.
        if (e.stunTimer > 0.0f) {
            e.ai = AIState::Stunned;
            ClearEnemyPath(e);
            continue;
        }
        if (e.ai == AIState::Stunned) {
            e.ai = AIState::ReturnHome;
            e.aiTimer = 0.0f;
            ClearEnemyPath(e);
        }

        const bool playerInAggro = distToPlayerSq <= aggroSq;

        // Any calm state can become Alert when the player is detected.
        if (playerInAggro && e.ai != AIState::Seek && e.ai != AIState::Alert) {
            e.ai = AIState::Alert;
            e.aiTimer = kAlertDuration;
            ClearEnemyPath(e);
        }

        switch (e.ai) {
        case AIState::Idle: {
            e.aiTimer -= fixedDt;
            if (e.aiTimer <= 0.0f) {
                e.patrolStep++;
                e.patrolTarget = PickPatrolTarget(e, m_map);
                e.ai = AIState::Patrol;
                e.aiTimer = 1.25f + HashToUnitFloat(e.id * 43u + std::uint32_t(e.patrolStep * 11)) * 0.75f;
            }
            break;
        }

        case AIState::Patrol: {
            e.aiTimer -= fixedDt;

            if (DistanceSq(e.pos, e.patrolTarget) <= kPatrolReach * kPatrolReach || e.aiTimer <= 0.0f) {
                e.ai = AIState::Idle;
                e.aiTimer = 0.5f + HashToUnitFloat(e.id * 97u + std::uint32_t(e.patrolStep * 7)) * 0.8f;
                e.vel = {0.0f,0.0f};
                break;
            }

            const TileCoord patrolTile = m_map.WorldToTile(e.patrolTarget);
            if (m_map.IsSolidTile(patrolTile.x, patrolTile.y)) {
                e.patrolTarget = e.homePos;
            }

            MoveEnemyDirect(e, e.patrolTarget, enemySpeed * 0.65f, fixedDt);
            break;
        }

        case AIState::Alert: {
            e.aiTimer -= fixedDt;
            if (!playerInAggro) {
                e.ai = AIState::ReturnHome;
                ClearEnemyPath(e);
                break;
            }
            if (e.aiTimer <= 0.0f) {
                e.ai = AIState::Seek;
                ClearEnemyPath(e);
            }
            break;
        }

        case AIState::Seek: {
            if (!playerInAggro && distToPlayerSq > aggroSq * 1.35f) {
                e.ai = AIState::ReturnHome;
                ClearEnemyPath(e);
                break;
            }

            if (distToPlayerSq <= kDirectSeekRange * kDirectSeekRange) {
                MoveEnemyDirect(e, player.pos, enemySpeed, fixedDt);
                break;
            }

            const float repathInterval = (e.enemyKind == EnemyKind::Fast) ? 0.18f :
                                         (e.enemyKind == EnemyKind::Tank) ? 0.34f : 0.25f;
            const float repathJitter = HashToUnitFloat(e.id * 211u) * 0.05f;

            const TileCoord goalT = m_map.WorldToTile(player.pos);
            e.path.repathTimer -= fixedDt;

            const bool goalChanged = (goalT.x != e.path.lastGoalTX || goalT.y != e.path.lastGoalTY);
            const bool needPath = e.path.waypoints.empty() || e.path.index >= (int)e.path.waypoints.size();

            if (e.path.repathTimer <= 0.0f && (goalChanged || needPath)) {
                RebuildEnemyPathTo(e, m_map, player.pos);
                e.path.repathTimer = repathInterval + repathJitter;
            }

            MoveEnemyOnPath(e, enemySpeed, fixedDt);
            break;
        }

        case AIState::ReturnHome: {
            if (DistanceSq(e.pos, e.homePos) <= kReturnHomeReach * kReturnHomeReach) {
                e.ai = AIState::Idle;
                e.aiTimer = 0.4f + HashToUnitFloat(e.id * 313u) * 0.8f;
                ClearEnemyPath(e);
                break;
            }

            e.path.repathTimer -= fixedDt;
            const bool needPath = e.path.waypoints.empty() || e.path.index >= (int)e.path.waypoints.size();
            if (e.path.repathTimer <= 0.0f || needPath) {
                RebuildEnemyPathTo(e, m_map, e.homePos);
                e.path.repathTimer = 0.30f + HashToUnitFloat(e.id * 17u) * 0.05f;
            }

            MoveEnemyOnPath(e, enemySpeed * 0.85f, fixedDt);
            break;
        }

        case AIState::Stunned:
            // handled above
            break;
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

void Game::ResolvePlayerEnemyCollisions(Entity& player, SdlPlatform& platform, float fixedDt, DebugState& dbg) {
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
                SpawnFloatingText(player.pos, "HIT!");
                platform.PlayTone(180.0f, 0.12f, 0.22f);
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

void Game::HandlePickupCollisions(Entity& player, SdlPlatform& platform) {
    for (Entity& e : m_entities) {
        if (!e.active || e.type != EntityType::Pickup) continue;
        if (!CheckCollision(player, e)) continue;

        e.active = false;

        switch (e.pickupKind) {
        case PickupKind::Token:
            m_tokensCollected += 1;
            m_toastText = "+TOKEN";
            m_toastTimer = m_toastDuration;
            SpawnFloatingText(e.pos, "+TOKEN");
            platform.PlayTone(740.0f, 0.08f, 0.18f);
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
                SpawnFloatingText(e.pos, "+HEALTH");
                platform.PlayTone(540.0f, 0.10f, 0.18f);
            }
            else {
                m_toastText = "HEALTH FULL";
                SpawnFloatingText(e.pos, "FULL");
                platform.PlayTone(260.0f, 0.07f, 0.14f);
            }
            m_toastTimer = m_toastDuration;
            break;

        case PickupKind::Speed:
            m_speedBuffTimer = m_speedBuffDuration;
            m_toastText = "+SPEED";
            m_toastTimer = m_toastDuration;
            SpawnFloatingText(e.pos, "+SPEED");
            platform.PlayTone(880.0f, 0.09f, 0.18f);
            break;

        case PickupKind::Shield:
            m_shieldTimer = m_shieldDuration;
            m_toastText = "+SHIELD";
            m_toastTimer = m_toastDuration;
            SpawnFloatingText(e.pos, "+SHIELD");
            platform.PlayTone(620.0f, 0.10f, 0.18f);
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

    if (UpdateFlowScreens(platform, input, dbg)) {
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
        SpawnFloatingText(player.pos, "STUN!");
        platform.PlayTone(320.0f, 0.14f, 0.22f);

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
    ResolvePlayerEnemyCollisions(player, platform, fixedDt, dbg);
    HandlePickupCollisions(player, platform);

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

