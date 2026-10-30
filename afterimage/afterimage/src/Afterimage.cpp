#include "Afterimage.h"
#include <algorithm>
#include "Config.h"

void Afterimage::reset(int width, int height) {
    w = width;
    h = height;
    heat.assign((size_t)w * h, 0);
    past.clear();
    recent.clear();
    last = {-1, -1};
    repeatScore = 0;
    activeT = 0.f;
    cooldown = 0.f;
}

void Afterimage::recordPast(const std::vector<Object>& objs) {
    past.clear();
    for (const Object& o : objs) {
        if (o.type == ObjType::Shelf) continue;  // static scenery, never changes
        ObjSnap s;
        s.id = o.id;
        s.tx = o.tx;
        s.ty = o.ty;
        s.open = o.open;
        s.on = o.on;
        s.present = o.present;
        past.push_back(s);
    }
}

const ObjSnap* Afterimage::pastOf(int id) const {
    for (const ObjSnap& s : past)
        if (s.id == id) return &s;
    return nullptr;
}

bool Afterimage::moved(const Object& o) const {
    const ObjSnap* s = pastOf(o.id);
    return s && (s->tx != o.tx || s->ty != o.ty);
}

bool Afterimage::stateChanged(const Object& o) const {
    const ObjSnap* s = pastOf(o.id);
    return s && (s->open != o.open || s->on != o.on || s->present != o.present);
}

void Afterimage::notePlayerTile(Point t) {
    if (t == last || t.x < 0 || t.y < 0 || t.x >= w || t.y >= h) return;
    last = t;
    int& v = heat[(size_t)t.y * w + t.x];
    v++;
    if (v > 2) repeatScore++;  // every repeat beyond the 2nd visit counts
    recent.push_back(t);
    if (recent.size() > 160) recent.pop_front();
}

int Afterimage::visits(int x, int y) const {
    if (x < 0 || y < 0 || x >= w || y >= h) return 0;
    return heat[(size_t)y * w + x];
}

float Afterimage::patternLevel() const { return std::min(1.f, repeatScore / 120.f); }

bool Afterimage::hottestTile(Point& out, int minVisits) const {
    int best = minVisits - 1;
    bool found = false;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int v = heat[(size_t)y * w + x];
            if (v > best) {
                best = v;
                out = {x, y};
                found = true;
            }
        }
    return found;
}

void Afterimage::update(float dt) {
    if (activeT > 0.f) {
        activeT -= dt;
        if (activeT <= 0.f) {
            activeT = 0.f;
            cooldown = Tune::AFTERIMAGE_COOLDOWN;
        }
    } else if (cooldown > 0.f) {
        cooldown = std::max(0.f, cooldown - dt);
    }
}

bool Afterimage::tryActivate() {
    if (!ready()) return false;
    activeT = Tune::AFTERIMAGE_DURATION;
    return true;
}
