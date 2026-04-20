#include "game/ProcgenValidation.h"

#include <queue>
#include <utility>

#include "game/Tilemap.h"

bool IsWalkableProcTile(int v) {
    return v != TileValue(TileType::Wall);
}

int CountReachableFloorTiles(const std::vector<int>& tiles, int w, int h, int sx, int sy) {
    if (w <= 0 || h <= 0) return 0;
    if (sx < 0 || sy < 0 || sx >= w || sy >= h) return 0;
    auto idx = [w](int x, int y) { return y * w + x; };
    if (!IsWalkableProcTile(tiles[idx(sx, sy)])) return 0;

    std::vector<unsigned char> seen((size_t)w * (size_t)h, 0);
    std::queue<std::pair<int, int>> q;
    q.push({sx, sy});
    seen[idx(sx, sy)] = 1;
    int count = 0;

    const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();
        ++count;
        for (const auto& d : dirs) {
            int nx = x + d[0];
            int ny = y + d[1];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            int id = idx(nx, ny);
            if (seen[id]) continue;
            if (!IsWalkableProcTile(tiles[id])) continue;
            seen[id] = 1;
            q.push({nx, ny});
        }
    }
    return count;
}

int CountTileValue(const std::vector<int>& tiles, int value) {
    int count = 0;
    for (int tile : tiles) {
        if (tile == value) {
            ++count;
        }
    }
    return count;
}
