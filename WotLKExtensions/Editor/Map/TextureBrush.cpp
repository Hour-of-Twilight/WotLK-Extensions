#include <Editor/Map/TextureBrush.h>

#include <Editor/Map/Adt/AlphaMap.h>
#include <Editor/Map/MapClient.h>
#include <Editor/Map/MapCoords.h>
#include <Editor/Map/TileSession.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace MapEditor::TextureBrush
{
	namespace
	{
		constexpr int32_t kSide = Access::kChunksPerTileSide;
		constexpr int32_t kTexelSide = Coords::kAlphaSide;
		constexpr int32_t kTexels = kTexelSide * kTexelSide;
		constexpr int32_t kMaxLayers = 4;

		float Weight(float distance, float radius, Sculpt::Falloff falloff)
		{
			if (radius <= 0.0f || distance >= radius)
				return 0.0f;

			float t = 1.0f - distance / radius;
			switch (falloff)
			{
				case Sculpt::Falloff::Flat: return 1.0f;
				case Sculpt::Falloff::Linear: return t;
				case Sculpt::Falloff::Smooth:
				default: return t * t * (3.0f - 2.0f * t);
			}
		}

		struct ChunkKey
		{
			int32_t tileX = 0;
			int32_t tileY = 0;
			int32_t chunkX = 0;
			int32_t chunkY = 0;

			bool operator<(ChunkKey const& other) const
			{
				if (tileX != other.tileX)
					return tileX < other.tileX;
				if (tileY != other.tileY)
					return tileY < other.tileY;
				if (chunkX != other.chunkX)
					return chunkX < other.chunkX;
				return chunkY < other.chunkY;
			}
		};

		// What the editor keeps for every chunk it has painted. `coverage` is the accumulator, one
		// 64x64 plane per layer including the base, in 8.8 fixed point because a single frame of a
		// stroke moves a texel by well under one 8 bit unit and round-tripping through the file
		// would quantize every step away.
		//
		// `mcal` is the blob the client is pointed at. Handing it our own buffer instead of writing
		// back over its copy is what makes a compressed chunk paintable, since re-encoding drops
		// the RLE and the bytes no longer fit the hole they came out of.
		struct PaintedChunk
		{
			Adt::ChunkAlpha alpha;
			std::vector<uint16_t> coverage;
			std::vector<uint8_t> mcly;
			std::vector<uint8_t> mcal;
		};

		constexpr float kFixedOne = 256.0f;
		constexpr float kFullCoverage = 255.0f;

		uint16_t ToFixed(float value)
		{
			float scaled = value * kFixedOne + 0.5f;
			if (scaled <= 0.0f)
				return 0;
			if (scaled >= kFullCoverage * kFixedOne)
				return static_cast<uint16_t>(kFullCoverage * kFixedOne);

			return static_cast<uint16_t>(scaled);
		}

		std::map<ChunkKey, PaintedChunk>& Painted()
		{
			static std::map<ChunkKey, PaintedChunk> painted;
			return painted;
		}

		// True when the chunk is about to list different textures than it does now. This is the
		// same comparison the client's pairing test makes, so it also answers whether a shared
		// render chunk is still legal.
		bool LayerSetChanged(CMapChunk* chunk, Adt::Mcnk const& mcnk, SMLayer const* fresh)
		{
			if (chunk->header->nLayers != mcnk.header.nLayers)
				return true;
			if (!chunk->layers)
				return mcnk.header.nLayers != 0;

			for (uint32_t i = 0; i < mcnk.header.nLayers; ++i)
			{
				if (chunk->layers[i].textureId != fresh[i].textureId
				    || chunk->layers[i].flags != fresh[i].flags)
					return true;
			}
			return false;
		}

		// Pushes an edited chunk's layers and blend masks into the client's live copy and has the
		// layer textures rebuilt from them. MCLY is repointed rather than written back in place for
		// the same reason MCAL is, adding a layer makes it longer than the hole it came out of, and
		// both are only offsets into the tile's file image so nothing frees them separately.
		void PushLive(PaintedChunk& state, CMapChunk* chunk, Adt::Mcnk const& mcnk)
		{
			if (!chunk || !chunk->header)
				return;

			Adt::SubChunk const* mcly = mcnk.Find(Adt::kMCLY);
			Adt::SubChunk const* mcal = mcnk.Find(Adt::kMCAL);
			if (!mcly || !mcal)
				return;

			size_t layerBytes = mcnk.header.nLayers * sizeof(SMLayer);
			if (!layerBytes || mcly->data.size() < layerBytes)
				return;

			bool relayered
			    = LayerSetChanged(chunk, mcnk, reinterpret_cast<SMLayer const*>(mcly->data.data()));

			state.mcly.assign(mcly->data.begin(), mcly->data.begin() + layerBytes);
			chunk->layers = reinterpret_cast<SMLayer*>(state.mcly.data());

			state.mcal = mcal->data;
			chunk->alpha = state.mcal.empty() ? nullptr : state.mcal.data();

			// CreateLayers reads the count out of the header rather than off the chunk, so a
			// layer that was added or removed only exists once this agrees with MCLY.
			chunk->header->nLayers = mcnk.header.nLayers;

			// A render chunk shared with the neighbour only has four layer slots for the pair
			// between them, and CreateLayers unions both chunks into it without checking. Once
			// the sets diverge the fifth write lands on the gx buffer pointer and the next
			// RenderPrep faults, so give each chunk its own before anything rebuilds.
			if (relayered)
				Access::SplitRenderChunk(chunk);

			// Layer textures are built once and then kept, so nothing would reread MCAL without
			// this. Dropping them puts the layer count back to zero, which is what RenderPrep
			// tests before rebuilding, and the unpack lands on the bytes just written.
			if (chunk->renderChunk)
				Access::CMapRenderChunk_FreeLayers(chunk->renderChunk);
		}

		// Redistributes one texel so the painted layer moves toward its target and the rest give
		// up, or take back, exactly what it gained. The terrain shader adds layers rather than
		// stacking them, so the masks plus what the base shows through always come to full, and
		// normalizing at the end is what keeps a texel from drifting over a long stroke.
		bool PaintTexel(float* coverage, int32_t count, int32_t layer, float target, float t)
		{
			float total = 0.0f;
			for (int32_t i = 0; i < count; ++i)
				total += coverage[i];

			float change = (target - coverage[layer]) * t;
			if (change == 0.0f)
				return false;

			float others = total - coverage[layer];

			if (others > 0.001f)
			{
				for (int32_t i = 0; i < count; ++i)
				{
					if (i != layer)
						coverage[i] -= change * coverage[i] / others;
				}
			}
			else if (change > 0.0f)
			{
				// Nothing else claims any of this texel, so the painted layer already owns it.
				for (int32_t i = 0; i < count; ++i)
				{
					if (i != layer)
						coverage[i] = 0.0f;
				}
			}
			else
			{
				// Taking coverage off the only layer present has to give it to something. The
				// base layer is the natural home, and when the base is what is being erased the
				// first other layer takes it.
				int32_t sink = layer == 0 ? 1 : 0;
				if (sink >= count)
					return false;

				coverage[sink] -= change;
			}

			coverage[layer] += change;

			for (int32_t i = 0; i < count; ++i)
				coverage[i] = coverage[i] < 0.0f ? 0.0f : (coverage[i] > 255.0f ? 255.0f : coverage[i]);

			float after = 0.0f;
			for (int32_t i = 0; i < count; ++i)
				after += coverage[i];

			if (after <= 0.001f)
			{
				for (int32_t i = 0; i < count; ++i)
					coverage[i] = 0.0f;

				coverage[0] = 255.0f;
			}
			else if (std::fabs(after - 255.0f) > 0.001f)
			{
				for (int32_t i = 0; i < count; ++i)
					coverage[i] *= 255.0f / after;
			}

			return true;
		}

		// One chunk's worth of painting. Returns how many texels changed.
		int32_t PaintChunk(Stroke const& stroke, ChunkKey const& key, CMapChunk* chunk,
		    Session::OpenTile& tile)
		{
			Adt::Mcnk* mcnk = tile.doc.ChunkAt(key.chunkX, key.chunkY);
			if (!mcnk)
				return 0;

			// The map being painted is always the one being stood in, so the client's own copy of
			// the WDT flags is the right source for the alpha format.
			Adt::AlphaFormat format = Adt::FormatFor(*mcnk, *Access::sMapFlags);

			PaintedChunk& state = Painted()[key];
			Adt::ChunkAlpha& alpha = state.alpha;

			// Decoded once and then kept. A layer count that no longer agrees means either that
			// this chunk has not been touched yet or that the tile was reloaded under us, and both
			// want the same thing: start from what is in the document now.
			if (alpha.layers.size() != mcnk->header.nLayers)
			{
				std::string error;
				if (!Adt::ReadChunkAlpha(*mcnk, format, alpha, error))
				{
					Painted().erase(key);
					return 0;
				}

				state.coverage.assign(alpha.layers.size() * kTexels, 0);
				for (size_t i = 0; i < alpha.layers.size(); ++i)
				{
					for (int32_t t = 0; t < kTexels; ++t)
						state.coverage[i * kTexels + t] = ToFixed(alpha.maps[i].texel[t]);
				}
			}

			int32_t count = static_cast<int32_t>(alpha.layers.size());
			if (count < 2 || stroke.layer < 0 || stroke.layer >= count || count > kMaxLayers)
				return 0;

			float target = stroke.blend >= 0.0f ? kFullCoverage : 0.0f;
			float rate = std::fabs(stroke.blend);
			int32_t changed = 0;

			for (int32_t row = 0; row < kTexelSide; ++row)
			{
				for (int32_t col = 0; col < kTexelSide; ++col)
				{
					C3Vector world = Coords::TexelWorldPos(chunk, row, col);
					float weight = Weight(Coords::Distance2D(world, stroke.center), stroke.radius,
					    stroke.falloff);

					if (weight <= 0.0f)
						continue;

					int32_t texel = Coords::EncodeTexel(row, col);

					// Layer 0 has no mask of its own, it is simply what the others leave over, so
					// its plane of the accumulator is filled in here rather than decoded.
					float coverage[kMaxLayers] = {};
					float base = kFullCoverage;
					for (int32_t i = 1; i < count; ++i)
					{
						coverage[i] = state.coverage[i * kTexels + texel] / kFixedOne;
						base -= coverage[i];
					}

					coverage[0] = base < 0.0f ? 0.0f : base;

					if (!PaintTexel(coverage, count, stroke.layer, target, weight * rate))
						continue;

					for (int32_t i = 1; i < count; ++i)
					{
						state.coverage[i * kTexels + texel] = ToFixed(coverage[i]);

						uint8_t value = static_cast<uint8_t>(coverage[i] + 0.5f);
						if (alpha.maps[i].texel[texel] == value)
							continue;

						alpha.maps[i].texel[texel] = value;
						++changed;
					}
				}
			}

			if (!changed)
				return 0;

			std::string error;
			if (!Adt::WriteChunkAlpha(alpha, format, *mcnk, error))
				return 0;

			tile.dirty = true;
			PushLive(state, chunk, *mcnk);
			return changed;
		}
	}

	void PushChunk(int32_t tileX, int32_t tileY, int32_t chunkX, int32_t chunkY, CMapChunk* chunk,
	    Adt::Mcnk const& mcnk)
	{
		ChunkKey key{tileX, tileY, chunkX, chunkY};
		PaintedChunk& state = Painted()[key];

		// The layer list has moved under the accumulator, so drop it. Clearing the decoded layers
		// is what the next stroke tests, and it re-seeds from the document.
		state.alpha = Adt::ChunkAlpha{};
		state.coverage.clear();

		PushLive(state, chunk, mcnk);
	}

	void Reset()
	{
		Painted().clear();
	}

	int32_t Apply(Stroke const& stroke)
	{
		if (stroke.radius <= 0.0f || stroke.blend == 0.0f)
			return 0;

		C3Vector lo{stroke.center.x - stroke.radius, stroke.center.y - stroke.radius, 0.0f};
		C3Vector hi{stroke.center.x + stroke.radius, stroke.center.y + stroke.radius, 0.0f};

		int32_t tileX0 = std::min(Coords::TileX(lo), Coords::TileX(hi));
		int32_t tileX1 = std::max(Coords::TileX(lo), Coords::TileX(hi));
		int32_t tileY0 = std::min(Coords::TileY(lo), Coords::TileY(hi));
		int32_t tileY1 = std::max(Coords::TileY(lo), Coords::TileY(hi));

		int32_t changed = 0;

		for (int32_t tileY = tileY0; tileY <= tileY1; ++tileY)
		{
			for (int32_t tileX = tileX0; tileX <= tileX1; ++tileX)
			{
				CMapArea* area = Access::GetArea(tileX, tileY);
				if (!Access::IsAreaReady(area))
					continue;

				Session::OpenTile* tile = nullptr;

				for (int32_t chunkY = 0; chunkY < kSide; ++chunkY)
				{
					for (int32_t chunkX = 0; chunkX < kSide; ++chunkX)
					{
						CMapChunk* chunk = Access::GetChunk(area, chunkX, chunkY);
						if (!Coords::CircleTouchesChunk(chunk, stroke.center, stroke.radius))
							continue;

						// Opened lazily so flying over a tile the brush never reaches does not
						// pull the whole thing off disk.
						if (!tile)
						{
							std::string error;
							tile = Session::Open(tileX, tileY, error);
							if (!tile)
								break;
						}

						ChunkKey key{tileX, tileY, chunkX, chunkY};
						changed += PaintChunk(stroke, key, chunk, *tile);
					}
				}
			}
		}

		return changed;
	}
}
