#pragma once

#include <ClientData/MathTypes.h>
#include <Editor/Map/MapClient.h>

#include <cmath>
#include <cstdint>

namespace MapEditor::Coords
{
	using namespace ClientData;

	constexpr float kMapHalf = 17066.666f;  // flt_9E2ACC, 32 tiles either side of the origin
	constexpr float kTileSize = 533.33333f;
	constexpr float kChunkSize = kTileSize / 16.0f; // 33.33333, flt_A3E554
	constexpr float kUnitSize = kChunkSize / 8.0f;  // 4.1666665

	// World.x is the stride axis and world.y the fast axis, at every level. CMap::QueryAreaId
	// (0x007A0490) is the clearest proof, it reads m_areaTable[(fromX >> 4) * 64 + (fromY >> 4)]
	// and inside that area mapChunks[(fromX & 15) * 16 + (fromY & 15)], while PrepareArea stores at
	// index.y * 64 + index.x.
	inline float TileCoordX(C3Vector const& world)
	{
		return (kMapHalf - world.y) / kTileSize;
	}

	inline float TileCoordY(C3Vector const& world)
	{
		return (kMapHalf - world.x) / kTileSize;
	}

	inline int32_t TileX(C3Vector const& world)
	{
		return static_cast<int32_t>(std::floor(TileCoordX(world)));
	}

	inline int32_t TileY(C3Vector const& world)
	{
		return static_cast<int32_t>(std::floor(TileCoordY(world)));
	}

	// MDDF and MODF store positions in the ADT's own frame rather than the world one. The loader
	// (CMap::CreateDoodadDef, CMap::CreateMapObjDef) negates z into world.x and x into world.y,
	// passes y straight through as the height, then adds the map origin, which is kMapHalf on both
	// horizontal axes and zero on the vertical.
	inline C3Vector AdtToWorld(C3Vector const& adt)
	{
		return {kMapHalf - adt.z, kMapHalf - adt.x, adt.y};
	}

	inline C3Vector WorldToAdt(C3Vector const& world)
	{
		return {kMapHalf - world.y, world.z, kMapHalf - world.x};
	}

	// Chunk index within its tile, 0..15 on each axis.
	inline int32_t ChunkX(C3Vector const& world)
	{
		float tile = TileCoordX(world);
		return static_cast<int32_t>((tile - std::floor(tile)) * 16.0f);
	}

	inline int32_t ChunkY(C3Vector const& world)
	{
		float tile = TileCoordY(world);
		return static_cast<int32_t>((tile - std::floor(tile)) * 16.0f);
	}

	// Global chunk index across the whole map, what CMapChunk::cOffset holds.
	inline int32_t GlobalChunkX(C3Vector const& world)
	{
		return static_cast<int32_t>(std::floor((kMapHalf - world.y) / kChunkSize));
	}

	inline int32_t GlobalChunkY(C3Vector const& world)
	{
		return static_cast<int32_t>(std::floor((kMapHalf - world.x) / kChunkSize));
	}

	// MCVT is indexed `xUnit * 17 + yUnit`, plus 9 for the inner vertex of a cell.
	// CMapChunk::CreateVerticesWorld (0x007C3F30) holds one world.x value fixed for a whole group
	// while walking consecutive entries down world.y, so the stride-17 block is the x axis and both
	// count down from the chunk's top-left corner.
	struct VertexCoord
	{
		int32_t row = 0;    // x axis, 0..8 outer, 0..7 inner
		int32_t col = 0;    // y axis, 0..8 outer, 0..7 inner
		bool inner = false; // the 8x8 grid offset half a unit into each cell
	};

	// MCVT is 145 floats: 9 rows of 9 outer verts interleaved with 8 rows of 8 inner verts,
	// so every group of 17 is one outer row followed by one inner row.
	inline VertexCoord DecodeVertex(int32_t index)
	{
		VertexCoord out;
		int32_t block = index / 17;
		int32_t rem = index % 17;

		out.inner = rem >= 9;
		out.row = block;
		out.col = out.inner ? rem - 9 : rem;
		return out;
	}

