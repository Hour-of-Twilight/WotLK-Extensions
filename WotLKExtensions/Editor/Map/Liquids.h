#pragma once

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
	constexpr uint32_t kBankWater = 0;
	constexpr uint32_t kBankOcean = 1;
	constexpr uint32_t kBankMagma = 2;
	constexpr uint32_t kBankSlime = 3;

	// More than this on one chunk is possible in the format and unheard of in practice.
	constexpr int32_t kMaxLayers = 4;

	struct TileRef
	{
		int32_t x = 0;
		int32_t y = 0;
	};

	inline int32_t CountBits(uint64_t mask)
	{
		int32_t count = 0;
		for (; mask; mask &= mask - 1)
			++count;

		return count;
	}

	// One LiquidType.dbc row, read out of the client's own copy at 0x00AD4064. There is no
	// liquidtype table on the server, so this is the only place the names live.
	struct TypeInfo
	{
		uint32_t id = 0;
		std::string name;
		uint32_t soundBank = 0;
		uint32_t materialId = 0;
	};

	bool GetType(uint32_t id, TypeInfo& out);
	void ListTypes(std::vector<TypeInfo>& out);

	struct LayerInfo
	{
		uint32_t type = 0;
		uint32_t format = 0;
		int32_t cells = 0;
		float minHeight = 0.0f;
		float maxHeight = 0.0f;
	};

	struct ChunkInfo
	{
		std::vector<LayerInfo> layers;
		uint64_t fishable = 0;
		uint64_t fatigue = 0;
	};

	// Switches liquid on for every cell the brush covers, with `height` the surface in absolute
	// world z. A layer already carrying this liquid type grows, otherwise the chunk gains one, and
	// the count of changed chunks comes back.
	int32_t Add(C3Vector const& center, float radius, uint32_t type, float height,
	    std::vector<TileRef>& touched, std::string& error);

	// Switches it off again, with `layer` of -1 clearing every layer. A layer with nothing left in
	// it is dropped on the way out, and a chunk with no layers left loses its entry entirely.
	int32_t Remove(C3Vector const& center, float radius, int32_t layer,
	    std::vector<TileRef>& touched, std::string& error);

	// Repoints a layer at a different LiquidType.dbc row. The vertex format follows the new type,
	// since magma and slime store scrolling uv where water stores a depth byte.
	int32_t SetType(C3Vector const& center, float radius, int32_t layer, uint32_t type,
	    std::vector<TileRef>& touched, std::string& error);

	// Moves the surface, where `relative` adds to whatever is there and otherwise it is an absolute
	// z. Only vertices belonging to cells the brush covers move.
	int32_t SetHeight(C3Vector const& center, float radius, int32_t layer, float height,
	    bool relative, std::vector<TileRef>& touched, std::string& error);

	// The two per-cell attribute masks. Each argument is -1 to leave alone, 0 to clear, 1 to set.
	int32_t SetFlags(C3Vector const& center, float radius, int32_t fishable, int32_t fatigue,
	    std::vector<TileRef>& touched, std::string& error);

	bool Describe(int32_t tileX, int32_t tileY, int32_t chunkX, int32_t chunkY, ChunkInfo& out,
	    std::string& error);
}
