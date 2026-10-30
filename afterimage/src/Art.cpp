#include "Art.h"
#include <string>
#include "Config.h"

namespace {

const char* kKeys[] = {"player",     "tenant",     "door",       "door_open",   "exit_locked",
                       "exit_open",  "cabinet",    "crate",      "fusebox",     "fusebox_on",
                       "wardrobe",   "alcove",     "note",       "bed",         "table",
                       "chair",      "shelf",      "switch_off", "switch_on",   "terminal",
                       "terminal_on"};
constexpr int kKeyCount = (int)(sizeof(kKeys) / sizeof(kKeys[0]));

Color C(int r, int g, int b, int a = 255) {
    return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};
}
void R(Image& im, int x, int y, int w, int h, Color c) { ImageDrawRectangle(&im, x, y, w, h, c); }
void P(Image& im, int x, int y, Color c) { ImageDrawPixel(&im, x, y, c); }

// Adds a 1px silhouette outline around opaque pixels for readability in the dark.
void Outline(Image& im, Color oc) {
    Image src = ImageCopy(im);
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int y = 0; y < im.height; ++y)
        for (int x = 0; x < im.width; ++x) {
            if (GetImageColor(src, x, y).a != 0) continue;
            for (int k = 0; k < 4; ++k) {
                int nx = x + dx[k], ny = y + dy[k];
                if (nx < 0 || ny < 0 || nx >= im.width || ny >= im.height) continue;
                if (GetImageColor(src, nx, ny).a > 0) {
                    P(im, x, y, oc);
                    break;
                }
            }
        }
    UnloadImage(src);
}

Image PlayerSheet() {
    // 8 frames of 16x16: (down0, down1, up0, up1, left0, left1, right0, right1)
    Image im = GenImageColor(128, 16, BLANK);
    Color coat = C(92, 74, 110), coatD = C(60, 44, 76), skin = C(206, 190, 168);
    Color hair = C(40, 30, 38), legs = C(36, 32, 40), eye = C(20, 14, 22);
    for (int dir = 0; dir < 4; ++dir)
        for (int f = 0; f < 2; ++f) {
            int ox = (dir * 2 + f) * 16;
            R(im, ox + 4, 14, 8, 2, C(0, 0, 0, 90));
            R(im, ox + 5, 11, 2, f ? 2 : 3, legs);
            R(im, ox + 9, 11, 2, f ? 3 : 2, legs);
            R(im, ox + 4, 6, 8, 6, coat);
            R(im, ox + 4, 6, 8, 1, coatD);
            if (dir < 2) {
                R(im, ox + 3, 7, 1, 4, coatD);
                R(im, ox + 12, 7, 1, 4, coatD);
            } else {
                R(im, ox + 7 + (f ? 1 : -1), 7, 2, 4, coatD);
            }
            R(im, ox + 5, 1, 6, 5, skin);
            switch (dir) {
                case 0:
                    R(im, ox + 5, 1, 6, 2, hair);
                    P(im, ox + 6, 4, eye);
                    P(im, ox + 9, 4, eye);
                    break;
                case 1: R(im, ox + 5, 1, 6, 5, hair); break;
                case 2:
                    R(im, ox + 7, 1, 4, 5, hair);
                    R(im, ox + 5, 1, 2, 1, hair);
                    P(im, ox + 6, 3, eye);
                    break;
                case 3:
                    R(im, ox + 5, 1, 4, 5, hair);
                    R(im, ox + 9, 1, 2, 1, hair);
                    P(im, ox + 9, 3, eye);
                    break;
            }
        }
    Outline(im, C(10, 8, 14, 220));
    return im;
}

Image Tenant() {
    // Tall, too-thin silhouette with long arms and a crimson eye slit. 16x24.
    Image im = GenImageColor(16, 24, BLANK);
    Color b = C(10, 8, 14), d = C(26, 18, 34);
    R(im, 6, 1, 5, 5, b);
    R(im, 7, 0, 3, 1, b);
    P(im, 7, 3, Pal::Crimson);
    P(im, 8, 3, Pal::BrightCrimson);
    P(im, 9, 3, Pal::Crimson);
    R(im, 7, 6, 2, 2, b);
    R(im, 5, 8, 7, 3, b);
    R(im, 6, 11, 5, 6, b);
    R(im, 6, 11, 1, 6, d);
    R(im, 4, 9, 1, 11, b);
    R(im, 12, 9, 1, 11, b);
    P(im, 3, 20, b);
    P(im, 4, 21, b);
    P(im, 13, 20, b);
    P(im, 12, 21, b);
    R(im, 6, 17, 2, 6, b);
    R(im, 9, 17, 2, 6, b);
    Outline(im, C(70, 40, 90, 230));
    return im;
}

