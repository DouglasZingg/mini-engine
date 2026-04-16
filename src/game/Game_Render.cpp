#include "game/Game.h"
#include "platform/SdlPlatform.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>

namespace {

void DrawCenteredOverlay(SdlPlatform& platform, const SdlTexture& font,
    const char* title, const char* bodyA, const char* bodyB)
{
    int w = 0, h = 0;
    platform.GetWindowSize(w, h);

    platform.DrawFilledRect(0, 0, w, h, 10, 10, 10);

    const int pw = 760;
    const int ph = 240;
    const int px = (w - pw) / 2;
    const int py = (h - ph) / 2;
    platform.DrawFilledRect(px, py, pw, ph, 30, 30, 30);

    const int glyphW = 8, glyphH = 8, cols = 16, scale = 3;

    int tx = px + 24;
    int ty = py + 24;
    platform.DrawTextBMP(font, tx, ty, title, glyphW, glyphH, cols, 32, scale);

    ty += glyphH * scale * 2;
    platform.DrawTextBMP(font, tx, ty, bodyA, glyphW, glyphH, cols, 32, 2);

    ty += glyphH * 2 * 2;
    platform.DrawTextBMP(font, tx, ty, bodyB, glyphW, glyphH, cols, 32, 2);
}

void DrawMenuOverlay(SdlPlatform& platform, const SdlTexture& font,
    const char* title,
    const char* const* items, int itemCount, int selectedIndex,
    const char* footerA = nullptr, const char* footerB = nullptr)
{
    int w = 0, h = 0;
    platform.GetWindowSize(w, h);

    platform.DrawFilledRect(0, 0, w, h, 10, 10, 10);

    const int pw = 760;
    const int ph = 360;
    const int px = (w - pw) / 2;
    const int py = (h - ph) / 2;
    platform.DrawFilledRect(px, py, pw, ph, 30, 30, 30);

    const int glyphW = 8, glyphH = 8, cols = 16;

    int tx = px + 24;
    int ty = py + 24;

    platform.DrawTextBMP(font, tx, ty, title, glyphW, glyphH, cols, 32, 3);

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

    if (footerA) {
        ty = py + ph - 72;
        platform.DrawTextBMP(font, tx, ty, footerA, glyphW, glyphH, cols, 32, 2);
        if (footerB) {
            ty += glyphH * 2 * 2;
            platform.DrawTextBMP(font, tx, ty, footerB, glyphW, glyphH, cols, 32, 2);
        }
    }
}

void DrawPulseDots(SdlPlatform& platform, const Camera2D& camera, const Vec2& center, float radius, int dotSize, int r, int g, int b) {
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

} // namespace

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

    if (dbg.showUI && !m_generatedRooms.empty()) {
        for (size_t i = 0; i < m_generatedRooms.size(); ++i) {
            const DungeonRoom& room = m_generatedRooms[i];
            int rr = 100, gg = 100, bb = 100;
            switch (room.state) {
            case RoomState::Hidden:   rr = 70;  gg = 70;  bb = 70;  break;
            case RoomState::Revealed: rr = 210; gg = 170; bb = 70;  break;
            case RoomState::Cleared:  rr = 80;  gg = 190; bb = 100; break;
            case RoomState::Locked:   rr = 170; gg = 70;  bb = 70;  break;
            }
            const Vec2 tl = m_camera.WorldToScreen(Vec2{ room.x * (float)m_map.TileSize(), room.y * (float)m_map.TileSize() });
            const int rw = room.w * m_map.TileSize();
            const int rh = room.h * m_map.TileSize();
            platform.DrawLine((int)tl.x, (int)tl.y, (int)tl.x + rw, (int)tl.y);
            platform.DrawLine((int)tl.x, (int)tl.y, (int)tl.x, (int)tl.y + rh);
            platform.DrawLine((int)tl.x + rw, (int)tl.y, (int)tl.x + rw, (int)tl.y + rh);
            platform.DrawLine((int)tl.x, (int)tl.y + rh, (int)tl.x + rw, (int)tl.y + rh);
        }
    }

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



