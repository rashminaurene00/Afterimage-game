// AFTERIMAGE - global constants, palette and gameplay tuning.
#pragma once
#include "raylib.h"

constexpr int TILE = 16;          // tile size in world pixels
constexpr int VIRTUAL_W = 320;    // low internal resolution (world is rendered here)
constexpr int VIRTUAL_H = 180;
constexpr int UI_W = 640;         // UI is laid out in a 640x360 virtual space
constexpr int UI_H = 360;
constexpr int WINDOW_W = 1280;
constexpr int WINDOW_H = 720;

// Limited 1990s-style palette: violet, purple, black, dirty grey, faded green, crimson.
namespace Pal {
inline const Color Black        {  8,   6,  12, 255};
inline const Color Void         { 14,  10,  20, 255};
inline const Color DeepViolet   { 34,  22,  48, 255};
inline const Color Violet       { 62,  40,  84, 255};
inline const Color Muted        { 96,  74, 118, 255};
inline const Color Lilac        {150, 126, 170, 255};
inline const Color Grey         { 88,  86,  92, 255};
inline const Color DirtyGrey    {128, 122, 118, 255};
inline const Color PaleGrey     {190, 184, 176, 255};
inline const Color DarkGreen    { 38,  56,  44, 255};
inline const Color FadedGreen   { 92, 120,  96, 255};
inline const Color PaleGreen    {150, 180, 140, 255};
inline const Color Crimson      {150,  24,  36, 255};
inline const Color BrightCrimson{210,  40,  52, 255};
inline const Color Bone         {214, 204, 180, 255};
}  // namespace Pal

namespace Tune {
constexpr float PLAYER_WALK = 24.f;     // px/s (slow default to leave time to read and explore)
constexpr float PLAYER_SPRINT = 42.f;   // px/s (faster, noisy, and uses stamina)
constexpr float STAMINA_DRAIN = 0.28f;  // per second while sprinting
constexpr float STAMINA_REGEN = 0.20f;

constexpr float ENEMY_PATROL = 18.f;
constexpr float ENEMY_INVESTIGATE = 24.f;
constexpr float ENEMY_SEARCH = 22.f;
constexpr float ENEMY_RETURN = 19.f;
constexpr float ENEMY_CHASE = 31.f;     // can be escaped with a sprint

constexpr float AFTERIMAGE_DURATION = 4.0f;
constexpr float AFTERIMAGE_COOLDOWN = 9.0f;
}  // namespace Tune
