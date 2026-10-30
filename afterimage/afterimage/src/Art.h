// AFTERIMAGE - sprite manager.
// Every sprite has a procedural pixel-art version generated at startup, so the game
// always runs. If assets/sprites/<key>.png exists (e.g. rendered from Blender with
// blender/render_props.py and tools/pixelate_sprites.py) it replaces the procedural one.
#pragma once
#include <map>
#include <string>
#include <vector>
#include "raylib.h"

struct CharacterAnimation {
    // Fixed production contract: 32x48 frames, four 8-frame directional rows.
    // Each row is ordered down, up, left, right; columns are idle(0), walk(1-4),
    // run(5-6), turn(7). A per-state atlas may use fewer frames where documented.
    Texture2D atlas{};
    int frameWidth = 32;
    int frameHeight = 48;
    int columns = 8;
    int rows = 4;
    bool loadedFromFile = false;
};

class Art {
public:
    void load();
    void unload();
    Texture2D get(const std::string& key) const;
    bool fromFile(const std::string& key) const;
    int fileCount() const;
    const CharacterAnimation& character(const std::string& key) const;
    bool hasProductionCharacterArt(const std::string& key) const;
    // Writes every procedural sprite to dir/<key>.png (reference sheet for artists).
    static void exportProcedural(const std::string& dir);

private:
    std::map<std::string, Texture2D> tex;
    std::map<std::string, bool> loadedFromFile;
    std::map<std::string, CharacterAnimation> characters;
};
