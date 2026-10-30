#include "World.h"
#include <cmath>

Vector2 TileCenter(Point p) {
    return {p.x * (float)TILE + TILE * 0.5f, p.y * (float)TILE + TILE * 0.5f};
}

Point ToTile(Vector2 v) {
    return {(int)std::floor(v.x / TILE), (int)std::floor(v.y / TILE)};
}

bool Object::isSolid() const {
    if (!present) return false;
    switch (type) {
        case ObjType::Door: return !open;
        case ObjType::Note: return false;
        default: return true;
    }
}

bool Object::blocksSight() const {
    if (!present) return false;
    switch (type) {
        case ObjType::Door: return !open;
        case ObjType::Exit:
        case ObjType::Wardrobe:
        case ObjType::Shelf: return true;
        default: return false;
    }
}

bool Object::isInteractable() const { return present && type != ObjType::Shelf; }

void Level::refresh() {
    occ.assign((size_t)w * (size_t)h, -1);
    for (size_t i = 0; i < objects.size(); ++i) {
        const Object& o = objects[i];
        if (!o.present || !inBounds(o.tx, o.ty)) continue;
        occ[(size_t)o.ty * w + o.tx] = (int)i;
    }
}

bool Level::inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }

char Level::tileAt(int x, int y) const { return inBounds(x, y) ? tiles[y][x] : '#'; }

bool Level::isWall(int x, int y) const {
    char c = tileAt(x, y);
    return c == '#' || c == ' ';
}

Object* Level::objectAt(int x, int y) {
    if (!inBounds(x, y) || occ.empty()) return nullptr;
    int i = occ[(size_t)y * w + x];
    return i < 0 ? nullptr : &objects[i];
}

const Object* Level::objectAt(int x, int y) const {
    if (!inBounds(x, y) || occ.empty()) return nullptr;
    int i = occ[(size_t)y * w + x];
    return i < 0 ? nullptr : &objects[i];
}

Object* Level::findGlyph(char c) {
    for (auto& o : objects)
        if (o.glyph == c) return &o;
    return nullptr;
}

bool Level::solidForPlayer(int x, int y) const {
    if (isWall(x, y)) return true;
    const Object* o = objectAt(x, y);
    return o && o->isSolid();
}

bool Level::passableForEnemy(int x, int y) const {
    if (isWall(x, y)) return false;
    const Object* o = objectAt(x, y);
    if (!o || !o->isSolid()) return true;
    return o->type == ObjType::Door && !o->locked;  // the Tenant opens unlocked doors
}

bool Level::blocksSight(int x, int y) const {
    if (isWall(x, y)) return true;
    const Object* o = objectAt(x, y);
    return o && o->blocksSight();
}

bool Level::lineOfSight(Vector2 a, Vector2 b) const {
    Point ta = ToTile(a), tb = ToTile(b);
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = std::sqrt(dx * dx + dy * dy);
    int steps = (int)(len / 4.f) + 1;
    for (int i = 1; i < steps; ++i) {
        float t = (float)i / (float)steps;
        Point p = ToTile({a.x + dx * t, a.y + dy * t});
        if (p == tb) return true;
        if (p == ta) continue;
        if (blocksSight(p.x, p.y)) return false;
    }
    return true;
}
