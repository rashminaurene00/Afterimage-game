// AFTERIMAGE - world rendering at 320x180, then scaled up with point filtering.
#include <algorithm>
#include <cmath>
#include "Config.h"
#include "Game.h"

namespace {
unsigned H2(int x, int y) {
    unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    return h ^ (h >> 15);
}
Color C(int r, int g, int b, int a = 255) {
    return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};
}
float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
float Dist(Vector2 a, Vector2 b) { return std::hypot(a.x - b.x, a.y - b.y); }

void DashedRect(float x, float y, float w, float h, Color c, float phase) {
    for (float i = 0; i < w; i += 2.f) {
        if (((int)(i + phase) / 2) % 2) continue;
        DrawRectangle((int)(x + i), (int)y, 1, 1, c);
        DrawRectangle((int)(x + w - 1 - i), (int)(y + h - 1), 1, 1, c);
    }
    for (float i = 0; i < h; i += 2.f) {
        if (((int)(i + phase) / 2) % 2) continue;
        DrawRectangle((int)x, (int)(y + h - 1 - i), 1, 1, c);
        DrawRectangle((int)(x + w - 1), (int)(y + i), 1, 1, c);
    }
}
}  // namespace

const char* Game::spriteFor(const Object& o) const {
    switch (o.type) {
        case ObjType::Door: return o.open ? "door_open" : "door";
        case ObjType::Exit: return o.locked ? "exit_locked" : "exit_open";
        case ObjType::Cabinet: return "cabinet";
        case ObjType::Crate: return "crate";
        case ObjType::FuseBox: return o.on ? "fusebox_on" : "fusebox";
        case ObjType::Wardrobe: return "wardrobe";
        case ObjType::Alcove: return "alcove";
        case ObjType::Note: return "note";
        case ObjType::Furniture: return o.sprite.c_str();
        case ObjType::Shelf: return "shelf";
        case ObjType::Switch: return o.on ? "switch_on" : "switch_off";
        case ObjType::Terminal: return o.on ? "terminal_on" : "terminal";
    }
    return "crate";
}

void Game::drawSprite(const char* key, float cx, float yBottom, Color tint, int frame, int frames) {
    Texture2D t = art.get(key);
    if (t.id == 0) return;
    float fw = (float)t.width / (float)frames;
    float scale = 16.f / fw;
    float dh = t.height * scale;
    Rectangle src{fw * frame, 0, fw, (float)t.height};
    Rectangle dst{std::floor(cx - 8.f), std::floor(yBottom - dh), 16.f, dh};
    DrawTexturePro(t, src, dst, {0, 0}, 0.f, tint);
}

void Game::drawCharacter(const char* key, float cx, float yBottom, Color tint, int direction, int animationFrame) {
    const CharacterAnimation& a = art.character(key);
    if (!a.atlas.id) {
        // Keep the current prototype art visible until production character frames
        // are supplied; documentation explicitly labels this fallback as provisional.
        drawSprite(key, cx, yBottom, tint, direction * 2 + (animationFrame > 0 ? 1 : 0), 8);
        return;
    }
    const int row = std::clamp(direction, 0, 3);
    const int col = std::clamp(animationFrame, 0, a.columns - 1);
    Rectangle src{(float)(col * a.frameWidth), (float)(row * a.frameHeight),
                  (float)a.frameWidth, (float)a.frameHeight};
    const float scale = 16.f / (float)a.frameWidth;
    const float w = a.frameWidth * scale, h = a.frameHeight * scale;
    Rectangle dst{std::floor(cx - w * 0.5f), std::floor(yBottom - h), w, h};
    DrawTexturePro(a.atlas, src, dst, {0, 0}, 0.f, tint);
}

