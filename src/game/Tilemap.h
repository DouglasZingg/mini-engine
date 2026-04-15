#pragma once
#include <vector>
#include <string>
#include "engine/Math.h"
#include "game/Pathfinding.h"

class SdlPlatform;

enum class TileType : int {
    Floor = 0,
    Wall = 1,
    TokenPickup = 2,
    EnemyChaserSpawn = 3,
    PlayerSpawn = 4,
    HealthPickup = 5,
    SpeedPickup = 6,
    ShieldPickup = 7,
    EnemyFastSpawn = 8,
    EnemyTankSpawn = 9,
    RevealDoor = 10,
    TrapArmed = 11,
    TrapSpent = 12,
};

constexpr int TileValue(TileType tile) { return static_cast<int>(tile); }

inline bool IsFloorLikeTile(TileType tile) {
    switch (tile) {
    case TileType::Floor:
    case TileType::TokenPickup:
    case TileType::EnemyChaserSpawn:
    case TileType::PlayerSpawn:
    case TileType::HealthPickup:
    case TileType::SpeedPickup:
    case TileType::ShieldPickup:
    case TileType::EnemyFastSpawn:
    case TileType::EnemyTankSpawn:
        return true;
    default:
        return false;
    }
}

inline bool IsSpawnMarkerTile(TileType tile) {
    switch (tile) {
    case TileType::EnemyChaserSpawn:
    case TileType::PlayerSpawn:
    case TileType::EnemyFastSpawn:
    case TileType::EnemyTankSpawn:
        return true;
    default:
        return false;
    }
}

inline bool IsPickupMarkerTile(TileType tile) {
    switch (tile) {
    case TileType::TokenPickup:
    case TileType::HealthPickup:
    case TileType::SpeedPickup:
    case TileType::ShieldPickup:
        return true;
    default:
        return false;
    }
}

inline bool IsSolidTileType(TileType tile) {
    return tile == TileType::Wall;
}

class Tilemap {
public:
    bool LoadCSV(const char* path);
    bool LoadFromData(int width, int height, const std::vector<int>& tiles);

    int Width() const { return m_w; }
    int Height() const { return m_h; }
    int TileSize() const { return m_tileSize; }

    int At(int x, int y) const;
    bool IsSolidAtWorld(const Vec2& world) const;

    // Debug render (colored rects)
    void Render(SdlPlatform& platform, const class Camera2D& cam) const;

    // Collision helper for circle-like entities
    void ResolveCircleCollision(Vec2& pos, float radius) const;

    bool IsSolidTile(int tx, int ty) const;        // tile coords
    TileCoord WorldToTile(const Vec2& world) const;
    Vec2 TileToWorldCenter(int tx, int ty) const;

    void SetAt(int x, int y, int v);

private:
    int m_w = 0;
    int m_h = 0;
    int m_tileSize = 64;
    std::vector<int> m_tiles; // row-major (y*m_w + x)
};
