#pragma once
#include <cstdint>
#include <vector>

struct TileCoord {
    int x = 0;
    int y = 0;

    bool operator==(const TileCoord& other) const {
        return x == other.x && y == other.y;
    }
};

class Tilemap;

namespace Pathfinding {
// Returns a path including the start and goal tiles. Empty means no path.
std::vector<TileCoord> AStar(const Tilemap& map, TileCoord start, TileCoord goal,
                             int maxNodesExpanded = 4000);
}
