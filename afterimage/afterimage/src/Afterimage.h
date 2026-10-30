// AFTERIMAGE - the signature mechanic.
//
// The building "remembers". This system stores:
//  1. A snapshot of every object's state (position, open/closed, on/off, present) taken
//     when the player arrives. Pressing Q shows that past state as ghosts and highlights
//     every object whose current state differs.
//  2. A per-tile visit counter (heat map) and a short trail of the player's recent tiles.
//     Tiles walked 3+ times become "worn". The total worn-ness is the repetition score,
//     which makes the Tenant faster, more perceptive, and drives where it searches.
// Plain std::vector / std::deque data - no machine learning, no image processing.
#pragma once
#include <deque>
#include <vector>
#include "World.h"

struct ObjSnap {
    int id = 0;
    int tx = 0, ty = 0;
    bool open = false, on = false, present = true;
};

class Afterimage {
public:
    bool unlocked = false;

    void reset(int w, int h);
    void recordPast(const std::vector<Object>& objs);
    const ObjSnap* pastOf(int id) const;
    bool moved(const Object& o) const;
    bool stateChanged(const Object& o) const;
    bool changed(const Object& o) const { return moved(o) || stateChanged(o); }

    void notePlayerTile(Point t);
    int visits(int x, int y) const;
    int repetitionScore() const { return repeatScore; }
    float patternLevel() const;                       // 0..1
    bool hottestTile(Point& out, int minVisits) const;
    const std::deque<Point>& trail() const { return recent; }

    void update(float dt);
    bool tryActivate();
    bool isActive() const { return activeT > 0.f; }
    float activeTime() const { return activeT; }
    float cooldownLeft() const { return cooldown; }
    bool ready() const { return unlocked && activeT <= 0.f && cooldown <= 0.f; }

private:
    std::vector<ObjSnap> past;
    std::vector<int> heat;
    std::deque<Point> recent;
    Point last{-1, -1};
    int w = 0, h = 0;
    int repeatScore = 0;
    float activeT = 0.f, cooldown = 0.f;
};
