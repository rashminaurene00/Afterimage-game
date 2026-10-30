// AFTERIMAGE - the four areas: maps, objects and environmental story text.
//
// Map legend (shared by every level):
//   '#' wall   ' ' void   '.' planks   ',' carpet   '~' tiled floor
//   'P' player start   'E' enemy spawn   '0'-'9' enemy patrol waypoints (in order)
//   'D' door   'W' wardrobe (hide)   'H' dark alcove (hide)   'x' archive shelf
// Any other letter is handled by the level's own handler below, or becomes a
// script marker (for example 'g' = where the apparition appears in Area 1).
#include <algorithm>
#include <functional>
#include "World.h"

namespace {

using Handler = std::function<bool(Level&, char, int, int)>;

// ---------------------------------------------------------------------------------
// AREA 1 - APARTMENT 4C (bedroom, living room, kitchen, hall).
// ---------------------------------------------------------------------------------
const std::vector<std::string> kArea1 = {
    "##############################",
    "#,,,,,,,#...0.............1..#",
    "#,B,,,,,#.....a....b....c....#",
    "#,,,,,,,#............E.......#",
    "#,,P,,n,#....................#",
    "#,,,,,,,D.......h.T..........#",
    "#,W,,,,,#....................#",
    "#,,,,,,,#...3.............2..#",
    "####D####.....###D############",
    "#.......#.....#~~~~~~~~~~~~~~#",
    "#...g...#.....#~~~~~~~~~~~~~~#",
    "X.......D.....D~~~~~~~~~~~~F~#",
    "#.......#.....#~~~~~k~~~~~~~~#",
    "#.......#.....#~~~~~~~~~~~~~~#",
    "##############################",
};

// ---------------------------------------------------------------------------------
// AREA 2 - THE REPEATING CORRIDOR. Walking off the east end loops you to the west.
// ---------------------------------------------------------------------------------
const std::vector<std::string> kArea2 = {
    "######################################Y###",
    "#....#....#....#....#....#....#....#.....#",
    "#.W..#.c..#..d.#.e..#.3r.#..f.#.W..#.....#",
    "#....#....#....#....#....#....#....#.....#",
    "###D####D####D####D####D####D####D##.....#",
    "SP..1....................................L",
    "#.n.......................E..............L",
    "###D####D####D####D####D####D####D##.....#",
    "#....#....#....#....#....#....#....#..2..#",
    "#.g..#....#.i..#....#.j..#....#.k..#....H#",
    "#....#....#....#....#....#....#....#.....#",
    "##########################################",
};

// ---------------------------------------------------------------------------------
// AREA 3 - THE ARCHIVE. Shelving aisles, four relay levers, the terminal, the exit.
// ---------------------------------------------------------------------------------
const std::vector<std::string> kArea3 = {
    "##Z###############################",
    "#.............................E..#",
    "#.a.............................b#",
    "#................................#",
    "#..xxxxx..xxxxx....xxxxx..xxxxx..#",
    "#.......1.....................2..#",
    "#..xxxxx..xxxxx....xxxxx..xxxxx..#",
    "#.W.............n..............H.#",
    "#..xxxxx..xxxxx....xxxxx..xxxxx..#",
    "#.......3.........o...........4..#",
    "#..xxxxx..xxxxx....xxxxx..xxxxx..#",
    "#.c............r...............d.#",
    "#...............P.........T......#",
    "################S#################",
};

// ---------------------------------------------------------------------------------
// AREA 4 - THE RESIDUE WARD. The last recorded visit and the way outside.
// ---------------------------------------------------------------------------------
const std::vector<std::string> kArea4 = {
    "####################################",
    "#P....q........#...................#",
    "#..............D......W............#",
    "#..n...........#....s...............#",
    "#..............#....#######.........#",
    "#####D##########....#..t..#.........#",
    "#.............m.....#..c..D.........#",
    "#....E..............#.....#.........#",
    "#...................####D##.........#",
    "#...........H...........f...........#",
    "#..............D..............l.....#",
    "#..............#.....g..............#",
    "#..............#....................#",
    "#..............D...................Z#",
    "####################################",
};

Object& Add(Level& lv, ObjType type, int x, int y, char glyph, const std::string& name,
            const std::string& sprite) {
    Object o;
    o.id = (int)lv.objects.size() + 1;
    o.type = type;
    o.glyph = glyph;
    o.tx = x;
    o.ty = y;
    o.name = name;
    o.sprite = sprite;
    lv.objects.push_back(o);
    return lv.objects.back();
}

char GuessFloor(const std::vector<std::string>& t, int x, int y, char def) {
    const char floors[3] = {'.', ',', '~'};
    int counts[3] = {0, 0, 0};
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; ++k) {
        int nx = x + dx[k], ny = y + dy[k];
        if (ny < 0 || ny >= (int)t.size() || nx < 0 || nx >= (int)t[ny].size()) continue;
        for (int f = 0; f < 3; ++f)
            if (t[ny][nx] == floors[f]) counts[f]++;
    }
    int best = -1, bestCount = 0;
    for (int f = 0; f < 3; ++f)
        if (counts[f] > bestCount) { bestCount = counts[f]; best = f; }
    return best < 0 ? def : floors[best];
}

