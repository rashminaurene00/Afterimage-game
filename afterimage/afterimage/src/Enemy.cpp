#include "Enemy.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "Config.h"
#include "Habits.h"
#include "Pathfinding.h"

bool Enemy::logTransitions = false;

namespace {
float Len(Vector2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }
Vector2 Sub(Vector2 a, Vector2 b) { return {a.x - b.x, a.y - b.y}; }
Vector2 Norm(Vector2 v) {
    float l = Len(v);
    return l > 0.0001f ? Vector2{v.x / l, v.y / l} : Vector2{1, 0};
}
Vector2 Rot(Vector2 v, float a) {
    float c = std::cos(a), s = std::sin(a);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}
unsigned Hash(int x, int y) {
    unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    return h ^ (h >> 15);
}
}  // namespace

const char* EnemyStateName(EnemyState s) {
    switch (s) {
        case EnemyState::Patrol: return "PATROL";
        case EnemyState::Investigate: return "INVESTIGATE";
        case EnemyState::Mimic: return "MIMIC";
        case EnemyState::Chase: return "CHASE";
        case EnemyState::Search: return "SEARCH";
        case EnemyState::Return: return "RETURN";
        case EnemyState::Attack: return "ATTACK";
    }
    return "?";
}

void Enemy::spawn(const Level& lv) {
    *this = Enemy{};
    pos = TileCenter(lv.enemySpawn);
    active = lv.hasEnemy;
    state = EnemyState::Patrol;
}

float Enemy::speedMultiplier(const Afterimage& ai) const { return pressure * (1.f + 0.30f * ai.patternLevel()); }

float Enemy::sightRange(const Afterimage& ai) const {
    if (state == EnemyState::Chase) return (8.f + 3.f * pressure) * TILE;
    return (4.5f + 1.5f * ai.patternLevel()) * pressure * TILE;
}

bool Enemy::canSee(const Level& lv, const Player& pl, const Afterimage& ai) const {
    if (pl.hidden) return false;
    Vector2 d = Sub(pl.pos, pos);
    float dist = Len(d);
    if (dist > sightRange(ai)) return false;
    if (state != EnemyState::Chase && dist > 1.6f * TILE) {
        Vector2 n = Norm(d);
        if (n.x * facing.x + n.y * facing.y < 0.57f) return false;  // ~110 degree cone
    }
    return lv.lineOfSight(pos, pl.pos);
}

void Enemy::enter(EnemyState s) {
    if (logTransitions && s != state)
        TraceLog(LOG_INFO, "AI: %s -> %s", EnemyStateName(state), EnemyStateName(s));
    if (s != EnemyState::Chase) sawHide = false;
    state = s;
    path.clear();
    pathIdx = 0;
    waitTimer = 0.f;
    repath = 0.f;
    if (s == EnemyState::Search) {
        searchTimer = 10.f;
        searchPts.clear();
        searchIdx = 0;
    }
    if (s == EnemyState::Chase) lostTimer = 0.f;
    if (s == EnemyState::Attack) attackTimer = 0.45f;
    if (s == EnemyState::Attack) catchReported = false;
    if (s != EnemyState::Mimic) mimicActive = false;
    targetTile = {-1, -1};
    decisionReason = "state transition";
}

void Enemy::setPath(const Level& lv, Point goal) {
    Point s = ToTile(pos);
    if (!lv.passableForEnemy(goal.x, goal.y)) {
        // Goal is solid (e.g. a wardrobe): aim for the nearest open neighbour instead.
        const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        int bestD = 1 << 30;
        bool found = false;
        Point best = goal;
        for (int k = 0; k < 4; ++k) {
            Point n{goal.x + dx[k], goal.y + dy[k]};
            if (!lv.passableForEnemy(n.x, n.y)) continue;
            int dd = std::abs(n.x - s.x) + std::abs(n.y - s.y);
            if (dd < bestD) { bestD = dd; best = n; found = true; }
        }
        if (!found) { path.clear(); pathIdx = 0; return; }
        goal = best;
    }
    path = FindPath(lv, s, goal);
    pathIdx = 0;
}

