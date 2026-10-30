// AFTERIMAGE - grid A* pathfinding (4-directional, Manhattan heuristic).
#pragma once
#include <vector>
#include "World.h"

// Returns the tiles to walk through from start to goal, excluding the start tile.
// Empty if start == goal or the goal is unreachable. Uses Level::passableForEnemy.
std::vector<Point> FindPath(const Level& lv, Point start, Point goal);