	inline int32_t EncodeOuterVertex(int32_t row, int32_t col)
	{
		return row * 17 + col;
	}

	inline int32_t EncodeInnerVertex(int32_t row, int32_t col)
	{
		return row * 17 + 9 + col;
	}

	// World position of one MCVT vertex. Height is exact: CMapChunk::CreateVerticesWorld writes
	// `vertex.z = chunk->topLeftCoords.z + MCVT[i]`.
	inline C3Vector VertexWorldPos(CMapChunk const* chunk, int32_t index)
	{
		C3Vector out{};
		if (!chunk || !chunk->vertices || index < 0 || index >= Access::kVertsPerChunk)
			return out;

		VertexCoord coord = DecodeVertex(index);
		float colOffset = static_cast<float>(coord.col) + (coord.inner ? 0.5f : 0.0f);
		float rowOffset = static_cast<float>(coord.row) + (coord.inner ? 0.5f : 0.0f);

		out.x = chunk->topLeftCoords.x - rowOffset * kUnitSize;
		out.y = chunk->topLeftCoords.y - colOffset * kUnitSize;
		out.z = chunk->topLeftCoords.z + chunk->vertices[index];
		return out;
	}

	inline float VertexHeight(CMapChunk const* chunk, int32_t index)
	{
		if (!chunk || !chunk->vertices || index < 0 || index >= Access::kVertsPerChunk)
			return 0.0f;

		return chunk->topLeftCoords.z + chunk->vertices[index];
	}

	inline float Distance2D(C3Vector const& a, C3Vector const& b)
	{
		float dx = a.x - b.x;
		float dy = a.y - b.y;
		return std::sqrt(dx * dx + dy * dy);
	}

	// MCAL blend masks are 64x64 over one chunk, on the same axes and direction as the vertex grid,
	// so the stride is the world.x row and the fast index the world.y column. CMapChunk::Intersect
	// (0x007D8730) feeds MCVT `col + row * 17` and tests holes at `(row >> 1) * 4 + (col >> 1)`,
	// and MCAL, MCSH and the hole mask all share that layout.
	constexpr int32_t kAlphaSide = 64;
	constexpr float kTexelSize = kChunkSize / static_cast<float>(kAlphaSide);

	inline int32_t EncodeTexel(int32_t row, int32_t col)
	{
		return row * kAlphaSide + col;
	}

	// Centre of one alpha texel in world space. The centre rather than the corner so a brush
	// covering half a texel does not systematically favour one side.
	inline C3Vector TexelWorldPos(CMapChunk const* chunk, int32_t row, int32_t col)
	{
		C3Vector out{};
		if (!chunk)
			return out;

		out.x = chunk->topLeftCoords.x - (static_cast<float>(row) + 0.5f) * kTexelSize;
		out.y = chunk->topLeftCoords.y - (static_cast<float>(col) + 0.5f) * kTexelSize;
		out.z = chunk->topLeftCoords.z;
		return out;
	}

	// Whether a brush circle reaches any part of a chunk's footprint. The chunk runs from its top
	// left corner back along both axes, since world x and y both count down as the chunk and
	// vertex indices count up.
	inline bool CircleTouchesChunk(CMapChunk const* chunk, C3Vector const& center, float radius)
	{
		if (!chunk)
			return false;

		float maxX = chunk->topLeftCoords.x;
		float maxY = chunk->topLeftCoords.y;
		float minX = maxX - kChunkSize;
		float minY = maxY - kChunkSize;

		float nearestX = center.x < minX ? minX : (center.x > maxX ? maxX : center.x);
		float nearestY = center.y < minY ? minY : (center.y > maxY ? maxY : center.y);
		float dx = center.x - nearestX;
		float dy = center.y - nearestY;

		return dx * dx + dy * dy <= radius * radius;
	}
}
