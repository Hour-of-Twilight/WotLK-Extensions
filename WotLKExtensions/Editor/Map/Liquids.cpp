#include <Editor/Map/Liquids.h>

#include <ClientData/ClientAddresses.h>
#include <ClientData/SharedDefines.h>
#include <Editor/Map/Adt/LiquidChunk.h>
#include <Editor/Map/MapClient.h>
#include <Editor/Map/MapCoords.h>
#include <Editor/Map/TileSession.h>

#include <cstring>

namespace MapEditor::Liquids
{
	namespace
	{
		constexpr int32_t kSide = Access::kChunksPerTileSide;

		LiquidTypeRow const* Row(uint32_t id)
		{
			return static_cast<LiquidTypeRow const*>(ClientDB::GetRowById(g_LiquidTypeDB, id));
		}

		// Shore fade, straight off Noggit's river and ocean opacity values. Depth is what makes a
		// waterline fade out instead of ending in a hard edge, and an ocean is meant to take far
		// longer to go opaque than a river does.
		float FadeFactor(uint32_t soundBank)
		{
			return soundBank == kBankOcean ? 0.007f : 0.0337f;
		}

		uint16_t FormatFor(uint32_t soundBank)
		{
			if (soundBank == kBankMagma || soundBank == kBankSlime)
				return Adt::kLiquidHeightUv;

			return Adt::kLiquidHeightDepth;
		}

		struct Terrain
		{
			bool valid = false;
			float height[Adt::kMcvtHeights] = {};
		};

		// Absolute terrain heights for one chunk, taken from the document rather than the live
		// buffer so an edit works the same on a tile that is only open for writing.
		Terrain ReadTerrain(Adt::Mcnk const& mcnk)
		{
			Terrain out;

			Adt::SubChunk const* mcvt = mcnk.Find(Adt::kMCVT);
			if (!mcvt || mcvt->data.size() < sizeof(out.height))
				return out;

			std::memcpy(out.height, mcvt->data.data(), sizeof(out.height));
			for (float& z : out.height)
				z += mcnk.header.position.z;

			out.valid = true;
			return out;
		}

		// Liquid vertex (x, y) sits on MCVT outer vertex (row y, col x), because the liquid grid is
		// the transpose of the terrain one. Noggit pairs the same two indices, water z * 9 + x
		// against terrain z * 17 + x.
		void RefreshDepth(Adt::LiquidLayer& layer, Terrain const& terrain, uint32_t soundBank)
		{
			float factor = FadeFactor(soundBank);

			for (int32_t y = 0; y < Adt::kLiquidVertSide; ++y)
			{
				for (int32_t x = 0; x < Adt::kLiquidVertSide; ++x)
				{
					int32_t v = Adt::LiquidVert(x, y);
					if (!terrain.valid)
					{
						layer.depth[v] = 255;
						continue;
					}

					float diff = layer.height[v] - terrain.height[Coords::EncodeOuterVertex(y, x)];
					float depth = diff < 0.0f ? 0.0f : (diff + 1.0f) * factor;
					depth = depth > 1.0f ? 1.0f : depth;
					layer.depth[v] = static_cast<uint8_t>(depth * 255.0f);
				}
			}
		}

		// Magma and slime scroll a texture over the surface instead of fading it, and the uv pair
		// is where that starts from. A quarter of a texture repeat per vertex, stored as Noggit
		// stores it, which is the normalised value times 255.
		void RefreshUv(Adt::LiquidLayer& layer)
		{
			for (int32_t y = 0; y < Adt::kLiquidVertSide; ++y)
			{
				for (int32_t x = 0; x < Adt::kLiquidVertSide; ++x)
				{
					int32_t v = Adt::LiquidVert(x, y) * 2;
					layer.uv[v] = static_cast<uint16_t>(x * 255 / 4);
					layer.uv[v + 1] = static_cast<uint16_t>(y * 255 / 4);
				}
			}
		}