Image Door(bool open) {
    Image im = GenImageColor(16, 16, BLANK);
    Color frame = C(40, 26, 30), panel = C(74, 50, 52), inset = C(62, 40, 44);
    if (!open) {
        R(im, 0, 0, 16, 16, frame);
        R(im, 2, 1, 12, 15, panel);
        R(im, 3, 2, 10, 5, inset);
        R(im, 3, 9, 10, 5, inset);
        P(im, 11, 8, C(190, 170, 120));
        R(im, 2, 1, 12, 1, C(96, 70, 70));
    } else {
        R(im, 0, 0, 2, 16, frame);
        R(im, 14, 0, 2, 16, frame);
        R(im, 2, 0, 3, 16, panel);
        R(im, 2, 0, 1, 16, inset);
    }
    return im;
}

Image ExitDoor(bool unlocked) {
    Image im = GenImageColor(16, 16, BLANK);
    R(im, 0, 0, 16, 16, C(30, 30, 36));
    R(im, 2, 2, 12, 14, C(70, 72, 80));
    R(im, 3, 4, 10, 1, C(52, 54, 62));
    R(im, 3, 8, 10, 1, C(52, 54, 62));
    R(im, 3, 12, 10, 1, C(52, 54, 62));
    Color lamp = unlocked ? C(110, 210, 120) : Pal::BrightCrimson;
    R(im, 6, 0, 4, 2, lamp);
    P(im, 11, 9, C(190, 180, 150));
    return im;
}

Image Cabinet() {
    Image im = GenImageColor(16, 16, BLANK);
    R(im, 1, 3, 14, 12, C(66, 50, 58));
    R(im, 1, 2, 14, 2, C(92, 72, 80));
    R(im, 2, 6, 12, 3, C(52, 38, 46));
    R(im, 2, 10, 12, 3, C(52, 38, 46));
    P(im, 8, 7, C(170, 150, 100));
    P(im, 8, 11, C(170, 150, 100));
    Outline(im, C(12, 8, 14, 220));
    return im;
}

Image Crate() {
    Image im = GenImageColor(16, 16, BLANK);
    Color wood = C(92, 76, 58), dark = C(62, 50, 38), light = C(116, 98, 74);
    R(im, 1, 2, 14, 13, wood);
    R(im, 1, 2, 14, 1, light);
    R(im, 1, 2, 1, 13, dark);
    R(im, 14, 2, 1, 13, dark);
    R(im, 1, 14, 14, 1, dark);
    for (int i = 0; i < 12; ++i) {
        P(im, 2 + i, 3 + i, dark);
        P(im, 13 - i, 3 + i, dark);
    }
    Outline(im, C(12, 8, 14, 220));
    return im;
}

Image FuseBox(bool on) {
    Image im = GenImageColor(16, 16, BLANK);
    R(im, 3, 1, 10, 14, C(88, 92, 96));
    R(im, 4, 2, 8, 12, C(58, 62, 68));
    R(im, 5, 4, 6, 2, on ? C(150, 140, 110) : C(36, 38, 42));
    R(im, 5, 7, 6, 2, C(150, 140, 110));
    R(im, 5, 10, 6, 2, C(150, 140, 110));
    R(im, 10, 13, 2, 1, on ? C(110, 210, 120) : Pal::Crimson);
    Outline(im, C(12, 8, 14, 220));
    return im;
}

Image Wardrobe() {
    Image im = GenImageColor(16, 24, BLANK);
    R(im, 1, 1, 14, 22, C(46, 32, 36));
    R(im, 2, 3, 6, 19, C(64, 44, 48));
    R(im, 8, 3, 6, 19, C(64, 44, 48));
    R(im, 8, 3, 1, 19, C(30, 20, 24));
    R(im, 1, 1, 14, 2, C(80, 58, 62));
    P(im, 7, 12, C(170, 150, 100));
    P(im, 9, 12, C(170, 150, 100));
    Outline(im, C(12, 8, 14, 220));
    return im;
}