void Parse(Level& lv, std::vector<std::string> src, char defFloor, const Handler& custom) {
    size_t w = 0;
    for (auto& r : src) w = std::max(w, r.size());
    for (auto& r : src) r.resize(w, '#');
    lv.w = (int)w;
    lv.h = (int)src.size();
    lv.tiles = src;
    std::map<char, Point> wps;

    for (int y = 0; y < lv.h; ++y) {
        for (int x = 0; x < lv.w; ++x) {
            char c = src[y][x];
            if (c == '#' || c == ' ' || c == '.' || c == ',' || c == '~') continue;
            lv.tiles[y][x] = GuessFloor(src, x, y, defFloor);
            if (c == 'P') { lv.playerStart = {x, y}; continue; }
            if (c == 'E') { lv.enemySpawn = {x, y}; lv.hasEnemy = true; continue; }
            if (c >= '0' && c <= '9') { wps[c] = {x, y}; continue; }
            if (custom && custom(lv, c, x, y)) continue;
            switch (c) {
                case 'D': Add(lv, ObjType::Door, x, y, c, "Door", "door"); break;
                case 'W': Add(lv, ObjType::Wardrobe, x, y, c, "Wardrobe", "wardrobe"); break;
                case 'H': Add(lv, ObjType::Alcove, x, y, c, "Dark alcove", "alcove"); break;
                case 'x': Add(lv, ObjType::Shelf, x, y, c, "Shelf", "shelf"); break;
                default: lv.markers[c] = {x, y}; break;
            }
        }
    }
    for (auto& kv : wps) lv.waypoints.push_back(kv.second);
}

// ------------------------------- Area 1 objects -----------------------------------
bool Area1(Level& lv, char c, int x, int y) {
    switch (c) {
        case 'B': {
            Object& o = Add(lv, ObjType::Furniture, x, y, c, "Bed", "bed");
            o.text = "The sheets are cold. The mattress holds a dent in your exact shape - "
                     "pressed in deeper than one night could make.";
            return true;
        }
        case 'n': {
            Object& o = Add(lv, ObjType::Note, x, y, c, "Note on the floor", "note");
            o.text =
                "VISIT 47.\n\n"
                "If you are reading this, you came back again. You always wake up in this bed. "
                "You never remember why.\n\n"
                "The building keeps copies of things. Where they were. Where you walked. "
                "Something here uses those copies.\n\n"
                "Don't trust what a room IS. Trust what it USED to be.\n\n"
                "(The handwriting is yours.)";
            return true;
        }
        case 'a':
        case 'b':
        case 'c': {
            Object& o = Add(lv, ObjType::Cabinet, x, y, c, "Cabinet", "cabinet");
            if (c == 'b') o.holds = "Fuse";
            return true;
        }
        case 'T': {
            Object& o = Add(lv, ObjType::Furniture, x, y, c, "Table", "table");
            o.text = "A table set for two. One cup is full of rainwater. The other was wiped clean "
                     "very recently.";
            return true;
        }
        case 'h': {
            Object& o = Add(lv, ObjType::Furniture, x, y, c, "Chair", "chair");
            o.text = "A chair, pulled out as if someone has only just stood up.";
            return true;
        }
        case 'F': {
            Object& o = Add(lv, ObjType::FuseBox, x, y, c, "Fuse box", "fusebox");
            o.needItem = "Fuse";
            return true;
        }
        case 'X': {
            Object& o = Add(lv, ObjType::Exit, x, y, c, "Front door", "exit");
            o.locked = true;
            o.target = 1;
            o.lockedMsg = "The front door has an electronic lock. Its panel is dead - no power.";
            return true;
        }
        case 'k': {
            Object& o = Add(lv, ObjType::Note, x, y, c, "Grocery list", "note");
            o.text =
                "- milk\n- batteries\n- FUSES (spares are in the living room cabinets)\n\n"
                "Call Ilse back.\nCall Ilse back.\nCall Ilse back.\n\n"
                "The last line has been traced over so many times the pen has torn the paper.";
            return true;
        }
        default: return false;
    }
}