		void Finish(Adt::LiquidLayer& layer, Adt::Mcnk const& mcnk, uint32_t soundBank)
		{
			if (layer.format == Adt::kLiquidHeightUv)
				RefreshUv(layer);
			else
				RefreshDepth(layer, ReadTerrain(mcnk), soundBank);
		}

		uint32_t BankOf(uint32_t type)
		{
			LiquidTypeRow const* row = Row(type);
			return row ? row->soundBank : kBankWater;
		}

		// The chunk's cells the brush circle covers, as a bit per LiquidCell. Cell x runs along
		// world y and cell y along world x, the same transpose the vertex grid uses.
		uint64_t CoveredCells(CMapChunk const* chunk, C3Vector const& center, float radius)
		{
			uint64_t mask = 0;
			if (!chunk)
				return mask;

			for (int32_t cy = 0; cy < Adt::kLiquidCellSide; ++cy)
			{
				for (int32_t cx = 0; cx < Adt::kLiquidCellSide; ++cx)
				{
					float worldX = chunk->topLeftCoords.x
					    - (static_cast<float>(cy) + 0.5f) * Coords::kUnitSize;
					float worldY = chunk->topLeftCoords.y
					    - (static_cast<float>(cx) + 0.5f) * Coords::kUnitSize;

					float dx = center.x - worldX;
					float dy = center.y - worldY;
					if (dx * dx + dy * dy <= radius * radius)
						mask |= 1ull << Adt::LiquidCell(cx, cy);
				}
			}

			return mask;
		}

		bool CircleTouchesTile(CMapArea* area, C3Vector const& center, float radius)
		{
			for (int32_t i = 0; i < Access::kChunksPerTile; ++i)
			{
				if (Coords::CircleTouchesChunk(area->mapChunks[i], center, radius))
					return true;
			}

			return false;
		}

		// Every cell any layer still has liquid on, so the attribute masks can be trimmed back
		// when the last layer over a cell goes.
		uint64_t OccupiedCells(Adt::ChunkLiquid const& liquid)
		{
			uint64_t mask = 0;
			for (Adt::LiquidLayer const& layer : liquid.layers)
			{
				for (int32_t cell = 0; cell < Adt::kLiquidCells; ++cell)
				{
					if (layer.exists[cell])
						mask |= 1ull << cell;
				}
			}

			return mask;
		}

		// Walks the chunks the brush reaches, one tile at a time. MH2O is a single block for the
		// whole tile, so it is read once, edited across every chunk, and rebuilt once.
		template <typename Visit>
		int32_t EditTiles(C3Vector const& center, float radius, std::vector<TileRef>& touched,
		    std::string& error, Visit&& visit)
		{
			int32_t changed = 0;

			// Tile coordinates run backwards along both world axes, so the corner that gives the
			// low index is the one at plus radius.
			C3Vector lo{center.x + radius, center.y + radius, 0.0f};
			C3Vector hi{center.x - radius, center.y - radius, 0.0f};

			for (int32_t tileY = Coords::TileY(lo); tileY <= Coords::TileY(hi); ++tileY)
			{
				for (int32_t tileX = Coords::TileX(lo); tileX <= Coords::TileX(hi); ++tileX)
				{
					CMapArea* area = Access::GetArea(tileX, tileY);
					if (!Access::IsAreaReady(area) || !CircleTouchesTile(area, center, radius))
						continue;

					Session::OpenTile* tile = Session::Open(tileX, tileY, error);
					if (!tile)
						continue;

					Adt::TileLiquid liquid;
					if (!Adt::ReadTileLiquid(tile->doc, liquid, error))
						continue;

					int32_t here = 0;

					for (int32_t chunkY = 0; chunkY < kSide; ++chunkY)
					{
						for (int32_t chunkX = 0; chunkX < kSide; ++chunkX)
						{
							CMapChunk* chunk = Access::GetChunk(area, chunkX, chunkY);
							if (!Coords::CircleTouchesChunk(chunk, center, radius))
								continue;

							Adt::Mcnk* mcnk = tile->doc.ChunkAt(chunkX, chunkY);
							if (!mcnk)
								continue;

							uint64_t mask = CoveredCells(chunk, center, radius);
							if (!mask)
								continue;

							if (visit(liquid.chunks[chunkY * kSide + chunkX], *mcnk, mask))
								++here;
						}
					}

					if (!here)
						continue;

					if (!Adt::WriteTileLiquid(liquid, tile->doc, error))
						continue;

					tile->dirty = true;
					touched.push_back({tileX, tileY});
					changed += here;
				}
			}

			return changed;
		}

