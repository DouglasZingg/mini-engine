#include "game/Tilemap.h"
#include "platform/SdlPlatform.h"
#include "engine/Camera2D.h" 
#include "game/Pathfinding.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

int Tilemap::At(int x, int y) const {
    if (x < 0 || y < 0 || x >= m_w || y >= m_h) return 1; // outside = solid
    return m_tiles[(size_t)y * (size_t)m_w + (size_t)x];
}

bool Tilemap::LoadCSV(const char* path) {
    std::ifstream f(path);
    if (!f) return false;

    m_tiles.clear();
    m_w = 0;
    m_h = 0;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string cell;
        std::vector<int> row;

        while (std::getline(ss, cell, ',')) {
            row.push_back(std::atoi(cell.c_str()));
        }

        if (m_w == 0) m_w = (int)row.size();
        if ((int)row.size() != m_w) return false;

        for (int v : row) m_tiles.push_back(v);
        m_h++;
    }

    return (m_w > 0 && m_h > 0);
}

bool Tilemap::IsSolidAtWorld(const Vec2& world) const {
    int tx = (int)std::floor(world.x / (float)m_tileSize);
    int ty = (int)std::floor(world.y / (float)m_tileSize);
    return At(tx, ty) == 1;
}

static float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

void Tilemap::ResolveCircleCollision(Vec2& pos, float radius) const {
    // Check tiles around the circle
    int minX = (int)std::floor((pos.x - radius) / m_tileSize);
    int maxX = (int)std::floor((pos.x + radius) / m_tileSize);
    int minY = (int)std::floor((pos.y - radius) / m_tileSize);
    int maxY = (int)std::floor((pos.y + radius) / m_tileSize);

    for (int ty = minY; ty <= maxY; ++ty) {
        for (int tx = minX; tx <= maxX; ++tx) {
            if (At(tx, ty) != 1) continue;

            float left = tx * (float)m_tileSize;
            float top = ty * (float)m_tileSize;
            float right = left + m_tileSize;
            float bottom = top + m_tileSize;

            // Closest point on AABB to circle center
            float cx = clampf(pos.x, left, right);
            float cy = clampf(pos.y, top, bottom);

            float dx = pos.x - cx;
            float dy = pos.y - cy;
            float distSq = dx * dx + dy * dy;

            if (distSq < radius * radius && distSq > 0.00001f) {
                float dist = std::sqrt(distSq);
                float pen = radius - dist;
                float nx = dx / dist;
                float ny = dy / dist;
                pos.x += nx * pen;
                pos.y += ny * pen;
            }
        }
    }
}

void Tilemap::Render(SdlPlatform& platform, const Camera2D& cam) const {
    // Render solid tiles as filled rects. (Color params depend on your API)
    // We only draw tiles in view, but simplest first: draw all.
    for (int y = 0; y < m_h; ++y) {
        for (int x = 0; x < m_w; ++x) {
            if (At(x, y) != 1) continue;

            Vec2 world{ x * (float)m_tileSize + m_tileSize * 0.5f,
                        y * (float)m_tileSize + m_tileSize * 0.5f };
            Vec2 screen = cam.WorldToScreen(world);

            int drawX = (int)(screen.x - m_tileSize * 0.5f);
            int drawY = (int)(screen.y - m_tileSize * 0.5f);

            platform.DrawFilledRect(drawX, drawY, m_tileSize, m_tileSize, 60, 60, 60);
        }
    }
}

bool Tilemap::IsSolidTile(int tx, int ty) const {
    return At(tx, ty) == 1; // 1 = wall
}

TileCoord Tilemap::WorldToTile(const Vec2& world) const {
    int tx = (int)std::floor(world.x / (float)m_tileSize);
    int ty = (int)std::floor(world.y / (float)m_tileSize);
    return TileCoord{ tx, ty };
}

Vec2 Tilemap::TileToWorldCenter(int tx, int ty) const {
    return Vec2{
        tx * (float)m_tileSize + m_tileSize * 0.5f,
        ty * (float)m_tileSize + m_tileSize * 0.5f
    };
}
