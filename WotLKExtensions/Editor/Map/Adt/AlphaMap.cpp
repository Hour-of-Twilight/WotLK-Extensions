#include <Editor/Map/Adt/AlphaMap.h>

#include <cstdio>
#include <cstring>

namespace MapEditor::Adt
{
	namespace
	{
		// The client rebuilds the last row and column from the one before it whenever the chunk's
		// doNotFix flag is clear, so the stored bytes there are dead. Doing the same on read keeps
		// what gets painted matching what is drawn, and doing it on write keeps the file agreeing
		// with both.
		void FixLastRowAndColumn(AlphaMap& map)
		{
			for (int32_t i = 0; i < kAlphaSide; ++i)
			{
				map.texel[i * kAlphaSide + 63] = map.texel[i * kAlphaSide + 62];
				map.texel[63 * kAlphaSide + i] = map.texel[62 * kAlphaSide + i];
			}

			map.texel[63 * kAlphaSide + 63] = map.texel[62 * kAlphaSide + 62];
		}

		// MCAL RLE. Each command byte is a 7 bit count plus a fill/copy bit: fill repeats the one
		// byte that follows, copy takes the next count bytes verbatim.
		bool DecodeCompressed(uint8_t const* data, size_t available, AlphaMap& out,
		    std::string& error)
		{
			size_t read = 0;
			int32_t written = 0;

			while (written < kAlphaTexels)
			{
				if (read >= available)
				{
					error = "compressed alpha ran out of input";
					return false;
				}

				uint8_t command = data[read++];
				int32_t count = command & 0x7F;
				bool fill = (command & 0x80) != 0;

				// A truncated run is worth clamping rather than refusing, since the rest of the
				// map is still perfectly good and a hard failure would block editing the tile.
				if (written + count > kAlphaTexels)
					count = kAlphaTexels - written;

				if (!count)
					continue;

				if (fill)
				{
					if (read >= available)
					{
						error = "compressed alpha ran out of input mid fill";
						return false;
					}

					std::memset(out.texel + written, data[read++], count);
				}
				else
				{
					if (read + count > available)
					{
						error = "compressed alpha ran out of input mid copy";
						return false;
					}

					std::memcpy(out.texel + written, data + read, count);
					read += count;
				}

				written += count;
			}

			return true;
		}

		bool DecodeOne(uint8_t const* data, size_t available, uint32_t layerFlags,
		    AlphaFormat const& format, AlphaMap& out, std::string& error)
		{
			// Compression only ever appears alongside big alpha, so a 4 bit map is always flat.
			if (format.bigAlpha && (layerFlags & kLayerCompressed))
			{
				if (!DecodeCompressed(data, available, out, error))
					return false;
			}
			else if (format.bigAlpha)
			{
				if (available < static_cast<size_t>(kAlphaTexels))
				{
					error = "big alpha layer is short";
					return false;
				}

				std::memcpy(out.texel, data, kAlphaTexels);
			}
			else
			{
				if (available < static_cast<size_t>(kAlphaTexels / 2))
				{
					error = "packed alpha layer is short";
					return false;
				}

				for (int32_t i = 0; i < kAlphaTexels / 2; ++i)
				{
					// The low nibble is the even texel. Both widen by repeating the nibble rather
					// than shifting, so 0xF becomes 0xFF and full opacity stays full.
					uint8_t lo = data[i] & 0x0F;
					uint8_t hi = (data[i] >> 4) & 0x0F;

					out.texel[i * 2 + 0] = static_cast<uint8_t>(lo | (lo << 4));
					out.texel[i * 2 + 1] = static_cast<uint8_t>(hi | (hi << 4));
				}
			}

			if (!format.doNotFix)
				FixLastRowAndColumn(out);

			return true;
		}

		// 8 bits back down to 4, to the nearest of the 16 values the file can hold rather than down
		// to the next one. Decoding widens by repeating the nibble so the levels sit 17 apart, and
		// a plain `>> 4` loses enough that a slow stroke never moves a texel at all.
		uint8_t ToNibble(uint8_t value)
		{
			return static_cast<uint8_t>((value * 15 + 127) / 255);
		}

		void EncodeOne(AlphaMap const& map, AlphaFormat const& format, std::vector<uint8_t>& out)
		{
			if (format.bigAlpha)
			{
				out.insert(out.end(), map.texel, map.texel + kAlphaTexels);
				return;
			}

			for (int32_t i = 0; i < kAlphaTexels / 2; ++i)
			{
				uint8_t lo = ToNibble(map.texel[i * 2 + 0]);
				uint8_t hi = ToNibble(map.texel[i * 2 + 1]);
				out.push_back(static_cast<uint8_t>(lo | (hi << 4)));
			}
		}

		// Where MCAL belongs when a chunk never had one. The client dispatches on magic rather
		// than position, but keeping the usual order means the file still looks like every other
		// tile to anything else that reads it.
		size_t McalInsertPoint(Mcnk const& mcnk)
		{
			uint32_t const before[] = { kMCRF, kMCLY, kMCNR, kMCVT };

			for (uint32_t id : before)
			{
				for (size_t i = 0; i < mcnk.subs.size(); ++i)
				{
					if (mcnk.subs[i].id == id)
						return i + 1;
				}
			}

			return mcnk.subs.size();
		}
	}

