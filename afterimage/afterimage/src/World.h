// AFTERIMAGE - tile map, environmental objects and level container.
#pragma once
#include <map>
#include <string>
#include <vector>
#include "Config.h"
#include "raylib.h"

struct Point {
    int x = 0, y = 0;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Point& o) const { return !(*this == o); }
};

Vector2 TileCenter(Point p);
Point ToTile(Vector2 v);

enum class ObjType {
    Door,       // opens/closes, may be locked
    Exit,       // level transition or ending; usually locked until a condition is met
    Cabinet,    // searchable
    Crate,      // searchable, noisy when empty
    FuseBox,    // needs a fuse
    Wardrobe,   // hiding place
    Alcove,     // hiding place
    Note,       // readable
    Furniture,  // inspectable decoration (solid)
    Shelf,      // archive shelving (solid, blocks sight)
    Switch,     // relay lever
    Terminal    // story terminal
};

struct Object {
    int id = 0;
    ObjType type = ObjType::Furniture;
    char glyph = '?';
    int tx = 0, ty = 0;              // tile position (objects move in whole tiles)
    bool open = false;
    bool locked = false;
    bool on = false;
    bool searched = false;
    bool present = true;
    std::string name;
    std::string sprite;              // sprite key for Furniture
    std::string text;                // note text / inspect text
    std::string needItem;            // item required to unlock / use
    std::string holds;               // item found when searched
    std::string lockedMsg;
    int echoOrder = 0;               // >0: number revealed by the afterimage (archive levers)
    int target = -1;                 // Exit: next level index, -1 = ending, -2 = never opens

    bool isSolid() const;
    bool blocksSight() const;
    bool isInteractable() const;
};

struct Level {
    int index = 0;
    std::string name;
    int w = 0, h = 0;
    std::vector<std::string> tiles;  // '#' wall, ' ' void, '.' planks, ',' carpet, '~' tiles
    std::vector<Object> objects;
    std::vector<Point> waypoints;    // enemy patrol route (map digits 0-9, in order)
    std::map<char, Point> markers;   // script markers (unrecognised map letters)
    Point playerStart{1, 1};
    Point enemySpawn{1, 1};
    bool hasEnemy = false;

    void refresh();                  // rebuild the object occupancy grid
    bool inBounds(int x, int y) const;
    char tileAt(int x, int y) const;
    bool isWall(int x, int y) const;
    Object* objectAt(int x, int y);
    const Object* objectAt(int x, int y) const;
    Object* findGlyph(char c);
    bool solidForPlayer(int x, int y) const;
    bool passableForEnemy(int x, int y) const;
    bool blocksSight(int x, int y) const;
    // True if nothing blocks sight between a and b (the tile containing b is not tested,
    // so walls and doors facing the viewer count as visible).
    bool lineOfSight(Vector2 a, Vector2 b) const;

private:
    std::vector<int> occ;
};

constexpr int LEVEL_COUNT = 4;
bool BuildLevel(int index, Level& out);
