// Bounded, local rule-based observations that survive a run restart.
#pragma once
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>
#include "AtomicFile.h"
#include "World.h"

struct RouteHabit { int level = 0; Point from{}, to{}; unsigned count = 0; };
struct HideHabit { int level = 0; Point tile{}; unsigned count = 0; };

class HabitProfile {
public:
    static constexpr int VERSION = 1;
    static constexpr size_t MAX_RECORDS = 128;
    std::vector<RouteHabit> routes;
    std::vector<HideHabit> hides;
    unsigned noisySprints = 0;
    unsigned escapes = 0;
    bool dirty = false;

    void reset() { routes.clear(); hides.clear(); noisySprints = escapes = 0; dirty = true; }
    void observeRoute(int level, Point from, Point to) {
        if (from == to || level < 0 || level >= LEVEL_COUNT) return;
        auto it = std::find_if(routes.begin(), routes.end(), [&](const RouteHabit& r) {
            return r.level == level && r.from == from && r.to == to;
        });
        if (it == routes.end()) {
            if (routes.size() == MAX_RECORDS) routes.erase(routes.begin());
            routes.push_back({level, from, to, 1});
        } else it->count = std::min(1000u, it->count + 1);
        dirty = true;
    }
    void observeHide(int level, Point tile) {
        if (level < 0 || level >= LEVEL_COUNT) return;
        auto it = std::find_if(hides.begin(), hides.end(), [&](const HideHabit& h) {
            return h.level == level && h.tile == tile;
        });
        if (it == hides.end()) {
            if (hides.size() == MAX_RECORDS) hides.erase(hides.begin());
            hides.push_back({level, tile, 1});
        } else it->count = std::min(1000u, it->count + 1);
        dirty = true;
    }
    void observeSprint() { noisySprints = std::min(100000u, noisySprints + 1); dirty = true; }
    void observeEscape() { escapes = std::min(100000u, escapes + 1); dirty = true; }
    float sprintNoiseMultiplier() const { return std::min(1.25f, 1.f + 0.00625f * (float)noisySprints); }
    unsigned routeUse(int level, Point to) const {
        unsigned n = 0;
        for (const auto& r : routes) if (r.level == level && r.to == to) n += r.count;
        return std::min(n, 10000u);
    }
    unsigned hideUse(int level, Point tile) const {
        for (const auto& h : hides) if (h.level == level && h.tile == tile) return h.count;
        return 0;
    }
    unsigned roomUse(int level, Point tile) const { return routeUse(level, tile) + hideUse(level, tile); }
    bool bestRoute(int level, Point exclude, Point& out) const {
        unsigned best = 2;
        bool found = false;
        for (const auto& r : routes) {
            if (r.level != level || r.to == exclude || r.count <= best) continue;
            best = r.count; out = r.to; found = true;
        }
        for (const auto& h : hides) {
            if (h.level != level || h.tile == exclude || h.count <= best) continue;
            best = h.count; out = h.tile; found = true;
        }
        return found;
    }
    bool save(const std::string& path) {
        if (!dirty) return true;
        std::string temp = path + ".tmp";
        std::string data = "AFTERIMAGE_AI " + std::to_string(VERSION) + " " + std::to_string(routes.size()) + " " +
                           std::to_string(hides.size()) + " " + std::to_string(noisySprints) + " " +
                           std::to_string(escapes) + "\n";
        for (const auto& r : routes) data += "R " + std::to_string(r.level) + " " + std::to_string(r.from.x) + " " +
            std::to_string(r.from.y) + " " + std::to_string(r.to.x) + " " + std::to_string(r.to.y) + " " + std::to_string(r.count) + "\n";
        for (const auto& h : hides) data += "H " + std::to_string(h.level) + " " + std::to_string(h.tile.x) + " " +
            std::to_string(h.tile.y) + " " + std::to_string(h.count) + "\n";
        std::vector<char> writable(data.begin(), data.end());
        writable.push_back('\0');
        if (!SaveFileText(temp.c_str(), writable.data())) return false;
        if (!AtomicReplaceFile(temp.c_str(), path.c_str())) { std::remove(temp.c_str()); return false; }
        dirty = false;
        return true;
    }
    bool load(const std::string& path) {
        if (!FileExists(path.c_str())) return true;
        char* raw = LoadFileText(path.c_str());
        if (!raw) return false;
        std::istringstream in(raw); UnloadFileText(raw);
        std::string magic; int version = 0; size_t nr = 0, nh = 0; unsigned sprints = 0, escapeCount = 0;
        if (!(in >> magic >> version >> nr >> nh >> sprints >> escapeCount) || magic != "AFTERIMAGE_AI" ||
            version != VERSION || nr > MAX_RECORDS || nh > MAX_RECORDS || sprints > 100000 || escapeCount > 100000) return false;
        std::vector<RouteHabit> newRoutes; std::vector<HideHabit> newHides;
        for (size_t i = 0; i < nr; ++i) {
            char kind; RouteHabit r;
            if (!(in >> kind >> r.level >> r.from.x >> r.from.y >> r.to.x >> r.to.y >> r.count) || kind != 'R' ||
                r.level < 0 || r.level >= LEVEL_COUNT || r.from.x < 0 || r.from.y < 0 || r.to.x < 0 || r.to.y < 0 ||
                r.from.x >= 256 || r.from.y >= 256 || r.to.x >= 256 || r.to.y >= 256 || r.count == 0 || r.count > 1000) return false;
            newRoutes.push_back(r);
        }
        for (size_t i = 0; i < nh; ++i) {
            char kind; HideHabit h;
            if (!(in >> kind >> h.level >> h.tile.x >> h.tile.y >> h.count) || kind != 'H' ||
                h.level < 0 || h.level >= LEVEL_COUNT || h.tile.x < 0 || h.tile.y < 0 ||
                h.tile.x >= 256 || h.tile.y >= 256 || h.count == 0 || h.count > 1000) return false;
            newHides.push_back(h);
        }
        std::string extra; if (in >> extra) return false;
        routes = std::move(newRoutes); hides = std::move(newHides);
        noisySprints = sprints; escapes = escapeCount; dirty = false;
        return true;
    }
};