// ------------------------------- Area 2 objects -----------------------------------
bool Area2(Level& lv, char c, int x, int y) {
    switch (c) {
        case 'S': {
            Object& o = Add(lv, ObjType::Exit, x, y, c, "Way back", "exit");
            o.locked = true;
            o.target = -2;
            o.lockedMsg = "The door you came through is gone. There is only wet wallpaper.";
            return true;
        }
        case 'Y': {
            Object& o = Add(lv, ObjType::Exit, x, y, c, "ARCHIVE door", "exit");
            o.locked = true;
            o.needItem = "Brass Key";
            o.target = 2;
            o.lockedMsg = "A heavy door stencilled ARCHIVE. It needs a brass key.";
            return true;
        }
        case 'c': case 'd': case 'e': case 'f':
        case 'g': case 'i': case 'j': case 'k': {
            Object& o = Add(lv, ObjType::Crate, x, y, c, "Crate", "crate");
            if (c == 'j') o.holds = "Brass Key";
            return true;
        }
        case 'n': {
            Object& o = Add(lv, ObjType::Note, x, y, c, "Scrawled note", "note");
            o.text =
                "YOU CAN'T HEAR IT. YOU CAN ONLY SEE WHAT IT CHANGES.\n\n"
                "It touches things as it walks. The building remembers where they were. "
                "Whatever it moved, it was hiding something in. Look back [Q].\n\n"
                "Searching the wrong boxes is loud. It will come to look.\n\n"
                "Don't walk the same path twice. It learns the paths you wear into the floor.";
            return true;
        }
        case 'r': {
            Object& o = Add(lv, ObjType::Note, x, y, c, "Notice to residents", "note");
            o.text =
                "HALVORSEN COURT - NOTICE TO RESIDENTS\n\n"
                "The 4th floor corridor is closed for repairs. Residents who report that the "
                "corridor 'starts over' are reminded that it has seven doors per side, not "
                "infinitely many.\n\n"
                "Residents who report a tall figure that 'stands like them' are asked to stop.";
            return true;
        }
        default: return false;
    }
}

// ------------------------------- Area 3 objects -----------------------------------
bool Area3(Level& lv, char c, int x, int y) {
    switch (c) {
        case 'a': case 'b': case 'c': case 'd': {
            Object& o = Add(lv, ObjType::Switch, x, y, c, "Relay lever", "switch");
            // The correct order is only visible in the afterimage: c, a, d, b.
            o.echoOrder = (c == 'c') ? 1 : (c == 'a') ? 2 : (c == 'd') ? 3 : 4;
            return true;
        }
        case 'T': {
            Add(lv, ObjType::Terminal, x, y, c, "Terminal", "terminal");
            return true;
        }
        case 'Z': {
            Object& o = Add(lv, ObjType::Exit, x, y, c, "Exit door", "exit");
            o.locked = true;
            o.target = 3;
            o.lockedMsg = "The final record room remains sealed. Power the terminal to release it.";
            return true;
        }
        case 'S': {
            Object& o = Add(lv, ObjType::Exit, x, y, c, "Way back", "exit");
            o.locked = true;
            o.target = -2;
            o.lockedMsg = "Behind this door the corridor is starting over. You won't go back.";
            return true;
        }
        case 'n': {
            Object& o = Add(lv, ObjType::Note, x, y, c, "Archive record 0047", "note");
            o.text =
                "RESIDUE ARCHIVE - HALVORSEN COURT\n\n"
                "Visitor: (your name)\nVisits logged: 47\nStatus: DID NOT LEAVE\n\n"
                "Note: every failed visit leaves a residue - a copy of the visitor's habits, "
                "routes and posture. Residue accumulates.\n\nResidue has begun to walk.";
            return true;
        }
        case 'o': {
            Object& o = Add(lv, ObjType::Note, x, y, c, "Archive record - Ilse", "note");
            o.text =
                "Tenant: Ilse - Apartment 4C.\n"
                "Moved out six years ago. Forwarding address on file.\n\n"
                "Letters received from her, unopened: 112.\n"
                "All addressed to you.";
            return true;
        }
        case 'r': {
            Object& o = Add(lv, ObjType::Note, x, y, c, "Torn page", "note");
            o.text =
                "The four relay levers must be thrown in the order the building remembers.\n\n"
                "You have thrown them before. Every visit. Look back [Q] and you will see the "
                "marks your own hands left.\n\n"
                "Get the order wrong and the levers snap back. It hears that.";
            return true;
        }
        default: return false;
    }
}

