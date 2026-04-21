#include "game/Tilemap.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "engine/Camera2D.h"
#include "platform/SdlPlatform.h"

namespace {
	float ClampFloat(float value, float minValue, float maxValue) {
		return std::max(minValue, std::min(value, maxValue));
	}
}

int Tilemap::At(int x, int y) const {
	if (x < 0 || y < 0 || x >= m_w || y >= m_h) {
		return TileValue(TileType::Wall); // treat out-of-bounds as solid
	}
	return m_tiles[static_cast<size_t>(y) * static_cast<size_t>(m_w) + static_cast<size_t>(x)];
}

bool Tilemap::LoadCSV(const char* path) {
	std::ifstream file(path);
	if (!file) {
		return false;
	}

	m_tiles.clear();
	m_w = 0;
	m_h = 0;

	std::string line;
	while (std::getline(file, line)) {
		if (line.empty()) {
			continue;
		}

		std::stringstream lineStream(line);
		std::string cell;
		std::vector<int> row;

		while (std::getline(lineStream, cell, ',')) {
			row.push_back(std::atoi(cell.c_str()));
		}

		if (m_w == 0) {
			m_w = static_cast<int>(row.size());
		}
		if (static_cast<int>(row.size()) != m_w) {
			return false;
		}

		for (int value : row) {
			m_tiles.push_back(value);
		}
		++m_h;
	}

	return (m_w > 0 && m_h > 0);
}

bool Tilemap::LoadFromData(int width, int height, const std::vector<int>& tiles) {
	if (width <= 0 || height <= 0) {
		return false;
	}
	if (static_cast<int>(tiles.size()) != width * height) {
		return false;
	}

	m_w = width;
	m_h = height;
	m_tiles = tiles;
	return true;
}

bool Tilemap::IsSolidAtWorld(const Vec2& world) const {
	const int tx = static_cast<int>(std::floor(world.x / static_cast<float>(m_tileSize)));
	const int ty = static_cast<int>(std::floor(world.y / static_cast<float>(m_tileSize)));
	return At(tx, ty) == TileValue(TileType::Wall);
}

void Tilemap::ResolveCircleCollision(Vec2& pos, float radius) const {
	const int minX = static_cast<int>(std::floor((pos.x - radius) / m_tileSize));
	const int maxX = static_cast<int>(std::floor((pos.x + radius) / m_tileSize));
	const int minY = static_cast<int>(std::floor((pos.y - radius) / m_tileSize));
	const int maxY = static_cast<int>(std::floor((pos.y + radius) / m_tileSize));

	for (int ty = minY; ty <= maxY; ++ty) {
		for (int tx = minX; tx <= maxX; ++tx) {
			if (At(tx, ty) != TileValue(TileType::Wall)) {
				continue;
			}

			const float left = tx * static_cast<float>(m_tileSize);
			const float top = ty * static_cast<float>(m_tileSize);
			const float right = left + m_tileSize;
			const float bottom = top + m_tileSize;

			const float closestX = ClampFloat(pos.x, left, right);
			const float closestY = ClampFloat(pos.y, top, bottom);

			const float dx = pos.x - closestX;
			const float dy = pos.y - closestY;
			const float distSq = dx * dx + dy * dy;

			if (distSq < radius * radius && distSq > 0.00001f) {
				const float dist = std::sqrt(distSq);
				const float penetration = radius - dist;
				const float nx = dx / dist;
				const float ny = dy / dist;
				pos.x += nx * penetration;
				pos.y += ny * penetration;
			}
		}
	}
}


void Tilemap::Render(SdlPlatform& platform, const Camera2D& camera) const {
	for (int y = 0; y < m_h; ++y) {
		for (int x = 0; x < m_w; ++x) {
			const TileType tile = static_cast<TileType>(At(x, y));
			if (IsFloorLikeTile(tile)) {
				continue;
			}

			const Vec2 worldCenter{
				x * static_cast<float>(m_tileSize) + m_tileSize * 0.5f,
				y * static_cast<float>(m_tileSize) + m_tileSize * 0.5f
			};
			const Vec2 screenPos = camera.WorldToScreen(worldCenter);

			const int drawX = static_cast<int>(screenPos.x - m_tileSize * 0.5f);
			const int drawY = static_cast<int>(screenPos.y - m_tileSize * 0.5f);

			if (tile == TileType::Wall) {
				platform.DrawFilledRect(drawX, drawY, m_tileSize, m_tileSize, 60, 60, 60);
			}
			else if (tile == TileType::RevealDoor) {
				// brown base tile
				platform.DrawFilledRect(drawX, drawY, m_tileSize, m_tileSize, 95, 60, 25);

				// door panel
				platform.DrawFilledRect(drawX + 10, drawY + 6, m_tileSize - 20, m_tileSize - 12, 140, 90, 40);

				// inner panel detail
				platform.DrawFilledRect(drawX + 18, drawY + 14, m_tileSize - 36, m_tileSize - 28, 110, 70, 30);

				// doorknob
				platform.DrawFilledRect(drawX + m_tileSize - 18, drawY + m_tileSize / 2, 6, 6, 220, 190, 90);

			}
			else if (tile == TileType::TrapArmed) {
				platform.DrawFilledRect(drawX, drawY, m_tileSize, m_tileSize, 120, 40, 40); // armed trap
				platform.DrawFilledRect(drawX + 12, drawY + 12, m_tileSize - 24, m_tileSize - 24, 180, 70, 70);
			}
			else if (tile == TileType::TrapSpent) {
				platform.DrawFilledRect(drawX, drawY, m_tileSize, m_tileSize, 65, 35, 35); // spent trap
			}
		}
	}
}


bool Tilemap::IsSolidTile(int tx, int ty) const {
	return At(tx, ty) == TileValue(TileType::Wall);
}

TileCoord Tilemap::WorldToTile(const Vec2& world) const {
	return TileCoord{
		static_cast<int>(std::floor(world.x / static_cast<float>(m_tileSize))),
		static_cast<int>(std::floor(world.y / static_cast<float>(m_tileSize)))
	};
}

Vec2 Tilemap::TileToWorldCenter(int tx, int ty) const {
	return Vec2{
		tx * static_cast<float>(m_tileSize) + m_tileSize * 0.5f,
		ty * static_cast<float>(m_tileSize) + m_tileSize * 0.5f
	};
}

void Tilemap::SetAt(int x, int y, int value) {
	if (x < 0 || y < 0 || x >= m_w || y >= m_h) {
		return;
	}
	m_tiles[static_cast<size_t>(y) * static_cast<size_t>(m_w) + static_cast<size_t>(x)] = value;
}
