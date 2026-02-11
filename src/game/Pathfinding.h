#pragma once
#include <vector>
#include <cstdint>

struct TileCoord {
    int x = 0;
    int y = 0;
    bool operator==(const TileCoord& o) const { return x == o.x && y == o.y; }
};

class Tilemap;

namespace Pathfinding {
    // Returns path INCLUDING start and goal tiles if found. Empty = no path.
    std::vector<TileCoord> AStar(const Tilemap& map, TileCoord start, TileCoord goal,
        int maxNodesExpanded = 4000);
}