bool Area4(Level& lv, char c, int x, int y) {
    switch (c) {
        case 'q': {
            Object& o = Add(lv, ObjType::Furniture, x, y, c, "Intake cot", "bed");
            o.text = "Intake tag 047. The sheets are tied down, though the restraints are buckled from the inside.";
            return true;
        }
        case 's': {
            Object& o = Add(lv, ObjType::Furniture, x, y, c, "Observation mirror", "cabinet");
            o.text = "The mirror is scratched from this side. Its reflection is still facing the old wall.";
            return true;
        }
        case 't': {
            Object& o = Add(lv, ObjType::Furniture, x, y, c, "Chart table", "table");
            o.text = "A patient chart lists your height, gait, dominant hand and the way you turn toward a door.";
            return true;
        }
        case 'm': {
            Object& o = Add(lv, ObjType::Furniture, x, y, c, "Intake chair", "chair");
            o.text = "The chair faces the cot. A second set of footprints ends behind it.";
            return true;
        }
        case 'f': {
            Object& o = Add(lv, ObjType::Furniture, x, y, c, "File shelves", "shelf");
            o.text = "Hundreds of visit folders. Each contains a map of the route you took before you died.";
            return true;
        }
        case 'g': {
            Object& o = Add(lv, ObjType::Furniture, x, y, c, "Imitation frame", "wardrobe");
            o.text = "A narrow frame with your old coat hanging inside. The sleeves are still warm.";
            return true;
        }
        case 'n': {
            Object& o = Add(lv, ObjType::Note, x, y, c, "Residue Ward intake", "note");
            o.text = "RESIDUE WARD - VISIT 47\n\n"
                     "The Tenant was assembled from the versions of you that never made it out. "
                     "This room stores the earliest copy.\n\n"
                     "The stair door is at the far end. The rain is real. Keep walking.";
            return true;
        }
        case 'c': {
            Object& o = Add(lv, ObjType::Crate, x, y, c, "Archive drawer", "crate");
            o.holds = "Ilse's letter";
            return true;
        }
        case 'l': {
            Object& o = Add(lv, ObjType::Note, x, y, c, "Ilse's last letter", "note");
            o.text = "You came looking for me again. I left 112 letters because I hoped one would "
                     "reach you.\n\nI am safe. You can stop coming back now.\n\nIlse";
            return true;
        }
        case 'Z': {
            Object& o = Add(lv, ObjType::Exit, x, y, c, "Outside stair", "exit");
            o.locked = true;
            o.needItem = "Ilse's letter";
            o.target = -1;
            o.lockedMsg = "The stair door has a letter slot. Find what the Tenant hid in the drawer.";
            return true;
        }
        default: return false;
    }
}

}  // namespace

bool BuildLevel(int index, Level& out) {
    out = Level{};
    out.index = index;
    switch (index) {
        case 0:
            out.name = "APARTMENT 4C";
            Parse(out, kArea1, '.', Area1);
            break;
        case 1:
            out.name = "THE REPEATING CORRIDOR";
            Parse(out, kArea2, '.', Area2);
            break;
        case 2:
            out.name = "THE ARCHIVE";
            Parse(out, kArea3, '.', Area3);
            break;
        case 3:
            out.name = "THE RESIDUE WARD";
            Parse(out, kArea4, '.', Area4);
            break;
        default: return false;
    }
    out.refresh();
    return true;
}
