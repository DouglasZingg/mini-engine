#pragma once
#include <vector>

bool IsWalkableProcTile(int v);
int CountReachableFloorTiles(const std::vector<int>& tiles, int w, int h, int sx, int sy);
int CountTileValue(const std::vector<int>& tiles, int value);
