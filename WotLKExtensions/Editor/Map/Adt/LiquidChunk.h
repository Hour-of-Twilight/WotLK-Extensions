#pragma once

#include <Defines.h>

#include <Editor/Map/Adt/AdtDocument.h>
#include <Editor/Map/MapClient.h>

#include <cstdint>
#include <string>
#include <vector>

namespace MapEditor::Adt
{
	constexpr int32 kLiquidCellSide = 8;
	constexpr int32 kLiquidCells = kLiquidCellSide * kLiquidCellSide;
	constexpr int32 kLiquidVertSide = 9;
	constexpr int32 kLiquidVerts = kLiquidVertSide * kLiquidVertSide;

	// SMLiquidInstance::vertexFormat. A 3 turns up in later builds, but every WotLK accessor
	// falls through to zero on it, so the editor never writes one.
	constexpr uint16 kLiquidHeightDepth = 0;
	constexpr uint16 kLiquidHeightUv = 1;
	constexpr uint16 kLiquidDepthOnly = 2;

	// Liquid grids run on the same axes as MCVT, only transposed in the file, so liquid vertex (x,
	// y) sits on MCVT outer vertex Coords::EncodeOuterVertex(y, x). CChunkLiquid::CreateVertXY
	// (0x007CDF80) is where that comes from.
	inline int32 LiquidCell(int32 x, int32 y)
	{
		return y * kLiquidCellSide + x;
	}

	inline int32 LiquidVert(int32 x, int32 y)
	{
		return y * kLiquidVertSide + x;
	}

	// One liquid layer, unpacked from its stored sub-rectangle onto the chunk's whole grid so an
	// edit never has to think about the packing. Whatever cells end up switched on decide the
	// rectangle again on the way out.
	struct LiquidLayer
	{
		uint16 type = 0;
		uint16 format = kLiquidHeightDepth;

		bool exists[kLiquidCells] = {};  // per cell, LiquidCell(x, y)
		float height[kLiquidVerts] = {}; // per vertex, absolute world height
		uint8 depth[kLiquidVerts] = {};  // 0 is a dry shoreline, 255 is fully deep
		uint16 uv[kLiquidVerts * 2] = {};

		int32 CellCount() const;

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
		uint64 fishable = 0;
		uint64 fatigue = 0;
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
