#include "Player.h"
#include <algorithm>
#include <cmath>
#include "Config.h"

Rectangle Player::hitboxAt(Vector2 p) const { return {p.x - 5.f, p.y - 4.f, 10.f, 9.f}; }

bool Player::collides(const Level& lv, Vector2 p) const {
    Rectangle r = hitboxAt(p);
    int x0 = (int)std::floor(r.x / TILE);
    int x1 = (int)std::floor((r.x + r.width - 0.01f) / TILE);
    int y0 = (int)std::floor(r.y / TILE);
    int y1 = (int)std::floor((r.y + r.height - 0.01f) / TILE);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            if (lv.solidForPlayer(x, y)) return true;
    return false;
}

Vector2 Player::facingVector() const {
    switch (dir) {
        case 1: return {0, -1};
        case 2: return {-1, 0};
        case 3: return {1, 0};
        default: return {0, 1};
    }
}

void Player::update(float dt, const Level& lv, bool canMove) {
    moving = false;
    sprinting = false;
    if (hidden || !canMove) {
        stamina = std::min(1.f, stamina + Tune::STAMINA_REGEN * dt);
        return;
    }

    Vector2 in{0, 0};
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) in.y -= 1;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) in.y += 1;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) in.x -= 1;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) in.x += 1;

    if (in.x != 0 || in.y != 0) {
        float l = std::sqrt(in.x * in.x + in.y * in.y);
        in.x /= l;
        in.y /= l;
        moving = true;
        if (std::fabs(in.x) > 0.1f) dir = in.x < 0 ? 2 : 3;
        else dir = in.y < 0 ? 1 : 0;

        bool wantSprint = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (exhausted && stamina > 0.35f) exhausted = false;
        sprinting = wantSprint && !exhausted && stamina > 0.f;
        const float pace[] = {0.85f, 1.f, 1.15f};
        float spd = (sprinting ? Tune::PLAYER_SPRINT : Tune::PLAYER_WALK) * pace[std::clamp(paceSetting, 0, 2)];

        // Axis-separated movement lets the player slide along walls.
        Vector2 np{pos.x + in.x * spd * dt, pos.y};
        if (!collides(lv, np)) pos.x = np.x;
        np = {pos.x, pos.y + in.y * spd * dt};
        if (!collides(lv, np)) pos.y = np.y;
        animTime += dt * (sprinting ? 9.f : 6.f);
    }

    if (sprinting) {
        stamina -= Tune::STAMINA_DRAIN * dt;
        if (stamina <= 0.f) {
            stamina = 0.f;
            exhausted = true;
        }
    } else {
        stamina = std::min(1.f, stamina + Tune::STAMINA_REGEN * dt);
    }
}