// -------------------------------------------------------------------------------------
void Game::drawTiles(int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            int px = x * TILE, py = y * TILE;
            char c = level.tileAt(x, y);
            unsigned h = H2(x, y);
            if (c == ' ') continue;
            if (c == '#') {
                DrawRectangle(px, py, TILE, TILE, C(26, 18, 36));
                if (!level.isWall(x, y + 1)) {
                    // Front-facing wall: stained wallpaper and a baseboard (pseudo 3/4 view).
                    DrawRectangle(px, py + 5, TILE, 11, C(56, 38, 68));
                    for (int i = 0; i < 4; ++i) DrawRectangle(px + i * 4 + 1, py + 6, 1, 9, C(48, 32, 60));
                    DrawRectangle(px, py + 5, TILE, 1, C(82, 62, 98));
                    DrawRectangle(px, py + 14, TILE, 2, C(22, 14, 28));
                    if (h % 5 == 0) DrawRectangle(px + (int)(h % 11), py + 7, 2, 3 + (int)(h % 5), C(70, 94, 74, 90));
                    if (h % 9 == 0) DrawRectangle(px + 3 + (int)(h % 7), py + 6, 1, 8, C(30, 20, 36));
                } else if (h % 7 == 0) {
                    DrawRectangle(px + (int)(h % 13), py + (int)((h >> 4) % 13), 1, 1, C(40, 28, 50));
                }
                continue;
            }
            if (c == ',') {
                DrawRectangle(px, py, TILE, TILE, C(46, 30, 54));
                if ((x + y) % 2 == 0) DrawRectangle(px + 7, py + 7, 2, 2, C(62, 42, 70));
                if (h % 6 == 0) DrawRectangle(px + (int)(h % 14), py + (int)((h >> 3) % 14), 2, 1, C(34, 22, 40));
            } else if (c == '~') {
                DrawRectangle(px, py, TILE, TILE, C(44, 54, 48));
                DrawRectangle(px, py, TILE, 1, C(34, 42, 38));
                DrawRectangle(px, py, 1, TILE, C(34, 42, 38));
                DrawRectangle(px, py + 8, TILE, 1, C(38, 46, 42));
                DrawRectangle(px + 8, py, 1, TILE, C(38, 46, 42));
                if (h % 8 == 0) DrawRectangle(px + 2 + (int)(h % 10), py + 3, 3, 2, C(70, 90, 70, 120));
            } else {
                DrawRectangle(px, py, TILE, TILE, C(42, 38, 46));
                for (int k = 0; k < 4; ++k) {
                    DrawRectangle(px, py + k * 4 + 3, TILE, 1, C(34, 30, 38));
                    int seam = (int)((h >> (k * 3)) % 14) + 1;
                    DrawRectangle(px + seam, py + k * 4, 1, 3, C(36, 32, 40));
                }
                if (h % 4 == 0) DrawRectangle(px + (int)(h % 15), py + (int)((h >> 5) % 15), 1, 1, C(60, 56, 60));
                if (h % 23 == 0) DrawRectangle(px + 3, py + 4, 6, 4, C(30, 40, 34, 140));  // damp patch
            }
        }
}

