#pragma once

#include <Editor/Map/Adt/AdtDocument.h>
#include <Editor/Map/MapClient.h>

#include <cstdint>
#include <string>
#include <vector>

namespace MapEditor::Adt
{
	constexpr int32_t kLiquidCellSide = 8;
	constexpr int32_t kLiquidCells = kLiquidCellSide * kLiquidCellSide;
	constexpr int32_t kLiquidVertSide = 9;
	constexpr int32_t kLiquidVerts = kLiquidVertSide * kLiquidVertSide;

	// SMLiquidInstance::vertexFormat. A 3 turns up in later builds, but every WotLK accessor
	// falls through to zero on it, so the editor never writes one.
	constexpr uint16_t kLiquidHeightDepth = 0;
	constexpr uint16_t kLiquidHeightUv = 1;
	constexpr uint16_t kLiquidDepthOnly = 2;

	// Liquid grids run on the same axes as MCVT, only transposed in the file, so liquid vertex (x,
	// y) sits on MCVT outer vertex Coords::EncodeOuterVertex(y, x). CChunkLiquid::CreateVertXY
	// (0x007CDF80) is where that comes from.
	inline int32_t LiquidCell(int32_t x, int32_t y)
	{
		return y * kLiquidCellSide + x;
	}

	inline int32_t LiquidVert(int32_t x, int32_t y)
	{
		return y * kLiquidVertSide + x;
	}

	// One liquid layer, unpacked from its stored sub-rectangle onto the chunk's whole grid so an
	// edit never has to think about the packing. Whatever cells end up switched on decide the
	// rectangle again on the way out.
	struct LiquidLayer
	{
		uint16_t type = 0;
		uint16_t format = kLiquidHeightDepth;

		bool exists[kLiquidCells] = {};   // per cell, LiquidCell(x, y)
		float height[kLiquidVerts] = {};  // per vertex, absolute world height
		uint8_t depth[kLiquidVerts] = {}; // 0 is a dry shoreline, 255 is fully deep
		uint16_t uv[kLiquidVerts * 2] = {};

		int32_t CellCount() const;

		bool Any() const
		{
			return CellCount() != 0;
		}
	};

	// Every layer on one chunk, plus the two 64 bit cell masks the chunk carries once for all of
	// them, indexed by LiquidCell(x, y). Fatigue is the ocean drowning mask, which Blizzard only
	// sets on cells that are deep everywhere.
	struct ChunkLiquid
	{
		std::vector<LiquidLayer> layers;
		uint64_t fishable = 0;
		uint64_t fatigue = 0;
	};

	// A whole tile's liquid, indexed chunkY * 16 + chunkX exactly as AdtDocument::ChunkAt is.
	struct TileLiquid
	{
		ChunkLiquid chunks[256];
		bool present = false; // whether the file had an MH2O at all
	};

	bool ReadTileLiquid(AdtDocument const& doc, TileLiquid& out, std::string& error);

	// Rebuilds MH2O from scratch and drops it into the document, inserting or removing the chunk as
	// needed and dropping layers with no live cells, which is what erasing liquid comes down to.
	// Also clears MCLQ off any chunk that ends up with MH2O liquid, since CMapChunk::CreateLiquids
	// would otherwise build both.
	bool WriteTileLiquid(TileLiquid const& in, AdtDocument& doc, std::string& error);
}