		// Which layers an operation applies to. -1 is all of them, anything else is one index.
		bool Targeted(int32_t layer, size_t index)
		{
			return layer < 0 || static_cast<size_t>(layer) == index;
		}
	}

	bool GetType(uint32_t id, TypeInfo& out)
	{
		LiquidTypeRow const* row = Row(id);
		if (!row)
			return false;

		out.id = row->id;
		out.name = row->name ? row->name : "";
		out.soundBank = row->soundBank;
		out.materialId = row->materialId;
		return true;
	}

	void ListTypes(std::vector<TypeInfo>& out)
	{
		out.clear();

		if (!g_LiquidTypeDB || !g_LiquidTypeDB->isLoaded)
			return;

		int32_t first = g_LiquidTypeDB->minIndex;
		int32_t last = g_LiquidTypeDB->maxIndex;
		if (first < 0 || last < first || last - first > 100000)
			return;

		for (int32_t id = first; id <= last; ++id)
		{
			TypeInfo info;
			if (GetType(static_cast<uint32_t>(id), info))
				out.push_back(std::move(info));
		}
	}

	int32_t Add(C3Vector const& center, float radius, uint32_t type, float height,
	    std::vector<TileRef>& touched, std::string& error)
	{
		TypeInfo info;
		if (!GetType(type, info))
		{
			error = "no LiquidType.dbc row with that id";
			return 0;
		}

		if (radius <= 0.0f)
		{
			error = "the brush has no radius";
			return 0;
		}

		int32_t full = 0;

		int32_t changed = EditTiles(center, radius, touched, error,
		    [&](Adt::ChunkLiquid& liquid, Adt::Mcnk const& mcnk, uint64_t mask)
		    {
			    Adt::LiquidLayer* layer = nullptr;
			    for (Adt::LiquidLayer& candidate : liquid.layers)
			    {
				    if (candidate.type == type)
				    {
					    layer = &candidate;
					    break;
				    }
			    }

			    if (!layer)
			    {
				    if (static_cast<int32_t>(liquid.layers.size()) >= kMaxLayers)
				    {
					    ++full;
					    return false;
				    }

				    Adt::LiquidLayer fresh;
				    fresh.type = static_cast<uint16_t>(type);
				    fresh.format = FormatFor(info.soundBank);

				    // Seeded flat across the whole grid so growing this layer later never lands a
				    // cell next to a vertex that was never given a height.
				    for (int32_t v = 0; v < Adt::kLiquidVerts; ++v)
					    fresh.height[v] = height;

				    liquid.layers.push_back(fresh);
				    layer = &liquid.layers.back();
			    }

			    for (int32_t cy = 0; cy < Adt::kLiquidCellSide; ++cy)
			    {
				    for (int32_t cx = 0; cx < Adt::kLiquidCellSide; ++cx)
				    {
					    int32_t cell = Adt::LiquidCell(cx, cy);
					    if (!(mask & (1ull << cell)))
						    continue;

					    layer->exists[cell] = true;
					    liquid.fishable |= 1ull << cell;

					    for (int32_t dy = 0; dy <= 1; ++dy)
					    {
						    for (int32_t dx = 0; dx <= 1; ++dx)
							    layer->height[Adt::LiquidVert(cx + dx, cy + dy)] = height;
					    }
				    }
			    }

			    Finish(*layer, mcnk, info.soundBank);
			    return true;
		    });

		if (!changed && error.empty())
		{
			error = full ? "every chunk under the brush is already at four liquid layers"
			             : "nothing under the brush to put liquid on";
		}

		return changed;
	}

