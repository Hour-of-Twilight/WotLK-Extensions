#include <Editor/Map/Adt/LiquidChunk.h>

#include <cstdio>
#include <cstring>

namespace MapEditor::Adt
{
	namespace
	{
		constexpr size_t kHeaderTable = 256 * sizeof(SMLiquidChunk);

		TopChunk const* FindTop(AdtDocument const& doc, uint32_t id)
		{
			for (TopChunk const& top : doc.order)
			{
				if (top.id == id)
					return &top;
			}

			return nullptr;
		}

		void Append(std::vector<uint8_t>& blob, void const* from, size_t bytes)
		{
			uint8_t const* at = static_cast<uint8_t const*>(from);
			blob.insert(blob.end(), at, at + bytes);
		}

		// Floats and offsets are read straight out of the blob by the client, so every block that
		// holds one starts on a four byte boundary. Only the depth and exists byte arrays can
		// leave it off.
		void Pad(std::vector<uint8_t>& blob)
		{
			while (blob.size() % 4)
				blob.push_back(0);
		}

		// Bytes one instance's vertex block takes, n being (width + 1) * (height + 1).
		size_t VertexBlockSize(uint16_t format, size_t n)
		{
			switch (format)
			{
			case kLiquidHeightDepth:
				return n * sizeof(float) + n;
			case kLiquidHeightUv:
				return n * sizeof(float) + n * 2 * sizeof(uint16_t);
			case kLiquidDepthOnly:
				return n;
			default:
				return 0;
			}
		}
	}

	int32_t LiquidLayer::CellCount() const
	{
		int32_t count = 0;
		for (bool on : exists)
		{
			if (on)
				++count;
		}

		return count;
	}

	bool ReadTileLiquid(AdtDocument const& doc, TileLiquid& out, std::string& error)
	{
		out = TileLiquid();

		TopChunk const* mh2o = FindTop(doc, kMH2O);
		if (!mh2o || mh2o->data.empty())
			return true;

		uint8_t const* base = mh2o->data.data();
		size_t size = mh2o->data.size();

		if (size < kHeaderTable)
		{
			error = "MH2O is shorter than its 256 chunk headers";
			return false;
		}

		out.present = true;

		for (int32_t index = 0; index < 256; ++index)
		{
			SMLiquidChunk head{};
			std::memcpy(&head, base + index * sizeof(SMLiquidChunk), sizeof(SMLiquidChunk));

			if (head.layerCount == 0)
				continue;

			ChunkLiquid& chunk = out.chunks[index];

			// A zero here is the client's shorthand for every cell, both masks. sub_8A30B0 and
			// sub_8A30E0 hand back an all ones static rather than reading anything.
			if (head.offsetAttributes == 0)
			{
				chunk.fishable = ~0ull;
				chunk.fatigue = ~0ull;
			}
			else if (head.offsetAttributes + 16 <= size)
			{
				std::memcpy(&chunk.fishable, base + head.offsetAttributes, sizeof(uint64_t));
				std::memcpy(&chunk.fatigue, base + head.offsetAttributes + 8, sizeof(uint64_t));
			}
			else
			{
				error = "MH2O attributes run past the end of the chunk";
				return false;
			}

			for (uint32_t i = 0; i < head.layerCount; ++i)
			{
				size_t at = head.offsetInstances + i * sizeof(SMLiquidInstance);
				if (at + sizeof(SMLiquidInstance) > size)
				{
					error = "MH2O instance runs past the end of the chunk";
					return false;
				}

				SMLiquidInstance inst{};
				std::memcpy(&inst, base + at, sizeof(SMLiquidInstance));

				int32_t ox = inst.xOffset;
				int32_t oy = inst.yOffset;
				int32_t w = inst.width;
				int32_t h = inst.height;

				if (w < 1 || h < 1 || ox + w > kLiquidCellSide || oy + h > kLiquidCellSide)
				{
					char message[128];
					std::snprintf(message, sizeof(message),
					    "MH2O chunk %d layer %u covers %d,%d %dx%d, which is off the grid", index, i,
					    ox, oy, w, h);
					error = message;
					return false;
				}

				LiquidLayer layer;
				layer.type = inst.liquidType;
				layer.format = inst.vertexFormat;

				// Seeded so the cells around whatever the file stored are already sane, which is
				// what lets a caller grow the liquid outwards without inventing heights. Depth only
				// layers really do render at absolute zero, since sub_8A3130 returns 0.0 for them.
				float seed = inst.vertexFormat == kLiquidDepthOnly ? 0.0f : inst.minHeight;
				for (int32_t v = 0; v < kLiquidVerts; ++v)
				{
					layer.height[v] = seed;
					layer.depth[v] = 255;
				}

				for (int32_t cy = 0; cy < h; ++cy)
				{
					for (int32_t cx = 0; cx < w; ++cx)
					{
						bool on = true;
						if (inst.offsetExistsBitmap)
						{
							int32_t bit = cy * w + cx;
							size_t byteAt = inst.offsetExistsBitmap + bit / 8;
							if (byteAt >= size)
							{
								error = "MH2O exists bitmap runs past the end of the chunk";
								return false;
							}

							on = (base[byteAt] >> (bit & 7)) & 1;
						}

						if (on)
							layer.exists[LiquidCell(ox + cx, oy + cy)] = true;
					}
				}

				int32_t vw = w + 1;
				int32_t vh = h + 1;
				size_t n = static_cast<size_t>(vw) * vh;
				uint32_t vd = inst.offsetVertexData;

				if (vd && vd + VertexBlockSize(inst.vertexFormat, n) > size)
				{
					error = "MH2O vertex data runs past the end of the chunk";
					return false;
				}

				// A zero offset is not the flat surface it looks like, sub_8A3130 never checks for
				// it and would read the header table back as floats. Nothing the editor writes
				// leaves it at zero, and reading one here just keeps the seeded heights.
				if (vd)
				{
					for (int32_t vy = 0; vy < vh; ++vy)
					{
						for (int32_t vx = 0; vx < vw; ++vx)
						{
							size_t src = static_cast<size_t>(vy) * vw + vx;
							int32_t dst = LiquidVert(ox + vx, oy + vy);

							switch (inst.vertexFormat)
							{
							case kLiquidHeightDepth:
								std::memcpy(&layer.height[dst], base + vd + src * 4, sizeof(float));
								layer.depth[dst] = base[vd + n * 4 + src];
								break;

							case kLiquidHeightUv:
								std::memcpy(&layer.height[dst], base + vd + src * 4, sizeof(float));
								std::memcpy(&layer.uv[dst * 2], base + vd + (n + src) * 4,
								    2 * sizeof(uint16_t));
								break;

							case kLiquidDepthOnly:
								layer.depth[dst] = base[vd + src];
								break;

							default:
								break;
							}
						}
					}
				}

				chunk.layers.push_back(layer);
			}
		}

		return true;
	}