// -------------------------------------------------------------------------------------
void Game::drawWorld() {
    float left = cam.target.x - VIRTUAL_W / 2.f, top = cam.target.y - VIRTUAL_H / 2.f;
    int x0 = std::max(0, (int)std::floor(left / TILE) - 1);
    int y0 = std::max(0, (int)std::floor(top / TILE) - 1);
    int x1 = std::min(level.w - 1, (int)std::floor((left + VIRTUAL_W) / TILE) + 1);
    int y1 = std::min(level.h - 1, (int)std::floor((top + VIRTUAL_H) / TILE) + 2);

    BeginMode2D(cam);
    drawTiles(x0, y0, x1, y1);

    // Subtle ward bay markings turn otherwise identical corridors into identifiable rooms.
    if (levelIndex == 3) {
        const int bays[][4] = {{1, 1, 14, 4}, {19, 1, 34, 4}, {19, 5, 26, 8},
                               {1, 5, 17, 8}, {1, 9, 15, 13}, {16, 9, 34, 13}};
        const char* names[] = {"INTAKE 047", "MIRROR BAY", "OBSERVATION", "CHART ROOM", "WARD C", "RECORDS"};
        for (int i = 0; i < 6; ++i) {
            const auto& b = bays[i];
            int x = b[0] * TILE + 4, y = b[1] * TILE + 4;
            if (x < left || x > left + VIRTUAL_W || y < top || y > top + VIRTUAL_H) continue;
            DrawRectangle(x, y, 70, 9, ColorAlpha(Pal::Black, 0.65f));
            DrawRectangle(x, y, 2, 9, ColorAlpha(Pal::BrightCrimson, 0.8f));
            DrawText(names[i], x + 5, y + 1, 7, ColorAlpha(Pal::PaleGrey, 0.72f));
        }
    }

    // Y-sorted drawables: objects, player, enemy, apparition.
    struct D { float y; int kind; size_t idx; };
    std::vector<D> list;
    for (size_t i = 0; i < level.objects.size(); ++i) {
        const Object& o = level.objects[i];
        if (!o.present || o.tx < x0 || o.tx > x1 || o.ty < y0 || o.ty > y1) continue;
        float sy = o.type == ObjType::Note ? o.ty * (float)TILE : (o.ty + 1) * (float)TILE - 0.5f;
        list.push_back({sy, 0, i});
    }
    if (!player.hidden) list.push_back({player.pos.y + 5.f, 1, 0});
    if (level.hasEnemy && enemy.active) list.push_back({enemy.pos.y + 5.f, 2, 0});
    auto gm = level.markers.find('g');
    if (levelIndex == 0 && flags.apparition > 0.f && gm != level.markers.end())
        list.push_back({TileCenter(gm->second).y + 5.f, 3, 0});
    std::sort(list.begin(), list.end(), [](const D& a, const D& b) { return a.y < b.y; });

    for (const D& d : list) {
        switch (d.kind) {
            case 0: {
                const Object& o = level.objects[d.idx];
                drawSprite(spriteFor(o), o.tx * (float)TILE + 8.f, (o.ty + 1) * (float)TILE, WHITE);
            } break;
            case 1: {
                int anim = player.moving ? 1 + (((int)player.animTime % 4) + 4) % 4 : 0;
                drawCharacter("player", player.pos.x, player.pos.y + 6.f, WHITE, player.dir, anim);
            } break;
            case 2: {
                float bob = std::sin(enemy.animTime * 5.f) > 0.f ? 0.f : 1.f;
                if (enemy.mimicking && enemy.state != EnemyState::Chase) {
                    const int anim = enemy.mimicMoving ? 1 + (((int)(enemy.animTime * (enemy.mimicSprinting ? 10.f : 5.f)) % 4) + 4) % 4 : 0;
                    drawCharacter("player", enemy.pos.x, enemy.pos.y + 6.f + bob, ColorAlpha(WHITE, 0.88f),
                                  enemy.mimicDir, anim);
                    DrawRectangle((int)enemy.pos.x - 1, (int)enemy.pos.y - 11, 3, 1, Pal::BrightCrimson);
                    DrawRectangle((int)enemy.pos.x - 7, (int)enemy.pos.y - 12, 1, 8, ColorAlpha(Pal::Lilac, 0.72f));
                } else {
                    drawSprite("tenant", enemy.pos.x, enemy.pos.y + 5.f + bob, WHITE);
                }
            } break;
            case 3: {
                if (GetRandomValue(0, 10) > 2) {
                    Vector2 g = TileCenter(gm->second);
                    float a = Clamp01(flags.apparition / 2.6f);
                    drawSprite("tenant", g.x, g.y + 5.f, ColorAlpha(WHITE, 0.4f + 0.6f * a));
                    DrawRectangle((int)g.x - 1, (int)g.y - 16, 3, 1, Pal::BrightCrimson);
                }
            } break;
        }
    }

    drawDarkness();
    if (after.isActive()) drawAfterimageOverlay();
    drawEnemyOverlay();

    for (const NoiseRing& r : rings) {
        float k = r.t / 0.8f;
        DrawCircleLines((int)r.pos.x, (int)r.pos.y, r.radius * k, ColorAlpha(Pal::Lilac, 0.55f * (1.f - k)));
    }
    if (player.hidden) {
        // A thin slit of vision through the wardrobe door.
        DrawRectangle((int)player.pos.x - 4, (int)player.pos.y - 1, 8, 1, ColorAlpha(Pal::PaleGrey, 0.5f));
    }
    if (debug) drawDebug();
    EndMode2D();
    drawScreenFx();
}