	int32_t Remove(C3Vector const& center, float radius, int32_t layer,
	    std::vector<TileRef>& touched, std::string& error)
	{
		int32_t changed = EditTiles(center, radius, touched, error,
		    [&](Adt::ChunkLiquid& liquid, Adt::Mcnk const&, uint64_t mask)
		    {
			    bool any = false;

			    for (size_t i = 0; i < liquid.layers.size(); ++i)
			    {
				    if (!Targeted(layer, i))
					    continue;

				    for (int32_t cell = 0; cell < Adt::kLiquidCells; ++cell)
				    {
					    if (!(mask & (1ull << cell)) || !liquid.layers[i].exists[cell])
						    continue;

					    liquid.layers[i].exists[cell] = false;
					    any = true;
				    }
			    }

			    if (!any)
				    return false;

			    // A cell nothing covers any more has no business staying fishable or fatiguing.
			    uint64_t left = OccupiedCells(liquid);
			    liquid.fishable &= left;
			    liquid.fatigue &= left;
			    return true;
		    });

		if (!changed && error.empty())
			error = "no liquid under the brush";

		return changed;
	}

	int32_t SetType(C3Vector const& center, float radius, int32_t layer, uint32_t type,
	    std::vector<TileRef>& touched, std::string& error)
	{
		TypeInfo info;
		if (!GetType(type, info))
		{
			error = "no LiquidType.dbc row with that id";
			return 0;
		}

		int32_t changed = EditTiles(center, radius, touched, error,
		    [&](Adt::ChunkLiquid& liquid, Adt::Mcnk const& mcnk, uint64_t mask)
		    {
			    bool any = false;

			    for (size_t i = 0; i < liquid.layers.size(); ++i)
			    {
				    if (!Targeted(layer, i) || liquid.layers[i].type == type)
					    continue;

				    // Only layers the brush is actually over, so a wide circle does not quietly
				    // retype a pond on the far side of the chunk.
				    bool touching = false;
				    for (int32_t cell = 0; cell < Adt::kLiquidCells && !touching; ++cell)
					    touching = (mask & (1ull << cell)) && liquid.layers[i].exists[cell];

				    if (!touching)
					    continue;

				    liquid.layers[i].type = static_cast<uint16_t>(type);
				    liquid.layers[i].format = FormatFor(info.soundBank);
				    Finish(liquid.layers[i], mcnk, info.soundBank);
				    any = true;
			    }

			    return any;
		    });

		if (!changed && error.empty())
			error = "no liquid under the brush was a different type";

		return changed;
	}

	int32_t SetHeight(C3Vector const& center, float radius, int32_t layer, float height,
	    bool relative, std::vector<TileRef>& touched, std::string& error)
	{
		int32_t changed = EditTiles(center, radius, touched, error,
		    [&](Adt::ChunkLiquid& liquid, Adt::Mcnk const& mcnk, uint64_t mask)
		    {
			    bool any = false;

			    for (size_t i = 0; i < liquid.layers.size(); ++i)
			    {
				    if (!Targeted(layer, i))
					    continue;

				    Adt::LiquidLayer& target = liquid.layers[i];

				    // Vertices belonging to a covered cell that this layer actually has, so the
				    // edge of a pool moves with its middle.
				    bool moved[Adt::kLiquidVerts] = {};
				    for (int32_t cy = 0; cy < Adt::kLiquidCellSide; ++cy)
				    {
					    for (int32_t cx = 0; cx < Adt::kLiquidCellSide; ++cx)
					    {
						    int32_t cell = Adt::LiquidCell(cx, cy);
						    if (!(mask & (1ull << cell)) || !target.exists[cell])
							    continue;

						    for (int32_t dy = 0; dy <= 1; ++dy)
						    {
							    for (int32_t dx = 0; dx <= 1; ++dx)
								    moved[Adt::LiquidVert(cx + dx, cy + dy)] = true;
						    }
					    }
				    }

				    bool here = false;
				    for (int32_t v = 0; v < Adt::kLiquidVerts; ++v)
				    {
					    if (!moved[v])
						    continue;

					    target.height[v] = relative ? target.height[v] + height : height;
					    here = true;
				    }

				    if (!here)
					    continue;

				    Finish(target, mcnk, BankOf(target.type));
				    any = true;
			    }

			    return any;
		    });

		if (!changed && error.empty())
			error = "no liquid under the brush";

		return changed;
	}

