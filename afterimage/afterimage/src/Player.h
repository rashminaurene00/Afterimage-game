// AFTERIMAGE - player controller (movement, collision, sprinting, hiding state).
#pragma once
#include "World.h"
#include "raylib.h"

struct Player {
    Vector2 pos{0, 0};        // centre of the hitbox in world pixels
    int dir = 0;              // 0 down, 1 up, 2 left, 3 right
    float animTime = 0.f;
    bool moving = false;
    bool sprinting = false;
    bool exhausted = false;
    float stamina = 1.f;
    int paceSetting = 0;
    bool hidden = false;
    int hideObj = -1;
    Vector2 preHidePos{0, 0};

    void update(float dt, const Level& lv, bool canMove);
    Rectangle hitboxAt(Vector2 p) const;
    bool collides(const Level& lv, Vector2 p) const;
    Vector2 facingVector() const;
};
