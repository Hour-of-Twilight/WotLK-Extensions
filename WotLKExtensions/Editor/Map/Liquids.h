#pragma once

#include <Defines.h>

#include <ClientData/MathTypes.h>

#include <cstdint>
#include <string>
#include <vector>

// Liquid editing: adding water to a chunk, taking it away, changing what it is made of and moving
// its surface around. Nothing here shows up until the tile has been written back out and purged,
// because the client parses MH2O once on load into an accessor that points into the file image.
//
// So these are one-shot commands rather than a drag brush, each one costing a save and a reload of
// every tile it touched. The caller gets the list back in `touched` and publishes them.
namespace MapEditor::Liquids
{
	using namespace ClientData;

	// LiquidType.dbc SoundBank, which is what decides the vertex format and the shore fade.
	constexpr uint32 kBankWater = 0;
	constexpr uint32 kBankOcean = 1;
	constexpr uint32 kBankMagma = 2;
	constexpr uint32 kBankSlime = 3;

	// More than this on one chunk is possible in the format and unheard of in practice.
	constexpr int32 kMaxLayers = 4;

	struct TileRef
	{
		int32 x = 0;
		int32 y = 0;
	};

	inline int32 CountBits(uint64 mask)
	{
		int32 count = 0;
		for (; mask; mask &= mask - 1)
			++count;

		return count;
	}

	// One LiquidType.dbc row, read out of the client's own copy at 0x00AD4064. There is no
	// liquidtype table on the server, so this is the only place the names live.
	struct TypeInfo
	{
		uint32 id = 0;
		std::string name;
		uint32 soundBank = 0;
		uint32 materialId = 0;
	};

	bool GetType(uint32 id, TypeInfo& out);
	void ListTypes(std::vector<TypeInfo>& out);

	struct LayerInfo
	{
		uint32 type = 0;
		uint32 format = 0;
		int32 cells = 0;
		float minHeight = 0.0f;
		float maxHeight = 0.0f;
	};

	struct ChunkInfo
	{
		std::vector<LayerInfo> layers;
		uint64 fishable = 0;
		uint64 fatigue = 0;
	};

	// Switches liquid on for every cell the brush covers, with `height` the surface in absolute
	// world z. A layer already carrying this liquid type grows, otherwise the chunk gains one, and
	// the count of changed chunks comes back.
	int32 Add(C3Vector const& center, float radius, uint32 type, float height,
	    std::vector<TileRef>& touched, std::string& error);

	// Switches it off again, with `layer` of -1 clearing every layer. A layer with nothing left in
	// it is dropped on the way out, and a chunk with no layers left loses its entry entirely.
	int32 Remove(C3Vector const& center, float radius, int32 layer,
	    std::vector<TileRef>& touched, std::string& error);

	// Repoints a layer at a different LiquidType.dbc row. The vertex format follows the new type,
	// since magma and slime store scrolling uv where water stores a depth byte.
	int32 SetType(C3Vector const& center, float radius, int32 layer, uint32 type,
	    std::vector<TileRef>& touched, std::string& error);

	// Moves the surface, where `relative` adds to whatever is there and otherwise it is an absolute
	// z. Only vertices belonging to cells the brush covers move.
	int32 SetHeight(C3Vector const& center, float radius, int32 layer, float height,
	    bool relative, std::vector<TileRef>& touched, std::string& error);

	// The two per-cell attribute masks. Each argument is -1 to leave alone, 0 to clear, 1 to set.
	int32 SetFlags(C3Vector const& center, float radius, int32 fishable, int32 fatigue,
	    std::vector<TileRef>& touched, std::string& error);

	bool Describe(int32 tileX, int32 tileY, int32 chunkX, int32 chunkY, ChunkInfo& out,
	    std::string& error);
}