// -------------------------------------------------------------------------------------
// Darkness: a grid of 4x4 "light cells". Each cell's brightness comes from distance to
// the player, a flashlight cone in the facing direction, and tile line-of-sight, so walls
// and shelves cast real shadows. Cheap (about 4k rectangles) and very 1990s.
void Game::drawDarkness() {
    float left = cam.target.x - VIRTUAL_W / 2.f, top = cam.target.y - VIRTUAL_H / 2.f;
    int tx0 = (int)std::floor(left / TILE) - 1, ty0 = (int)std::floor(top / TILE) - 1;
    int tw = VIRTUAL_W / TILE + 3, th = VIRTUAL_H / TILE + 3;
    std::vector<char> vis((size_t)tw * th, 0);
    for (int j = 0; j < th; ++j)
        for (int i = 0; i < tw; ++i) {
            Point t{tx0 + i, ty0 + j};
            vis[(size_t)j * tw + i] = level.inBounds(t.x, t.y) && level.lineOfSight(player.pos, TileCenter(t));
        }

    Vector2 f = player.facingVector();
    float baseR = 52.f + (levelIndex == 0 && flags.powerOn ? 14.f : 0.f);
    float coneR = 112.f;
    const float cosCone = 0.84f;  // ~33 degrees half-angle
    float ambient = 0.08f + lightning;
    float afterA = 0.f;
    if (after.isActive()) {
        float t = after.activeTime();
        afterA = std::min(Clamp01((Tune::AFTERIMAGE_DURATION - t) / 0.25f), Clamp01(t / 0.5f));
        ambient = std::max(ambient, 0.42f * afterA);
    }
    if (player.hidden) { baseR = 22.f; coneR = 22.f; }
    float dip = flicker > 0.f && GetRandomValue(0, 3) == 0 ? 0.55f : 0.f;

    int cx0 = (int)std::floor(left / 4.f) * 4 - 4, cy0 = (int)std::floor(top / 4.f) * 4 - 4;
    for (int cy = cy0; cy < top + VIRTUAL_H + 4; cy += 4)
        for (int cx = cx0; cx < left + VIRTUAL_W + 4; cx += 4) {
            Vector2 c{cx + 2.f, cy + 2.f};
            float dx = c.x - player.pos.x, dy = c.y - player.pos.y;
            float d = std::sqrt(dx * dx + dy * dy);
            float R = baseR;
            if (!player.hidden && d > 0.1f) {
                float dot = (dx * f.x + dy * f.y) / d;
                if (dot > cosCone) R = baseR + (coneR - baseR) * Clamp01((dot - cosCone) / (1.f - cosCone) * 2.f);
            }
            float b = 1.f - Clamp01((d - R * 0.35f) / (R * 0.65f));
            b = b * b * (3.f - 2.f * b);
            int ti = (int)std::floor(c.x / TILE) - tx0, tj = (int)std::floor(c.y / TILE) - ty0;
            bool v = ti >= 0 && tj >= 0 && ti < tw && tj < th && vis[(size_t)tj * tw + ti];
            if (!v) b *= 0.12f;
            b = std::max(b, ambient);
            b *= (1.f - dip);
            float alpha = (1.f - b) * 0.96f;
            if (alpha > 0.01f) DrawRectangle(cx, cy, 4, 4, ColorAlpha(C(6, 4, 10), alpha));
        }
}

