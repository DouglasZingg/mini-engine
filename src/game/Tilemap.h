#pragma once
#include <vector>
#include <string>
#include "engine/Math.h"

class SdlPlatform;
struct TileCoord;

class Tilemap {
public:
    bool LoadCSV(const char* path);

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