Image Alcove() {
    Image im = GenImageColor(16, 16, BLANK);
    R(im, 0, 0, 16, 16, C(6, 4, 8));
    R(im, 0, 0, 3, 16, C(70, 22, 36));
    R(im, 13, 0, 3, 16, C(70, 22, 36));
    R(im, 1, 0, 1, 16, C(50, 14, 26));
    R(im, 14, 0, 1, 16, C(50, 14, 26));
    R(im, 0, 0, 16, 1, C(40, 26, 30));
    return im;
}

Image Note() {
    Image im = GenImageColor(16, 16, BLANK);
    R(im, 4, 5, 8, 7, C(200, 192, 170));
    for (int y = 6; y < 11; y += 2) R(im, 5, y, 6, 1, C(120, 112, 104));
    P(im, 10, 10, Pal::Crimson);
    Outline(im, C(12, 8, 14, 200));
    return im;
}

Image Bed() {
    Image im = GenImageColor(16, 16, BLANK);
    R(im, 1, 1, 14, 14, C(54, 40, 44));
    R(im, 2, 2, 12, 3, C(180, 176, 170));
    R(im, 2, 6, 12, 8, C(70, 52, 90));
    R(im, 2, 6, 12, 1, C(96, 74, 118));
    Outline(im, C(12, 8, 14, 220));
    return im;
}

Image Table() {
    Image im = GenImageColor(16, 16, BLANK);
    R(im, 1, 4, 14, 8, C(80, 60, 52));
    R(im, 1, 4, 14, 1, C(104, 82, 70));
    R(im, 2, 12, 2, 3, C(50, 36, 32));
    R(im, 12, 12, 2, 3, C(50, 36, 32));
    R(im, 4, 6, 2, 2, C(190, 184, 176));
    R(im, 10, 7, 2, 2, C(190, 184, 176));
    Outline(im, C(12, 8, 14, 220));
    return im;
}

Image Chair() {
    Image im = GenImageColor(16, 16, BLANK);
    R(im, 4, 2, 8, 3, C(70, 52, 46));
    R(im, 4, 6, 8, 5, C(86, 64, 56));
    R(im, 4, 11, 1, 4, C(50, 36, 32));
    R(im, 11, 11, 1, 4, C(50, 36, 32));
    Outline(im, C(12, 8, 14, 220));
    return im;
}

Image Shelf() {
    Image im = GenImageColor(16, 16, BLANK);
    R(im, 0, 1, 16, 14, C(52, 40, 40));
    const Color books[4] = {C(84, 40, 52), C(60, 70, 62), C(80, 70, 96), C(110, 100, 86)};
    for (int row = 0; row < 2; ++row)
        for (int i = 0; i < 7; ++i)
            R(im, 1 + i * 2, 2 + row * 6, 2, 5, books[(i + row * 3) % 4]);
    R(im, 0, 7, 16, 1, C(36, 26, 26));
    R(im, 0, 14, 16, 1, C(30, 22, 22));
    return im;
}

Image Switch(bool on) {
    Image im = GenImageColor(16, 16, BLANK);
    R(im, 4, 3, 8, 11, C(80, 80, 86));
    R(im, 5, 4, 6, 9, C(52, 52, 58));
    if (on) {
        R(im, 7, 8, 2, 5, C(170, 160, 140));
        R(im, 6, 12, 4, 2, C(200, 190, 170));
    } else {
        R(im, 7, 4, 2, 5, C(170, 160, 140));
        R(im, 6, 3, 4, 2, C(200, 190, 170));
    }
    R(im, 10, 13, 2, 1, on ? C(110, 210, 120) : Pal::Crimson);
    Outline(im, C(12, 8, 14, 220));
    return im;
}

Image Terminal(bool on) {
    Image im = GenImageColor(16, 16, BLANK);
    R(im, 1, 3, 14, 11, C(54, 56, 60));
    R(im, 3, 4, 10, 6, on ? C(40, 90, 60) : C(14, 18, 16));
    if (on) {
        R(im, 4, 5, 6, 1, C(150, 220, 160));
        R(im, 4, 7, 4, 1, C(150, 220, 160));
    }
    R(im, 3, 11, 10, 2, C(36, 38, 42));
    Outline(im, C(12, 8, 14, 220));
    return im;
}