// -------------------------------------------------------------------------------------
// The afterimage view: worn paths, ghosts of past object positions, changed-object
// highlights, and the archive's numbered hand-marks.
void Game::drawAfterimageOverlay() {
    float t = after.activeTime();
    float a = std::min(Clamp01((Tune::AFTERIMAGE_DURATION - t) / 0.25f), Clamp01(t / 0.5f));
    float pulse = 0.5f + 0.5f * std::sin(clock * 9.f);

    // 1) Your trail: faded green footprints, crimson where you walked 4+ times.
    for (int y = 0; y < level.h; ++y)
        for (int x = 0; x < level.w; ++x) {
            int v = after.visits(x, y);
            if (v <= 0) continue;
            Color col = v >= 4 ? Pal::BrightCrimson : Pal::FadedGreen;
            float al = std::min(0.7f, 0.18f + 0.1f * v) * a;
            DrawRectangle(x * TILE + 5, y * TILE + 7, 2, 3, ColorAlpha(col, al));
            DrawRectangle(x * TILE + 9, y * TILE + 5, 2, 3, ColorAlpha(col, al));
        }

    // 2) Objects whose state differs from the building's memory.
    for (const Object& o : level.objects) {
        const ObjSnap* s = after.pastOf(o.id);
        if (!s) continue;
        bool moved = after.moved(o), changedState = after.stateChanged(o);
        float ox = o.tx * (float)TILE, oy = o.ty * (float)TILE;
        if (moved) {
            float px = s->tx * (float)TILE, py = s->ty * (float)TILE;
            DrawRectangle((int)px + 1, (int)py + 1, 14, 14, ColorAlpha(Pal::Muted, 0.35f * a));
            drawSprite(spriteFor(o), px + 8.f, py + 16.f, ColorAlpha(Pal::Bone, 0.75f * a));
            DashedRect(px, py, 16, 16, ColorAlpha(Pal::Lilac, a), clock * 8.f);
            DrawLineV({px + 8.f, py + 8.f}, {ox + 8.f, oy + 8.f}, ColorAlpha(Pal::Lilac, 0.6f * a));
        }
        if (moved || changedState) {
            DrawRectangleLinesEx({ox - 1, oy - 1, 18, 18}, 1, ColorAlpha(Pal::BrightCrimson, (0.5f + 0.5f * pulse) * a));
            if (changedState && !moved) {
                Object past = o;
                past.open = s->open;
                past.on = s->on;
                drawSprite(spriteFor(past), ox + 8.f, oy + 16.f, ColorAlpha(Pal::Lilac, 0.45f * a));
            }
        }
        if (o.echoOrder > 0) {
            // A ghostly hand-mark with the order in which "you" threw this lever before.
            float hx = ox + 4.f, hy = oy - 12.f;
            DrawRectangle((int)hx - 1, (int)hy - 1, 10, 11, ColorAlpha(Pal::Black, 0.6f * a));
            DrawRectangleLines((int)hx - 1, (int)hy - 1, 10, 11, ColorAlpha(Pal::PaleGreen, a));
            DrawText(TextFormat("%d", o.echoOrder), (int)hx + 2, (int)hy, 10, ColorAlpha(Pal::PaleGreen, a));
        }
    }
}

// -------------------------------------------------------------------------------------
void Game::drawEnemyOverlay() {
    if (!level.hasEnemy || !enemy.active) return;
    float d = Dist(enemy.pos, player.pos);
    float range = 12.f * TILE;

    if (enemy.state == EnemyState::Chase) {
        int i = 0;
        for (const Vector2& g : enemy.ghosts) {
            if (i > 0) drawSprite("tenant", g.x, g.y + 5.f, ColorAlpha(Pal::Crimson, 0.28f - i * 0.04f));
            ++i;
        }
    }
    if (d < range) {
        // Its eyes are always visible within range - the main visual danger cue.
        float a = 0.25f + 0.75f * Clamp01(1.f - d / range);
        DrawCircle((int)enemy.pos.x, (int)enemy.pos.y - 16, 4.f, ColorAlpha(Pal::Crimson, 0.18f * a));
        DrawRectangle((int)enemy.pos.x - 1, (int)enemy.pos.y - 16, 3, 1, ColorAlpha(Pal::BrightCrimson, a));
        if (enemy.state != EnemyState::Chase && enemy.suspicion > 0.02f) {
            DrawRectangle((int)enemy.pos.x - 6, (int)enemy.pos.y - 27, 12, 2, ColorAlpha(Pal::Black, 0.8f));
            DrawRectangle((int)enemy.pos.x - 6, (int)enemy.pos.y - 27, (int)(12 * enemy.suspicion), 2,
                          Pal::BrightCrimson);
        }
    }
    if (enemy.alertFlash > 0.f && ((int)(clock * 12.f) % 2 == 0))
        DrawText("!", (int)enemy.pos.x - 2, (int)enemy.pos.y - 36, 10, Pal::BrightCrimson);
}

