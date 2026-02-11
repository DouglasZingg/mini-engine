#include "game/Pathfinding.h"
#include "game/Tilemap.h"
#include <queue>
#include <vector>
#include <limits>
#include <cstdlib>
#include <algorithm>

static int manhattan(TileCoord a, TileCoord b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

struct Node {
    int idx = -1;        // flattened index
    int f = 0;           // g + h
};

struct NodeCmp {
    bool operator()(const Node& a, const Node& b) const { return a.f > b.f; }
};

static int flatten(int x, int y, int w) { return y * w + x; }
static TileCoord unflatten(int idx, int w) { return TileCoord{ idx % w, idx / w }; }

namespace Pathfinding {

    std::vector<TileCoord> AStar(const Tilemap& map, TileCoord start, TileCoord goal, int maxNodesExpanded) {
        std::vector<TileCoord> out;

        const int w = map.Width();
        const int h = map.Height();
        if (w <= 0 || h <= 0) return out;

        auto inBounds = [&](int x, int y) { return x >= 0 && y >= 0 && x < w && y < h; };

        if (!inBounds(start.x, start.y) || !inBounds(goal.x, goal.y)) return out;
        if (map.IsSolidTile(start.x, start.y) || map.IsSolidTile(goal.x, goal.y)) return out;

        const int N = w * h;
        const int INF = std::numeric_limits<int>::max() / 4;

        std::vector<int> g(N, INF);
        std::vector<int> parent(N, -1);
        std::vector<uint8_t> closed(N, 0);

        const int sIdx = flatten(start.x, start.y, w);
        const int gIdx = flatten(goal.x, goal.y, w);

        g[sIdx] = 0;

        std::priority_queue<Node, std::vector<Node>, NodeCmp> open;
        open.push(Node{ sIdx, manhattan(start, goal) });

        int expanded = 0;

        const int dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };

        while (!open.empty()) {
            Node cur = open.top();
            open.pop();

            if (closed[cur.idx]) continue;
            closed[cur.idx] = 1;

            TileCoord c = unflatten(cur.idx, w);
            if (cur.idx == gIdx) break;

            if (++expanded > maxNodesExpanded) break;

            for (auto& d : dirs) {
                int nx = c.x + d[0];
                int ny = c.y + d[1];
                if (!inBounds(nx, ny)) continue;
                if (map.IsSolidTile(nx, ny)) continue;

                int nIdx = flatten(nx, ny, w);
                if (closed[nIdx]) continue;

                int tentativeG = g[cur.idx] + 1;
                if (tentativeG < g[nIdx]) {
                    g[nIdx] = tentativeG;
                    parent[nIdx] = cur.idx;
                    int f = tentativeG + manhattan(TileCoord{ nx, ny }, goal);
                    open.push(Node{ nIdx, f });
                }
            }
        }

        // Reconstruct
        if (parent[gIdx] == -1 && gIdx != sIdx) return out;

        int walk = gIdx;
        out.push_back(unflatten(walk, w));
        while (walk != sIdx) {
            walk = parent[walk];
            if (walk < 0) { out.clear(); return out; }
            out.push_back(unflatten(walk, w));
        }
        std::reverse(out.begin(), out.end());
        return out;
    }

} // namespace Pathfinding
