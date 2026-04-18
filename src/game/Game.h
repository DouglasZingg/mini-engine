#pragma once
#include "engine/Assets.h"
#include "engine/Camera2D.h"
#include "engine/Config.h"
#include "engine/Input.h"
#include "engine/Math.h"
#include "game/Entity.h"
#include "engine/DebugState.h"
#include "game/Tilemap.h"
#include <filesystem>
#include <vector>
#include <string>
using EntityId = uint32_t;
class SdlPlatform;

/**
 * Game layer (rules + world state).
 * Owns entities and drives simulation/rendering using platform services.
 */
class Game {
public:
    bool Init(SdlPlatform& platform);

    // Fixed-step simulation update.
    void Update(SdlPlatform& platform, const Input& input, float fixedDt, DebugState& dbg);
    void Render(SdlPlatform& platform, float alpha, const DebugState& dbg);
    
    bool RequestedQuit() const { return m_requestQuit; }

private:
    void ClampPlayerToWorld(Entity& player) const;
    void UpdateCameraFollow(SdlPlatform& platform, const Entity& player);
    void UpdateWorldSizeFromMap();
    void ValidateAndSanitizeMap();
    bool GenerateProceduralLevel(int levelIndex);
    void DrawWorldGrid(SdlPlatform& platform) const;
    void RestartGame();

    // HUD + quick feedback
    void DrawHUD(SdlPlatform& platform) const;
    void DrawToast(SdlPlatform& platform) const;
    void DrawDamageFlash(SdlPlatform& platform) const;

private:
    Assets     m_assets;
    GameConfig m_cfg;
    Camera2D   m_camera;

    Vec2  m_worldSize{ 2000.0f, 2000.0f };
    std::string m_levelValidationMsg; // set after map load (shown on title + debug)

    float m_playerSpeed = 220.0f; // pixels/sec for now

    // Combat tuning (driven by DebugState)
    int   m_playerMaxHealth = 3;
    float m_hitKnockback = 280.0f;
    float m_invulnSeconds = 0.75f;

    std::vector<Entity> m_entities;
    int m_playerIndex = -1;

    float m_debugTimer = 0.0f;

    bool m_showDebug = true;

    float m_shakeTime = 0.0f;
    float m_shakeDuration = 0.0f;
    float m_shakeStrength = 0.0f;

    // HUD / feedback (kept tiny on purpose)
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

    // Player utility move: short-range stun pulse.
    float m_stunCooldown = 5.0f;
    float m_stunCooldownTimer = 0.0f;
    float m_stunRadius = 120.0f;
    float m_stunDuration = 1.5f;
    float m_stunPulseTimer = 0.0f;
    float m_stunPulseDuration = 0.25f;

    std::filesystem::file_time_type m_cfgTimestamp{};
    float m_cfgPollTimer = 0.0f;

    float m_enemySpeed = 120.0f;

    bool ReloadConfig(const char* path);
    void ApplyConfig(const GameConfig& cfg, bool respawnEnemies);
    void RespawnEnemiesFromConfig();

    EntityId m_nextEntityId = 1;
    Entity& CreateEntity(EntityType type, Vec2 pos, float radius);

    Tilemap m_map;

    bool m_gameOver = false;
    int  m_score = 0;
    int  m_pickupsRemaining = 0;
    bool m_gameWin = false;

    void SpawnPickupAt(const Vec2& worldPos, PickupKind kind);
    void SpawnDebugEnemyNearPlayer(EnemyKind kind);

    int m_tokensCollected = 0;
    int m_tokensTotal = 0;

    int m_currentLevel = 1;

    bool m_requestQuit = false;
    
    enum class FlowState { Title, Controls, Playing, Paused, Win, Lose, QuitConfirm };
    FlowState m_flowState = FlowState::Title;

    // simple menu cursors
    int m_titleMenuIndex = 0;
    int m_pauseMenuIndex = 0;
    int m_quitMenuIndex = 0;
    FlowState m_quitReturnState = FlowState::Paused;


    // player
    float m_playerMoveSpeed = 260.0f;
    float m_knockbackStrength = 650.0f;   // impulse strength
    float m_knockbackDamping = 10.0f;     // higher = stops faster
    float m_hitstunSeconds = 0.12f;
    float m_iframesSeconds = 0.75f;

    // enemies
    float m_enemyKnockbackStrength = 450.0f;
    float m_enemyHitstunSeconds = 0.08f;

    enum class RoomState { Hidden, Revealed, Cleared, Locked };

    struct DungeonRoom
    {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
        RoomState state = RoomState::Hidden;
        bool isStartRoom = false;
        bool hasCombatEncounter = false;
        bool hasPickupReward = false;
        int assignedEnemyCount = 0;
        int assignedPickupCount = 0;
    };

    int FindRoomIndexForTile(int tx, int ty) const;
    int FindRoomIndexAtWorld(const Vec2& worldPos) const;
    void RefreshRoomAssignments();
    void UpdateRoomStateForPlayer(const Vec2& playerPos);
    void RevealRoom(int roomIndex);
    void RefreshCurrentRoomState();
    bool DamageEnemy(Entity& enemy, int damage, const Vec2& damageSourcePos);
    void HandlePlayerTileInteractions(Entity& player);
    void HandlePlayerPickupInteractions(Entity& player);
    void HandlePlayerRoomAndWorldInteractions(Entity& player);

    std::vector<DungeonRoom> m_generatedRooms;
    int m_currentRoomIndex = -1;
};