void Game::drawDebug() {
    if (level.hasEnemy) {
        const auto& p = enemy.currentPath();
        for (const Point& n : p) DrawRectangleLines(n.x * TILE + 4, n.y * TILE + 4, 8, 8, Pal::PaleGreen);
        DrawCircleLines((int)enemy.pos.x, (int)enemy.pos.y, enemy.sightRange(after), ColorAlpha(Pal::Crimson, 0.5f));
        DrawLineV(enemy.pos, {enemy.pos.x + enemy.facing.x * 20.f, enemy.pos.y + enemy.facing.y * 20.f}, YELLOW);
        for (const Point& w : level.waypoints) DrawRectangle(w.x * TILE + 6, w.y * TILE + 6, 4, 4, SKYBLUE);
    }
    Rectangle hb = player.hitboxAt(player.pos);
    DrawRectangleLinesEx(hb, 1, GREEN);
}

// -------------------------------------------------------------------------------------
void Game::drawScreenFx() {
    float a = 0.f;
    if (after.isActive()) {
        float t = after.activeTime();
        a = std::min(Clamp01((Tune::AFTERIMAGE_DURATION - t) / 0.25f), Clamp01(t / 0.5f));
        DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, C(110, 60, 150, (int)(50 * a)));
        for (int i = 0; i < 3; ++i) {
            int y = (int)(std::fmod(clock * 40.f + i * 61.f, (float)VIRTUAL_H));
            DrawRectangle(0, y, VIRTUAL_W, 1, ColorAlpha(Pal::Lilac, 0.15f * a));
        }
    }
    if (level.hasEnemy && enemy.active && enemy.state == EnemyState::Chase && screen == Screen::Playing) {
        float p = 0.5f + 0.5f * std::sin(clock * 8.f);
        Color edge = ColorAlpha(Pal::Crimson, 0.35f * p + 0.1f);
        DrawRectangleGradientH(0, 0, 40, VIRTUAL_H, edge, BLANK);
        DrawRectangleGradientH(VIRTUAL_W - 40, 0, 40, VIRTUAL_H, BLANK, edge);
        DrawRectangleGradientV(0, 0, VIRTUAL_W, 24, edge, BLANK);
        DrawRectangleGradientV(0, VIRTUAL_H - 24, VIRTUAL_W, 24, BLANK, edge);
    }
    if (spotFlash > 0.f) DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, ColorAlpha(Pal::Crimson, 0.25f * spotFlash));

    // Rain streaks on the "glass" and restrained film grain.
    for (int i = 0; i < 70; ++i) {
        float x = std::fmod(i * 53.7f + clock * 30.f, (float)VIRTUAL_W + 20.f) - 10.f;
        float y = std::fmod(i * 97.1f + clock * 210.f, (float)VIRTUAL_H + 20.f) - 10.f;
        DrawLine((int)x, (int)y, (int)x - 2, (int)y + 6, ColorAlpha(Pal::Lilac, 0.07f));
    }
    for (int i = 0; i < 40; ++i)
        DrawPixel(GetRandomValue(0, VIRTUAL_W - 1), GetRandomValue(0, VIRTUAL_H - 1),
                  GetRandomValue(0, 1) ? ColorAlpha(Pal::Black, 0.5f) : ColorAlpha(Pal::PaleGrey, 0.08f));
    if (fade > 0.f) DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, ColorAlpha(BLACK, Clamp01(fade)));
}

