// AFTERIMAGE - entry point.
//   afterimage                    play
//   afterimage --selftest         automated checks + screenshots, then exit
//   afterimage --export-sprites   write the procedural sprites to sprites_procedural/
#include <cstring>
#include <string>
#include "Art.h"
#include "Config.h"
#include "Game.h"
#include "raylib.h"

int main(int argc, char** argv) {
    bool selfTest = false, exportSprites = false, freshAIProfile = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--selftest") == 0) selfTest = true;
        if (std::strcmp(argv[i], "--export-sprites") == 0) exportSprites = true;
        if (std::strcmp(argv[i], "--fresh-ai") == 0) freshAIProfile = true;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_W, WINDOW_H, "AFTERIMAGE");
    SetWindowMinSize(640, 360);
    SetExitKey(KEY_NULL);  // ESC is used for the pause menu
    SetTargetFPS(60);

    if (exportSprites) {
        MakeDirectory("sprites_procedural");
        Art::exportProcedural("sprites_procedural");
        TraceLog(LOG_INFO, "Exported procedural sprites to sprites_procedural/");
        CloseWindow();
        return 0;
    }

    // Run from the executable's folder so assets and save data resolve regardless of launch directory.
    ChangeDirectory(GetApplicationDirectory());

    Game game;
    game.init(selfTest, freshAIProfile);
    while (!WindowShouldClose() && !game.quit) {
        float dt = GetFrameTime();
        if (dt > 1.f / 30.f) dt = 1.f / 30.f;  // avoid tunnelling after a hitch
        if (selfTest) dt = 1.f / 60.f;         // deterministic self test
        game.update(dt);
        game.draw();
        game.afterDraw();
    }
    game.shutdown();
    CloseWindow();
    return 0;
}
