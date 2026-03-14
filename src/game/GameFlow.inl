// Split out from Game.cpp so the file is easier to work in.

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


