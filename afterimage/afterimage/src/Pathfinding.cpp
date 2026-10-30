#include "Pathfinding.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <functional>
#include <queue>
#include <utility>

std::vector<Point> FindPath(const Level& lv, Point s, Point g) {
    std::vector<Point> result;
    if (!lv.inBounds(s.x, s.y) || !lv.inBounds(g.x, g.y) || s == g) return result;
    if (!lv.passableForEnemy(g.x, g.y)) return result;

    const int W = lv.w, H = lv.h;
    auto idx = [W](int x, int y) { return y * W + x; };
    auto heur = [&](int x, int y) { return std::abs(x - g.x) + std::abs(y - g.y); };

    std::vector<int> gScore((size_t)W * H, INT_MAX);
    std::vector<int> cameFrom((size_t)W * H, -1);
    std::vector<char> closed((size_t)W * H, 0);
    using Node = std::pair<int, int>;  // (f, index)
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    const int start = idx(s.x, s.y), goal = idx(g.x, g.y);
    gScore[start] = 0;
    open.push({heur(s.x, s.y), start});
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};

    while (!open.empty()) {
        int cur = open.top().second;
        open.pop();
        if (closed[cur]) continue;
        closed[cur] = 1;
        if (cur == goal) break;
        int cx = cur % W, cy = cur / W;
        for (int k = 0; k < 4; ++k) {
            int nx = cx + dx[k], ny = cy + dy[k];
            if (!lv.inBounds(nx, ny)) continue;
            int ni = idx(nx, ny);
            if (closed[ni] || !lv.passableForEnemy(nx, ny)) continue;
            int ng = gScore[cur] + 1;
            if (ng < gScore[ni]) {
                gScore[ni] = ng;
                cameFrom[ni] = cur;
                open.push({ng + heur(nx, ny), ni});
            }
        }
    }
    if (cameFrom[goal] == -1) return result;
    for (int c = goal; c != start; c = cameFrom[c]) result.push_back({c % W, c / W});
    std::reverse(result.begin(), result.end());
    return result;
}