bool Enemy::followPath(float dt, Level& lv, float speed) {
    if (pathIdx >= path.size()) return true;
    Point node = path[pathIdx];
    Vector2 target = TileCenter(node);
    if (Object* o = lv.objectAt(node.x, node.y)) {
        if (o->type == ObjType::Door && !o->open && !o->locked && Len(Sub(target, pos)) < 20.f)
            o->open = true;  // it leaves doors open behind it - a visible trace
    }
    Vector2 d = Sub(target, pos);
    float l = Len(d);
    float step = speed * dt;
    if (l <= step) {
        pos = target;
        pathIdx++;
    } else {
        pos.x += d.x / l * step;
        pos.y += d.y / l * step;
        facing = {d.x / l, d.y / l};
    }
    return pathIdx >= path.size();
}

void Enemy::buildSearch(const Level& lv, const Afterimage& ai, const HabitProfile* habits) {
    searchPts.clear();
    searchIdx = 0;
    Point c = ToTile(lastKnown);
    searchPts.push_back(c);
    struct Cand { int score; Point p; };
    std::vector<Cand> cands;
    for (int dy = -5; dy <= 5; ++dy)
        for (int dx = -5; dx <= 5; ++dx) {
            int manh = std::abs(dx) + std::abs(dy);
            if (manh < 2 || manh > 6) continue;
            Point p{c.x + dx, c.y + dy};
            if (!lv.passableForEnemy(p.x, p.y)) continue;
            // Prefer tiles the player has walked often (their habits), then a stable hash.
            const int learned = habits ? (int)habits->roomUse(lv.index, p) * 7 : 0;
            cands.push_back({ai.visits(p.x, p.y) * 10 + learned + (int)(Hash(p.x, p.y) % 7), p});
        }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.score > b.score; });
    for (const Cand& cd : cands) {
        bool farEnough = true;
        for (const Point& q : searchPts)
            if (std::abs(q.x - cd.p.x) + std::abs(q.y - cd.p.y) < 3) farEnough = false;
        if (farEnough) searchPts.push_back(cd.p);
        if (searchPts.size() >= 4) break;
    }
}

void Enemy::hearNoise(Vector2 where, float radiusPx) {
    if (!active || dormant > 0.f || state == EnemyState::Chase) return;
    if (Len(Sub(where, pos)) > radiusPx) return;
    investigateTarget = ToTile(where);
    lastKnown = where;
    suspicion = std::max(suspicion, 0.3f);
    enter(EnemyState::Investigate);
}

void Enemy::forceChase(float seconds) {
    active = true;
    enter(EnemyState::Chase);
    forced = seconds;
    suspicion = 1.f;
    alertFlash = 1.5f;
}

void Enemy::onPlayerHide(const Level& lv, const Player& pl, const Afterimage& ai) {
    // If it is chasing and watches you climb in from close range, hiding will not save you.
    sawHide = state == EnemyState::Chase && canSee(lv, pl, ai) && Len(Sub(pl.pos, pos)) < 3.f * TILE;
}

EnemyEvents Enemy::update(float dt, Level& lv, const Player& pl, const Afterimage& ai) {
    static const std::vector<Point> noRoute;
    static const HabitProfile noHabits;
    return update(dt, lv, pl, ai, noRoute, -1, noHabits);
}

