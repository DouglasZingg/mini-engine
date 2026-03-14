#pragma once
#include <string>

// Returns a best-effort absolute path to an asset relative to the project.
std::string AssetPath(const char* relPath);