	bool WriteTileLiquid(TileLiquid const& in, AdtDocument& doc, std::string& error)
	{
		std::vector<uint8_t> blob(kHeaderTable, 0);
		bool any = false;

		for (int32_t index = 0; index < 256; ++index)
		{
			ChunkLiquid const& chunk = in.chunks[index];

			std::vector<LiquidLayer const*> live;
			for (LiquidLayer const& layer : chunk.layers)
			{
				if (layer.Any())
					live.push_back(&layer);
			}

			if (live.empty())
				continue;

			any = true;

			SMLiquidChunk head{};
			head.layerCount = static_cast<uint32_t>(live.size());

			// Written out rather than left at zero. Zero means all ones, which is a trap waiting
			// for the first caller who clears a mask and finds it comes back full.
			Pad(blob);
			head.offsetAttributes = static_cast<uint32_t>(blob.size());
			Append(blob, &chunk.fishable, sizeof(uint64_t));
			Append(blob, &chunk.fatigue, sizeof(uint64_t));

			head.offsetInstances = static_cast<uint32_t>(blob.size());
			size_t instanceAt = blob.size();
			blob.resize(blob.size() + live.size() * sizeof(SMLiquidInstance), 0);

			for (size_t i = 0; i < live.size(); ++i)
			{
				LiquidLayer const& layer = *live[i];

				int32_t minX = kLiquidCellSide;
				int32_t minY = kLiquidCellSide;
				int32_t maxX = -1;
				int32_t maxY = -1;

				for (int32_t cy = 0; cy < kLiquidCellSide; ++cy)
				{
					for (int32_t cx = 0; cx < kLiquidCellSide; ++cx)
					{
						if (!layer.exists[LiquidCell(cx, cy)])
							continue;

						minX = cx < minX ? cx : minX;
						minY = cy < minY ? cy : minY;
						maxX = cx > maxX ? cx : maxX;
						maxY = cy > maxY ? cy : maxY;
					}
				}

				int32_t w = maxX - minX + 1;
				int32_t h = maxY - minY + 1;
				int32_t vw = w + 1;
				int32_t vh = h + 1;

				SMLiquidInstance inst{};
				inst.liquidType = layer.type;
				inst.vertexFormat = layer.format <= kLiquidDepthOnly ? layer.format
				                                                     : kLiquidHeightDepth;
				inst.xOffset = static_cast<uint8_t>(minX);
				inst.yOffset = static_cast<uint8_t>(minY);
				inst.width = static_cast<uint8_t>(w);
				inst.height = static_cast<uint8_t>(h);

				// The client renders a depth only layer at absolute zero whatever the heights say,
				// so a layer that has been moved off zero has to give up the format.
				if (inst.vertexFormat == kLiquidDepthOnly)
				{
					for (int32_t vy = 0; vy < vh && inst.vertexFormat == kLiquidDepthOnly; ++vy)
					{
						for (int32_t vx = 0; vx < vw; ++vx)
						{
							if (layer.height[LiquidVert(minX + vx, minY + vy)] != 0.0f)
							{
								inst.vertexFormat = kLiquidHeightDepth;
								break;
							}
						}
					}
				}

				float lo = 0.0f;
				float hi = 0.0f;
				if (inst.vertexFormat != kLiquidDepthOnly)
				{
					lo = layer.height[LiquidVert(minX, minY)];
					hi = lo;

					for (int32_t vy = 0; vy < vh; ++vy)
					{
						for (int32_t vx = 0; vx < vw; ++vx)
						{
							float z = layer.height[LiquidVert(minX + vx, minY + vy)];
							lo = z < lo ? z : lo;
							hi = z > hi ? z : hi;
						}
					}
				}

				inst.minHeight = lo;
				inst.maxHeight = hi;

				// Always written, even when every cell in the rectangle is on. It costs eight bytes
				// and it means a later edit never has to tell a missing bitmap from a full one.
				Pad(blob);
				inst.offsetExistsBitmap = static_cast<uint32_t>(blob.size());
				blob.resize(blob.size() + 8, 0);

				for (int32_t cy = 0; cy < h; ++cy)
				{
					for (int32_t cx = 0; cx < w; ++cx)
					{
						if (!layer.exists[LiquidCell(minX + cx, minY + cy)])
							continue;

						int32_t bit = cy * w + cx;
						blob[inst.offsetExistsBitmap + bit / 8] |= static_cast<uint8_t>(1 << (bit & 7));
					}
				}

				Pad(blob);
				inst.offsetVertexData = static_cast<uint32_t>(blob.size());

				if (inst.vertexFormat != kLiquidDepthOnly)
				{
					for (int32_t vy = 0; vy < vh; ++vy)
					{
						for (int32_t vx = 0; vx < vw; ++vx)
						{
							float z = layer.height[LiquidVert(minX + vx, minY + vy)];
							Append(blob, &z, sizeof(float));
						}
					}
				}

				if (inst.vertexFormat == kLiquidHeightUv)
				{
					for (int32_t vy = 0; vy < vh; ++vy)
					{
						for (int32_t vx = 0; vx < vw; ++vx)
						{
							int32_t v = LiquidVert(minX + vx, minY + vy);
							Append(blob, &layer.uv[v * 2], 2 * sizeof(uint16_t));
						}
					}
				}
				else
				{
					for (int32_t vy = 0; vy < vh; ++vy)
					{
						for (int32_t vx = 0; vx < vw; ++vx)
							blob.push_back(layer.depth[LiquidVert(minX + vx, minY + vy)]);
					}
				}

				std::memcpy(blob.data() + instanceAt + i * sizeof(SMLiquidInstance), &inst,
				    sizeof(SMLiquidInstance));
			}

			std::memcpy(blob.data() + index * sizeof(SMLiquidChunk), &head, sizeof(SMLiquidChunk));

			int32_t chunkX = index % 16;
			int32_t chunkY = index / 16;
			if (Mcnk* mcnk = doc.ChunkAt(chunkX, chunkY))
			{
				for (size_t i = 0; i < mcnk->subs.size(); ++i)
				{
					if (mcnk->subs[i].id != kMCLQ)
						continue;

					mcnk->subs.erase(mcnk->subs.begin() + i);
					mcnk->header.ofsLiquid = 0;
					mcnk->header.sizeLiquid = 0;

					// Bits 2 through 5 are the legacy has-river, has-ocean, has-magma, has-slime
					// flags CMapChunk::CreateLiquids walks before it ever looks at MH2O.
					mcnk->header.flags &= ~0x3Cu;
					break;
				}
			}
		}

		for (size_t i = 0; i < doc.order.size(); ++i)
		{
			if (doc.order[i].id != kMH2O)
				continue;

			if (any)
				doc.order[i].data = std::move(blob);
			else
				doc.order.erase(doc.order.begin() + i);

			return true;
		}

		if (!any)
			return true;

		for (size_t i = 0; i < doc.order.size(); ++i)
		{
			if (doc.order[i].mcnkIndex < 0)
				continue;

			TopChunk top;
			top.id = kMH2O;
			top.data = std::move(blob);
			doc.order.insert(doc.order.begin() + i, std::move(top));
			return true;
		}

		error = "tile has no MCNK to put MH2O in front of";
		return false;
	}
}
