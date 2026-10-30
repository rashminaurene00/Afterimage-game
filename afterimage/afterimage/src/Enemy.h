// AFTERIMAGE - "The Tenant": finite-state-machine enemy AI.
//
// States: PATROL -> INVESTIGATE -> CHASE -> SEARCH -> RETURN -> PATROL
//  * Detection: range + 110-degree view cone + tile line-of-sight. Seeing the player
//    fills a suspicion meter (fast when close); at 1.0 it chases.
//  * Movement: grid A* over tiles; opens unlocked doors on its way.
//  * Behaviour response: every 14 s on patrol it checks the player's most-walked tile;
//    searches favour tiles the player has walked often; speed and sight range grow
//    with the repetition score from the Afterimage heat map. Deterministic logic only.
#pragma once
#include <deque>
#include <string>
#include <vector>
#include "Afterimage.h"
#include "Player.h"
#include "World.h"

enum class EnemyState { Patrol, Investigate, Mimic, Chase, Search, Return, Attack };
const char* EnemyStateName(EnemyState s);

struct EnemyEvents {
    bool caught = false;
    bool spotted = false;
    bool attackStarted = false;
};

struct HabitProfile;

class Enemy {
public:
    Vector2 pos{0, 0};
    Vector2 facing{1, 0};
    EnemyState state = EnemyState::Patrol;
    float suspicion = 0.f;
    bool active = false;
    float pressure = 0.8f;          // adjusted by the difficulty setting
    float dormant = 0.f;            // seconds before it starts acting
    float animTime = 0.f;
    float alertFlash = 0.f;
    bool mimicking = false;
    int mimicDir = 0;
    bool mimicMoving = false;
    bool mimicSprinting = false;
    Vector2 mimicEcho{0, 0};
    Point mimicSample{-1, -1};
    Point targetTile{-1, -1};
    int mimicRouteIndex = 0;
    bool mimicActive = false;
    std::vector<Point> mimicRoute;
    const char* decisionReason = "patrol";
    std::deque<Vector2> ghosts;     // its own afterimage trail (visual)
    static bool logTransitions;

    void spawn(const Level& lv);
    void hearNoise(Vector2 where, float radiusPx);
    void forceChase(float seconds);
    void onPlayerHide(const Level& lv, const Player& pl, const Afterimage& ai);
    EnemyEvents update(float dt, Level& lv, const Player& pl, const Afterimage& ai);
    EnemyEvents update(float dt, Level& lv, const Player& pl, const Afterimage& ai,
                       const std::vector<Point>& historicalRoute, int sampleIndex,
                       const HabitProfile& habits);

    float speedMultiplier(const Afterimage& ai) const;
    float sightRange(const Afterimage& ai) const;
    bool canSee(const Level& lv, const Player& pl, const Afterimage& ai) const;
    const std::vector<Point>& currentPath() const { return path; }
    Point lastKnownTile() const { return ToTile(lastKnown); }

private:
    std::vector<Point> path;
    size_t pathIdx = 0;
    size_t wpIdx = 0;
    float waitTimer = 0.f;
    float repath = 0.f;
    Vector2 lastKnown{0, 0};
    float lostTimer = 0.f;
    float searchTimer = 0.f;
    std::vector<Point> searchPts;
    size_t searchIdx = 0;
    float habitTimer = 0.f;
    float forced = 0.f;
    Point investigateTarget{0, 0};
    bool sawHide = false;
    float ghostTimer = 0.f;
    float attackTimer = 0.f;
    bool catchReported = false;

    void enter(EnemyState s);
    void setPath(const Level& lv, Point goal);
    bool followPath(float dt, Level& lv, float speed);
    void buildSearch(const Level& lv, const Afterimage& ai, const HabitProfile* habits = nullptr);
};