	int32_t SetFlags(C3Vector const& center, float radius, int32_t fishable, int32_t fatigue,
	    std::vector<TileRef>& touched, std::string& error)
	{
		if (fishable < 0 && fatigue < 0)
		{
			error = "nothing to change";
			return 0;
		}

		int32_t changed = EditTiles(center, radius, touched, error,
		    [&](Adt::ChunkLiquid& liquid, Adt::Mcnk const&, uint64_t mask)
		    {
			    // Only where there is liquid. The masks are per cell for the whole chunk, so
			    // setting a bit on a dry cell means nothing and just makes the file harder to read.
			    uint64_t live = mask & OccupiedCells(liquid);
			    if (!live)
				    return false;

			    uint64_t wasFishable = liquid.fishable;
			    uint64_t wasFatigue = liquid.fatigue;

			    if (fishable == 0)
				    liquid.fishable &= ~live;
			    else if (fishable > 0)
				    liquid.fishable |= live;

			    if (fatigue == 0)
				    liquid.fatigue &= ~live;
			    else if (fatigue > 0)
				    liquid.fatigue |= live;

			    return liquid.fishable != wasFishable || liquid.fatigue != wasFatigue;
		    });

		if (!changed && error.empty())
			error = "no liquid under the brush, or it was already set that way";

		return changed;
	}

	bool Describe(int32_t tileX, int32_t tileY, int32_t chunkX, int32_t chunkY, ChunkInfo& out,
	    std::string& error)
	{
		out = ChunkInfo();

		if (chunkX < 0 || chunkX >= kSide || chunkY < 0 || chunkY >= kSide)
		{
			error = "chunk is off the tile";
			return false;
		}

		Session::OpenTile* tile = Session::Open(tileX, tileY, error);
		if (!tile)
			return false;

		Adt::TileLiquid liquid;
		if (!Adt::ReadTileLiquid(tile->doc, liquid, error))
			return false;

		Adt::ChunkLiquid const& chunk = liquid.chunks[chunkY * kSide + chunkX];
		out.fishable = chunk.fishable;
		out.fatigue = chunk.fatigue;

		for (Adt::LiquidLayer const& layer : chunk.layers)
		{
			LayerInfo info;
			info.type = layer.type;
			info.format = layer.format;
			info.cells = layer.CellCount();

			bool first = true;
			for (int32_t cy = 0; cy < Adt::kLiquidCellSide; ++cy)
			{
				for (int32_t cx = 0; cx < Adt::kLiquidCellSide; ++cx)
				{
					if (!layer.exists[Adt::LiquidCell(cx, cy)])
						continue;

					for (int32_t dy = 0; dy <= 1; ++dy)
					{
						for (int32_t dx = 0; dx <= 1; ++dx)
						{
							float z = layer.height[Adt::LiquidVert(cx + dx, cy + dy)];
							if (first)
							{
								info.minHeight = z;
								info.maxHeight = z;
								first = false;
							}

							info.minHeight = z < info.minHeight ? z : info.minHeight;
							info.maxHeight = z > info.maxHeight ? z : info.maxHeight;
						}
					}
				}
			}

			out.layers.push_back(std::move(info));
		}

		return true;
	}
}
