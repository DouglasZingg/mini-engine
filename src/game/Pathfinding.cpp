#include "game/Pathfinding.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <queue>
#include <vector>

#include "game/Tilemap.h"

namespace {
int Manhattan(TileCoord a, TileCoord b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

struct Node {
    int idx = -1;
    int f = 0;
};

struct NodeCompare {
    bool operator()(const Node& a, const Node& b) const {
        return a.f > b.f;
    }
};

int Flatten(int x, int y, int width) {
    return y * width + x;
}

TileCoord Unflatten(int index, int width) {
    return TileCoord{ index % width, index / width };
}
}

namespace Pathfinding {

std::vector<TileCoord> AStar(const Tilemap& map, TileCoord start, TileCoord goal, int maxNodesExpanded) {
    std::vector<TileCoord> path;

    const int width = map.Width();
    const int height = map.Height();
    if (width <= 0 || height <= 0) {
        return path;
    }

    const auto inBounds = [width, height](int x, int y) {
        return x >= 0 && y >= 0 && x < width && y < height;
    };

    if (!inBounds(start.x, start.y) || !inBounds(goal.x, goal.y)) {
        return path;
    }
    if (map.IsSolidTile(start.x, start.y) || map.IsSolidTile(goal.x, goal.y)) {
        return path;
    }

    const int nodeCount = width * height;
    const int inf = std::numeric_limits<int>::max() / 4;

    std::vector<int> g(nodeCount, inf);
    std::vector<int> parent(nodeCount, -1);
    std::vector<uint8_t> closed(nodeCount, 0);

    const int startIndex = Flatten(start.x, start.y, width);
    const int goalIndex = Flatten(goal.x, goal.y, width);

    g[startIndex] = 0;

    std::priority_queue<Node, std::vector<Node>, NodeCompare> open;
    open.push(Node{ startIndex, Manhattan(start, goal) });

    int expanded = 0;
    const int dirs[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };

    while (!open.empty()) {
        const Node current = open.top();
        open.pop();

        if (closed[current.idx]) {
            continue;
        }
        closed[current.idx] = 1;

        const TileCoord currentTile = Unflatten(current.idx, width);
        if (current.idx == goalIndex) {
            break;
        }

        if (++expanded > maxNodesExpanded) {
            break;
        }

        for (const auto& dir : dirs) {
            const int nx = currentTile.x + dir[0];
            const int ny = currentTile.y + dir[1];
            if (!inBounds(nx, ny) || map.IsSolidTile(nx, ny)) {
                continue;
            }

            const int nextIndex = Flatten(nx, ny, width);
            if (closed[nextIndex]) {
                continue;
            }

            const int tentativeG = g[current.idx] + 1;
            if (tentativeG < g[nextIndex]) {
                g[nextIndex] = tentativeG;
                parent[nextIndex] = current.idx;
                const int f = tentativeG + Manhattan(TileCoord{ nx, ny }, goal);
                open.push(Node{ nextIndex, f });
            }
        }
    }

    if (parent[goalIndex] == -1 && goalIndex != startIndex) {
        return path;
    }

    int walk = goalIndex;
    path.push_back(Unflatten(walk, width));
    while (walk != startIndex) {
        walk = parent[walk];
        if (walk < 0) {
            path.clear();
            return path;
        }
        path.push_back(Unflatten(walk, width));
    }

    std::reverse(path.begin(), path.end());
    return path;
}

}