Image Generate(const std::string& k) {
    if (k == "player") return PlayerSheet();
    if (k == "tenant") return Tenant();
    if (k == "door") return Door(false);
    if (k == "door_open") return Door(true);
    if (k == "exit_locked") return ExitDoor(false);
    if (k == "exit_open") return ExitDoor(true);
    if (k == "cabinet") return Cabinet();
    if (k == "crate") return Crate();
    if (k == "fusebox") return FuseBox(false);
    if (k == "fusebox_on") return FuseBox(true);
    if (k == "wardrobe") return Wardrobe();
    if (k == "alcove") return Alcove();
    if (k == "note") return Note();
    if (k == "bed") return Bed();
    if (k == "table") return Table();
    if (k == "chair") return Chair();
    if (k == "shelf") return Shelf();
    if (k == "switch_off") return Switch(false);
    if (k == "switch_on") return Switch(true);
    if (k == "terminal") return Terminal(false);
    if (k == "terminal_on") return Terminal(true);
    return GenImageColor(16, 16, MAGENTA);
}

}  // namespace

void Art::load() {
    for (int i = 0; i < kKeyCount; ++i) {
        std::string key = kKeys[i];
        std::string file = "assets/sprites/" + key + ".png";
        Texture2D t{};
        bool fromFile = false;
        if (FileExists(file.c_str())) {
            t = LoadTexture(file.c_str());
            fromFile = t.id != 0;
        }
        if (!fromFile) {
            Image im = Generate(key);
            t = LoadTextureFromImage(im);
            UnloadImage(im);
        }
        SetTextureFilter(t, TEXTURE_FILTER_POINT);
        tex[key] = t;
        loadedFromFile[key] = fromFile;
    }
    for (const char* key : {"player", "tenant"}) {
        CharacterAnimation a;
        a.frameWidth = std::string(key) == "player" ? 32 : 40;
        a.frameHeight = 48;
        a.columns = 8;
        a.rows = 4;
        const std::string base = std::string("assets/sprites/") + key;
        const std::string atlasFile = base + "_atlas.png";
        if (FileExists(atlasFile.c_str())) {
            Texture2D t = LoadTexture(atlasFile.c_str());
            if (t.id && t.width % a.columns == 0 && t.height % a.rows == 0 &&
                t.width / a.columns >= 16 && t.height / a.rows >= 24) {
                a.atlas = t;
                a.frameWidth = t.width / a.columns;
                a.frameHeight = t.height / a.rows;
                a.loadedFromFile = true;
                SetTextureFilter(t, TEXTURE_FILTER_POINT);
            } else if (t.id) {
                UnloadTexture(t);
                TraceLog(LOG_WARNING, "ART: ignoring malformed %s", atlasFile.c_str());
            }
        }
        characters[key] = a;
    }
    TraceLog(LOG_INFO, "ART: %d sprites (%d loaded from assets/sprites, rest procedural)", kKeyCount,
             fileCount());
}

void Art::unload() {
    for (auto& kv : tex) UnloadTexture(kv.second);
    for (auto& kv : characters) if (kv.second.atlas.id) UnloadTexture(kv.second.atlas);
    tex.clear();
    characters.clear();
}

Texture2D Art::get(const std::string& key) const {
    auto it = tex.find(key);
    if (it != tex.end()) return it->second;
    return Texture2D{};
}

bool Art::fromFile(const std::string& key) const {
    auto it = loadedFromFile.find(key);
    return it != loadedFromFile.end() && it->second;
}

int Art::fileCount() const {
    int n = 0;
    for (auto& kv : loadedFromFile) n += kv.second ? 1 : 0;
    return n;
}

const CharacterAnimation& Art::character(const std::string& key) const {
    static const CharacterAnimation fallback{};
    auto it = characters.find(key);
    return it == characters.end() ? fallback : it->second;
}

bool Art::hasProductionCharacterArt(const std::string& key) const {
    return character(key).loadedFromFile;
}

void Art::exportProcedural(const std::string& dir) {
    for (int i = 0; i < kKeyCount; ++i) {
        Image im = Generate(kKeys[i]);
        std::string path = dir + "/" + kKeys[i] + ".png";
        ExportImage(im, path.c_str());
        UnloadImage(im);
    }
}
