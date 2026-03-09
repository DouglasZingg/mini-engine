// Split out from Game.cpp so render/UI code is easier to scan.

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