// -------------------------------------------------------------------------------------
// Title / ending backdrop: Halvorsen Court in the rain, one window lit, a shape inside.
void Game::drawTitleBackdrop(bool ending) {
    DrawRectangleGradientV(0, 0, VIRTUAL_W, VIRTUAL_H, C(24, 14, 34), C(6, 4, 10));
    if (lightning > 0.f) DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, ColorAlpha(Pal::Lilac, lightning * 0.5f));
    // neighbouring buildings
    DrawRectangle(10, 90, 70, 90, C(12, 9, 16));
    DrawRectangle(250, 80, 60, 100, C(12, 9, 16));
    // Halvorsen Court
    int bx = 110, by = 36, bw = 100;
    DrawRectangle(bx, by, bw, VIRTUAL_H - by, C(16, 11, 22));
    DrawRectangle(bx - 4, by - 4, bw + 8, 6, C(20, 14, 28));
    for (int row = 0; row < 6; ++row)
        for (int col = 0; col < 5; ++col) {
            int wx = bx + 8 + col * 18, wy = by + 10 + row * 22;
            bool lit = (row == 1 && col == 2);
            Color wc = lit ? C(120, 40, 50) : ((H2(row, col) % 4 == 0) ? C(40, 34, 30) : C(24, 18, 30));
            DrawRectangle(wx, wy, 10, 14, wc);
            if (lit) {
                drawSprite("tenant", wx + 5.f, wy + 14.f + 6.f, ColorAlpha(BLACK, 0.95f));
                DrawRectangle(wx, wy, 10, 1, C(160, 60, 70));
            }
        }
    DrawRectangle(bx + 40, VIRTUAL_H - 22, 20, 22, C(10, 7, 14));
    if (ending) {
        // A small figure standing in the street.
        drawSprite("player", 160.f, VIRTUAL_H - 4.f, WHITE, 2, 8);
    }
    for (int i = 0; i < 140; ++i) {
        float x = std::fmod(i * 37.3f + clock * 40.f, (float)VIRTUAL_W + 20.f) - 10.f;
        float y = std::fmod(i * 91.7f + clock * 230.f, (float)VIRTUAL_H + 20.f) - 10.f;
        DrawLine((int)x, (int)y, (int)x - 2, (int)y + 7, ColorAlpha(Pal::Lilac, 0.16f));
    }
    for (int i = 0; i < 50; ++i)
        DrawPixel(GetRandomValue(0, VIRTUAL_W - 1), GetRandomValue(0, VIRTUAL_H - 1), ColorAlpha(Pal::Black, 0.6f));
}

// -------------------------------------------------------------------------------------
void Game::draw() {
    BeginTextureMode(worldRT);
    ClearBackground(Pal::Black);
    switch (screen) {
        case Screen::Title:
        case Screen::Controls: drawTitleBackdrop(false); break;
        case Screen::Victory: drawTitleBackdrop(true); break;
        default: drawWorld(); break;
    }
    EndTextureMode();

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float scale = std::min(sw / (float)VIRTUAL_W, sh / (float)VIRTUAL_H);
    viewport = {(sw - VIRTUAL_W * scale) * 0.5f, (sh - VIRTUAL_H * scale) * 0.5f, VIRTUAL_W * scale, VIRTUAL_H * scale};

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(worldRT.texture, {0, 0, (float)VIRTUAL_W, -(float)VIRTUAL_H}, viewport, {0, 0}, 0.f, WHITE);

    // UI is drawn directly to the screen in a 640x360 virtual space (crisp pixel text).
    Camera2D uiCam{};
    uiCam.offset = {viewport.x, viewport.y};
    uiCam.zoom = viewport.width / (float)UI_W;
    BeginMode2D(uiCam);
    drawUI();
    EndMode2D();

    // Keep the CRT treatment restrained and let players disable it in Settings.
    if (screenEffects) {
        for (int y = (int)viewport.y; y < (int)(viewport.y + viewport.height); y += 3)
            DrawRectangle((int)viewport.x, y, (int)viewport.width, 1, ColorAlpha(BLACK, 0.09f));
        int vw = (int)(viewport.width * 0.06f);
        DrawRectangleGradientH((int)viewport.x, (int)viewport.y, vw, (int)viewport.height,
                               ColorAlpha(BLACK, 0.34f), BLANK);
        DrawRectangleGradientH((int)(viewport.x + viewport.width) - vw, (int)viewport.y, vw,
                               (int)viewport.height, BLANK, ColorAlpha(BLACK, 0.34f));
    }
    EndDrawing();
}