	bool ReadMapFlags(char const* mapName, uint32_t& flags, std::string& error)
	{
		flags = 0;

		if (!mapName || !*mapName)
		{
			error = "no map name";
			return false;
		}

		char path[512];
		std::snprintf(path, sizeof(path), "World\\Maps\\%s\\%s.wdt", mapName, mapName);

		std::vector<uint8_t> bytes;
		if (!ReadFileBytes(path, bytes, error))
			return false;

		// Plain IFF walk to MPHD. The client reads it positionally straight after MVER, but
		// seeking by magic costs nothing and survives a file that puts them the other way round.
		uint32_t at = 0;
		while (static_cast<size_t>(at) + 8 <= bytes.size())
		{
			uint32_t id = 0;
			uint32_t size = 0;
			std::memcpy(&id, bytes.data() + at, 4);
			std::memcpy(&size, bytes.data() + at + 4, 4);

			if (id == ChunkId("MPHD"))
			{
				if (size < 4 || static_cast<size_t>(at) + 8 + 4 > bytes.size())
				{
					error = "MPHD too short";
					return false;
				}

				std::memcpy(&flags, bytes.data() + at + 8, 4);
				return true;
			}

			at += 8 + size;
		}

		error = "no MPHD in the WDT";
		return false;
	}

	AlphaFormat FormatFor(Mcnk const& mcnk, uint32_t mapFlags)
	{
		AlphaFormat out;
		out.bigAlpha = (mapFlags & 0x4) != 0;
		out.doNotFix = (mcnk.header.flags & kChunkDoNotFixAlpha) != 0;
		return out;
	}

	bool ReadChunkAlpha(Mcnk const& mcnk, AlphaFormat const& format, ChunkAlpha& out,
	    std::string& error)
	{
		out.layers.clear();
		out.maps.clear();

		uint32_t count = mcnk.header.nLayers;
		if (!count)
			return true;

		SubChunk const* mcly = mcnk.Find(kMCLY);
		if (!mcly || mcly->data.size() < count * sizeof(SMLayer))
		{
			error = "MCLY missing or too short for the layer count";
			return false;
		}

		out.layers.resize(count);
		std::memcpy(out.layers.data(), mcly->data.data(), count * sizeof(SMLayer));
		out.maps.resize(count);

		SubChunk const* mcal = mcnk.Find(kMCAL);
		size_t available = mcal ? mcal->data.size() : 0;

		for (uint32_t i = 0; i < count; ++i)
		{
			SMLayer const& layer = out.layers[i];
			if (!(layer.flags & kLayerUseAlpha))
				continue;

			if (!available || layer.offsetInMCAL >= available)
			{
				error = "layer alpha offset lands past the end of MCAL";
				return false;
			}

			if (!DecodeOne(mcal->data.data() + layer.offsetInMCAL, available - layer.offsetInMCAL,
			        layer.flags, format, out.maps[i], error))
				return false;
		}

		return true;
	}

	bool WriteChunkAlpha(ChunkAlpha const& in, AlphaFormat const& format, Mcnk& mcnk,
	    std::string& error)
	{
		if (in.layers.size() != in.maps.size())
		{
			error = "layer and mask counts disagree";
			return false;
		}

		std::vector<SMLayer> layers = in.layers;
		std::vector<uint8_t> alpha;

		for (size_t i = 0; i < layers.size(); ++i)
		{
			SMLayer& layer = layers[i];

			if (i == 0)
			{
				layer.flags &= ~(kLayerUseAlpha | kLayerCompressed);
				layer.offsetInMCAL = 0;
				continue;
			}

			layer.flags |= kLayerUseAlpha;
			layer.flags &= ~kLayerCompressed;
			layer.offsetInMCAL = static_cast<uint32_t>(alpha.size());

			AlphaMap map = in.maps[i];
			if (!format.doNotFix)
				FixLastRowAndColumn(map);

			EncodeOne(map, format, alpha);
		}

		SubChunk* mcly = mcnk.Find(kMCLY);
		if (!mcly)
		{
			error = "chunk has no MCLY to write layers into";
			return false;
		}

		mcly->data.resize(layers.size() * sizeof(SMLayer));
		if (!layers.empty())
			std::memcpy(mcly->data.data(), layers.data(), mcly->data.size());

		SubChunk* mcal = mcnk.Find(kMCAL);
		if (!mcal)
		{
			SubChunk fresh;
			fresh.id = kMCAL;
			mcnk.subs.insert(mcnk.subs.begin() + McalInsertPoint(mcnk), std::move(fresh));
			mcal = mcnk.Find(kMCAL);
		}

		mcal->data = std::move(alpha);
		mcnk.header.nLayers = static_cast<uint32_t>(layers.size());
		return true;
	}
}