EnemyEvents Enemy::update(float dt, Level& lv, const Player& pl, const Afterimage& ai,
                          const std::vector<Point>& historicalRoute, int sampleIndex,
                          const HabitProfile& habits) {
    EnemyEvents ev;
    if (!active) return ev;
    animTime += dt;
    alertFlash = std::max(0.f, alertFlash - dt);
    ghostTimer -= dt;
    if (ghostTimer <= 0.f) {
        ghostTimer = 0.09f;
        ghosts.push_front(pos);
        if (ghosts.size() > 6) ghosts.pop_back();
    }
    if (dormant > 0.f) {
        dormant -= dt;
        return ev;
    }

    const float mult = speedMultiplier(ai);
    const bool sees = canSee(lv, pl, ai);
    const float dist = Len(Sub(pl.pos, pos));
    if (forced > 0.f) forced -= dt;

    // Visible evidence and noise take precedence. Historical playback itself does not
    // reveal the player's present location.
    if (sees && state != EnemyState::Chase) {
        // ordinary suspicion logic below decides when to commit to pursuit
    } else if (mimicking && !historicalRoute.empty() && sampleIndex >= 0 && state == EnemyState::Patrol) {
        mimicActive = true;
        mimicRoute.clear();
        for (Point p : historicalRoute) {
            if (mimicRoute.empty() || mimicRoute.back() != p) mimicRoute.push_back(p);
        }
        mimicRouteIndex = 0;
        enter(EnemyState::Mimic);
        decisionReason = "two-minute historical route playback";
    }

    // Between sightings the Tenant walks the player's two-minute-old route.
    // --- perception -> suspicion ---
    if (state == EnemyState::Attack) {
        // Finish the committed attack wind-up. Re-entering Chase here used to
        // reset attackTimer at close range every frame, leaving the Tenant
        // stuck against the player without ever delivering the catch event.
    } else if (sees) {
        lastKnown = pl.pos;
        if (state != EnemyState::Chase) {
            float rate = (dist < 2.5f * TILE ? 2.2f : 0.85f) * pressure;
            suspicion = std::min(1.f, suspicion + rate * dt);
        }
    } else if (state != EnemyState::Chase) {
        suspicion = std::max(0.f, suspicion - 0.3f * dt);
    }

    if (state != EnemyState::Chase && state != EnemyState::Attack && suspicion >= 1.f) {
        enter(EnemyState::Chase);
        ev.spotted = true;
        alertFlash = 1.2f;
    } else if (sees && suspicion > 0.35f &&
               (state == EnemyState::Patrol || state == EnemyState::Return || state == EnemyState::Search)) {
        investigateTarget = ToTile(pl.pos);
        enter(EnemyState::Investigate);
    }

    // --- state behaviour ---
    switch (state) {
        case EnemyState::Patrol: {
            decisionReason = "follow patrol route";
            if (lv.waypoints.empty()) break;
            habitTimer += dt;
            if (habitTimer > 14.f) {
                habitTimer = 0.f;
                Point hot{}, learned{};
                bool hasHot = ai.hottestTile(hot, 4);
                bool hasLearned = habits.bestRoute(lv.index, ToTile(pos), learned);
                if (hasLearned && habits.routeUse(lv.index, learned) >= 3 &&
                    (!hasHot || Hash(learned.x, learned.y) % 100 < 60)) hot = learned;
                if ((hasHot || hasLearned) && hot != ToTile(pos)) {
                    investigateTarget = hot;
                    enter(EnemyState::Investigate);
                    targetTile = hot;
                    decisionReason = hasLearned ? "repeated route tendency" : "current-run worn tile";
                    break;
                }
            }
            if (waitTimer > 0.f) {
                waitTimer -= dt;
                facing = Rot(facing, dt * 1.5f);
                break;
            }
            Point wp = lv.waypoints[wpIdx % lv.waypoints.size()];
            if (path.empty() && ToTile(pos) == wp) {
                waitTimer = 1.0f;
                wpIdx = (wpIdx + 1) % lv.waypoints.size();
                break;
            }
            if (path.empty()) {
                setPath(lv, wp);
                if (path.empty()) {
                    wpIdx = (wpIdx + 1) % lv.waypoints.size();
                    break;
                }
            }
            if (followPath(dt, lv, Tune::ENEMY_PATROL * mult)) path.clear();
        } break;

        case EnemyState::Investigate: {
            decisionReason = "investigate observed evidence";
            if (waitTimer > 0.f) {
                waitTimer -= dt;
                facing = Rot(facing, dt * 2.2f);
                if (waitTimer <= 0.f) {
                    enter(EnemyState::Return);
                }
                break;
            }
            if (path.empty()) {
                setPath(lv, investigateTarget);
                if (path.empty()) { waitTimer = 1.6f; break; }
            }
            if (followPath(dt, lv, Tune::ENEMY_INVESTIGATE * mult)) {
                path.clear();
                waitTimer = 1.6f;
            }
        } break;

        case EnemyState::Mimic: {
            decisionReason = "retrace the player sample from 120 seconds ago";
            if (!mimicActive || mimicRoute.empty()) {
                mimicActive = false;
                enter(EnemyState::Return);
                decisionReason = "historical trace unavailable";
                break;
            }
            if (mimicRouteIndex >= (int)mimicRoute.size()) {
                mimicActive = false;
                enter(EnemyState::Return);
                decisionReason = "historical route replay complete";
                break;
            }
            const Point target = mimicRoute[(size_t)mimicRouteIndex];
            mimicSample = target;
            if (!lv.passableForEnemy(target.x, target.y)) {
                mimicActive = false;
                enter(EnemyState::Return);
                decisionReason = "historical route blocked";
                break;
            }
            if (target == ToTile(pos)) {
                path.clear();
                targetTile = {-1, -1};
                mimicRouteIndex++;
                break;
            }
            if (targetTile != target && path.empty()) {
                targetTile = target;
                if (target != ToTile(pos)) {
                    setPath(lv, target);
                    if (path.empty()) {
                        decisionReason = "unreachable historical sample skipped";
                        mimicRouteIndex++;
                        break;
                    }
                }
            }
            if (!path.empty()) {
                float spd = mimicSprinting ? Tune::ENEMY_INVESTIGATE * 1.2f : Tune::ENEMY_PATROL * mult;
                if (followPath(dt, lv, spd)) { path.clear(); targetTile = {-1, -1}; }
            }
            break;
        }

        case EnemyState::Chase: {
            decisionReason = "pursue visible player or last known location";
            bool knows = sees || (forced > 0.f && !pl.hidden);
            if (knows) {
                lastKnown = pl.pos;
                lostTimer = 0.f;
            } else {
                lostTimer += dt;
            }
            bool dragOut = pl.hidden && sawHide;
            if (dragOut && dist < 20.f) {
                enter(EnemyState::Attack);
                ev.attackStarted = true;
                break;
            }
            if (!knows && !dragOut && lostTimer > 1.2f) {
                enter(EnemyState::Search);
                break;
            }
            repath -= dt;
            if (repath <= 0.f || path.empty()) {
                repath = 0.25f;
                setPath(lv, ToTile((knows || dragOut) ? pl.pos : lastKnown));
            }
            float spd = Tune::ENEMY_CHASE * mult * (forced > 0.f ? 1.05f : 1.f);
            if (!path.empty()) {
                if (followPath(dt, lv, spd)) path.clear();
            } else if (knows && dist < 1.5f * TILE && dist > 0.5f) {
                Vector2 n = Norm(Sub(pl.pos, pos));
                pos.x += n.x * spd * dt;
                pos.y += n.y * spd * dt;
                facing = n;
            }
        } break;

        case EnemyState::Search: {
            searchTimer -= dt;
            if (searchTimer <= 0.f) { enter(EnemyState::Return); break; }
            if (searchPts.empty()) buildSearch(lv, ai, &habits);
            if (waitTimer > 0.f) {
                waitTimer -= dt;
                facing = Rot(facing, dt * 2.6f);
                break;
            }
            if (searchIdx >= searchPts.size()) { enter(EnemyState::Return); break; }
            if (path.empty()) {
                setPath(lv, searchPts[searchIdx]);
                if (path.empty()) {
                    searchIdx++;
                    waitTimer = 0.6f;
                    break;
                }
            }
            if (followPath(dt, lv, Tune::ENEMY_SEARCH * mult)) {
                path.clear();
                searchIdx++;
                waitTimer = 0.9f;
            }
        } break;

        case EnemyState::Return: {
            decisionReason = "return to nearest patrol waypoint";
            if (lv.waypoints.empty()) { enter(EnemyState::Patrol); break; }
            if (path.empty()) {
                float best = 1e9f;
                for (size_t i = 0; i < lv.waypoints.size(); ++i) {
                    float d = Len(Sub(TileCenter(lv.waypoints[i]), pos));
                    if (d < best) { best = d; wpIdx = i; }
                }
                setPath(lv, lv.waypoints[wpIdx]);
                if (path.empty()) { enter(EnemyState::Patrol); break; }
            }
            if (followPath(dt, lv, Tune::ENEMY_RETURN * mult)) enter(EnemyState::Patrol);
        } break;

        case EnemyState::Attack:
            decisionReason = "attack wind-up";
            break;
    }

    if (state == EnemyState::Attack) {
        attackTimer -= dt;
        if (attackTimer <= 0.f && !catchReported) { ev.caught = true; catchReported = true; }
    } else if (!pl.hidden && Len(Sub(pl.pos, pos)) < 11.f) {
        enter(EnemyState::Attack);
        ev.attackStarted = true;
    }
    return ev;
}
