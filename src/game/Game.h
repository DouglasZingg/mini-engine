#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "engine/Assets.h"
#include "engine/Camera2D.h"
#include "engine/Config.h"
#include "engine/DebugState.h"
#include "engine/Input.h"
#include "engine/Math.h"
#include "game/Entity.h"
#include "game/Tilemap.h"

class SdlPlatform;

// Main game layer: owns world state, gameplay rules, and rendering helpers.
class Game {
public:
    bool Init(SdlPlatform& platform);
    void Update(SdlPlatform& platform, const Input& input, float fixedDt, DebugState& dbg);
    void Render(SdlPlatform& platform, float alpha, const DebugState& dbg);

    bool RequestedQuit() const { return m_requestQuit; }

private:
    // Core world / flow helpers
    void ClampPlayerToWorld(Entity& player) const;
    void UpdateCameraFollow(SdlPlatform& platform, const Entity& player);
    void UpdateWorldSizeFromMap();
    void ValidateAndSanitizeMap();
    bool GenerateProceduralLevel(int levelIndex);
    void RestartGame();


    // Flow / update / render sections live in the companion .inl files.
    void SyncDebugSnapshot(DebugState& dbg) const;
    bool UpdateFlowScreens(const Input& input, DebugState& dbg);
    bool RenderFlowScreen(SdlPlatform& platform) const;

    void UpdateRuntimeTimers(float fixedDt);
    void UpdatePlayer(Entity& player, const Input& input, float fixedDt);
    void UpdateEnemies(const Entity& player, float fixedDt);
    void ResolveEnemySeparation();
    void ResolvePlayerEnemyCollisions(Entity& player, float fixedDt, DebugState& dbg);
    void HandlePickupCollisions(Entity& player);

    // Screen-space helpers
    void DrawWorldGrid(SdlPlatform& platform) const;
    void DrawHUD(SdlPlatform& platform) const;
    void DrawToast(SdlPlatform& platform) const;
    void DrawDamageFlash(SdlPlatform& platform) const;

    // Config / entity helpers
    bool ReloadConfig(const char* path);
    void ApplyConfig(const GameConfig& cfg, bool respawnEnemies);
    void RespawnEnemiesFromConfig();
    Entity& CreateEntity(EntityType type, Vec2 pos, float radius);
    void SpawnPickupAt(const Vec2& worldPos, PickupKind kind);

private:
    Assets m_assets;
    GameConfig m_cfg;
    Camera2D m_camera;
    Tilemap m_map;

    std::vector<Entity> m_entities;
    int m_playerIndex = -1;
    EntityId m_nextEntityId = 1;

    Vec2 m_worldSize{ 2000.0f, 2000.0f };
    std::string m_levelValidationMsg;

    // Player / enemy tuning
    int m_playerMaxHealth = 3;
    float m_invulnSeconds = 0.75f;
    float m_enemySpeed = 120.0f;

    float m_playerMoveSpeed = 260.0f;
    float m_knockbackStrength = 650.0f;
    float m_knockbackDamping = 10.0f;
    float m_hitstunSeconds = 0.12f;
    float m_iframesSeconds = 0.75f;

    // Camera shake
    float m_shakeTime = 0.0f;
    float m_shakeDuration = 0.0f;
    float m_shakeStrength = 0.0f;

    // HUD / feedback
    std::string m_toastText;
    float m_toastTimer = 0.0f;
    float m_toastDuration = 1.0f;
    float m_damageFlashTimer = 0.0f;
    float m_damageFlashDuration = 0.18f;

    // Power-up timers
    float m_speedBuffTimer = 0.0f;
    float m_speedBuffDuration = 3.0f;
    float m_speedMultiplier = 1.5f;

    float m_shieldTimer = 0.0f;
    float m_shieldDuration = 2.5f;

    // Stun pulse
    float m_stunCooldown = 5.0f;
    float m_stunCooldownTimer = 0.0f;
    float m_stunRadius = 120.0f;
    float m_stunDuration = 1.5f;
    float m_stunPulseTimer = 0.0f;
    float m_stunPulseDuration = 0.25f;

    // Config hot reload
    std::filesystem::file_time_type m_cfgTimestamp{};
    float m_cfgPollTimer = 0.0f;

    // Game progression
    int m_pickupsRemaining = 0;
    int m_tokensCollected = 0;
    int m_tokensTotal = 0;
    int m_currentLevel = 1;

    bool m_requestQuit = false;

    enum class FlowState { Title, Controls, Playing, Paused, Win, Lose, QuitConfirm };
    FlowState m_flowState = FlowState::Title;

    // Menu cursors
    int m_titleMenuIndex = 0;
    int m_pauseMenuIndex = 0;
    int m_quitMenuIndex = 0;
    FlowState m_quitReturnState = FlowState::Paused;

    // Procedural dungeon room description.
    struct DungeonRoom {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
    };
};
