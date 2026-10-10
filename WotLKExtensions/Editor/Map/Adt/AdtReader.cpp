#include <Defines.h>
#include <ClientData/SharedDefines.h>
#include <Editor/Map/Adt/AdtDocument.h>
#include <Editor/Map/MapClient.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace MapEditor::Adt
{
	namespace
	{
		struct IffHeader
		{
			uint32 id;
			uint32 size;
		};

		// MCNK sub-chunk offsets are measured from the start of the MCNK IFF header, not from its
		// data. Confirmed against a real tile: ofsAlpha 0x4D0 equals 8 + 128 header + 588 MCVT
		// + 456 MCNR + 40 MCLY + 12 MCRF.
		constexpr uint32 kIffSize = 8;
		constexpr uint32 kMcnkHeaderSize = 128;
	}

	std::string ChunkName(uint32 id)
	{
		char text[5] = {};
		text[0] = static_cast<char>((id >> 24) & 0xFF);
		text[1] = static_cast<char>((id >> 16) & 0xFF);
		text[2] = static_cast<char>((id >> 8) & 0xFF);
		text[3] = static_cast<char>(id & 0xFF);

		for (int i = 0; i < 4; ++i)
		{
			if (text[i] < 32 || text[i] > 126)
				text[i] = '?';
		}

		return std::string(text);
	}

	SubChunk const* Mcnk::Find(uint32 id) const
	{
		for (SubChunk const& sub : subs)
		{
			if (sub.id == id)
				return &sub;
		}
		return nullptr;
	}

	SubChunk* Mcnk::Find(uint32 id)
	{
		for (SubChunk& sub : subs)
		{
			if (sub.id == id)
				return &sub;
		}
		return nullptr;
	}

	Mcnk* AdtDocument::ChunkAt(int32 chunkX, int32 chunkY)
	{
		int32 index = chunkY * 16 + chunkX;
		if (chunkX < 0 || chunkX > 15 || chunkY < 0 || chunkY > 15 || index >= static_cast<int32>(chunks.size()))
			return nullptr;

		return &chunks[index];
	}

	Mcnk const* AdtDocument::ChunkAt(int32 chunkX, int32 chunkY) const
	{
		return const_cast<AdtDocument*>(this)->ChunkAt(chunkX, chunkY);
	}

	std::string TilePath(char const* mapName, int32 tileX, int32 tileY)
	{
		char path[260] = {};
		std::snprintf(path, sizeof(path), "World\\Maps\\%s\\%s_%d_%d.adt", mapName, mapName,
		    tileX, tileY);
		return std::string(path);
	}

	std::string TilePath(int32 tileX, int32 tileY)
	{
		return TilePath(Access::sMapName, tileX, tileY);
	}

	bool ReadFileBytes(char const* path, std::vector<uint8>& out, std::string& error)
	{
		HANDLE file = nullptr;
		if (!SFile::OpenFile(path, &file) || !file)
		{
			error = std::string("SFile could not open ") + path;
			return false;
		}

		uint32 size = SFile::GetFileSize(file, nullptr);
		if (size == 0 || size == 0xFFFFFFFF)
		{
			SFile::CloseFile(file);
			error = std::string("bad file size for ") + path;
			return false;
		}

		out.assign(size, 0);
		uint32 read = 0;
		bool ok = SFile::ReadFile(file, out.data(), size, &read, nullptr, 0);
		SFile::CloseFile(file);

		if (!ok || read != size)
		{
			error = std::string("short read on ") + path;
			return false;
		}

		return true;
	}

	bool Read(uint8 const* bytes, uint32 size, AdtDocument& out, std::string& error)
	{
		out = AdtDocument();
		out.chunks.resize(256);

		uint32 pos = 0;
		int32 mcnkSeen = 0;

		// Tally of which MCIN size convention the file uses, settled by measuring against the
		// MCNK chunks themselves rather than assuming.
		int32 sizeWithHeader = 0;
		int32 sizeWithoutHeader = 0;

		while (pos + kIffSize <= size)
		{
			IffHeader head{};
			std::memcpy(&head, bytes + pos, kIffSize);

			if (static_cast<uint64>(pos) + kIffSize + head.size > size)
			{
				char message[160];
				std::snprintf(message, sizeof(message), "%s at 0x%X claims %u bytes, past the end",
				    ChunkName(head.id).c_str(), pos, head.size);
				error = message;
				return false;
			}

			uint8 const* data = bytes + pos + kIffSize;

			if (head.id == kMCNK)
			{
				if (mcnkSeen >= 256)
				{
					error = "more than 256 MCNK chunks";
					return false;
				}

				if (head.size < kMcnkHeaderSize)
				{
					error = "MCNK smaller than its header";
					return false;
				}

				Mcnk& chunk = out.chunks[mcnkSeen];
				std::memcpy(&chunk.header, data, kMcnkHeaderSize);

				// Where a sub-chunk ends can only come from where the next one starts. Nothing
				// stored with the sub-chunk survives contact with real tiles, Northrend has two
				// neighbouring chunks with identical sizeAlpha where one MCAL declares 8 and holds
				// 8 bytes and the other declares 512 and holds none, so only the header's offset
				// table can tell them apart.
				uint32 bounds[9] = {};
				uint32 boundCount = 0;

				uint32 const fields[] = { chunk.header.ofsHeight, chunk.header.ofsMCCV,
					chunk.header.ofsNormal, chunk.header.ofsLayer, chunk.header.ofsRefs,
					chunk.header.ofsAlpha, chunk.header.ofsShadow, chunk.header.ofsLiquid,
					chunk.header.ofsSndEmitters };

				for (uint32 field : fields)
				{
					// Absent sub-chunks leave a 0 here, and an offset for one that is absent can
					// also alias another's, so both are filtered out rather than trusted.
					if (field < kIffSize + kMcnkHeaderSize)
						continue;

					uint32 at = field - kIffSize;
					if (static_cast<uint64>(at) + kIffSize > head.size)
						continue;

					if (std::find(bounds, bounds + boundCount, at) == bounds + boundCount)
						bounds[boundCount++] = at;
				}

				std::sort(bounds, bounds + boundCount);

				uint32 sub = kMcnkHeaderSize;
				while (sub + kIffSize <= head.size)
				{
					IffHeader subHead{};
					std::memcpy(&subHead, data + sub, kIffSize);

					// The first boundary that leaves room for this sub-chunk's own header is its
					// end. The last sub-chunk runs to the end of the MCNK, which also soaks up
					// the handful of tiles carrying a few unaccounted trailing bytes.
					uint32 end = head.size;
					uint32 const* next = std::upper_bound(bounds, bounds + boundCount, sub + kIffSize - 1);
					if (next != bounds + boundCount)
						end = *next;

					SubChunk entry;
					entry.id = subHead.id;
					entry.declared = subHead.size;
					entry.data.assign(data + sub + kIffSize, data + end);
					entry.originalSize = static_cast<uint32>(entry.data.size());
					chunk.subs.push_back(std::move(entry));

					sub = end;
				}

				if (out.hasInfo)
				{
					uint32 stated = out.info[mcnkSeen].size;
					if (stated == head.size + kIffSize)
						++sizeWithHeader;
					else if (stated == head.size)
						++sizeWithoutHeader;
				}

				TopChunk top;
				top.id = kMCNK;
				top.mcnkIndex = mcnkSeen;
				out.order.push_back(std::move(top));
				++mcnkSeen;
			}
			else
			{
				if (head.id == kMHDR && head.size >= sizeof(SMMapHeader))
				{
					std::memcpy(&out.header, data, sizeof(SMMapHeader));
					out.hasHeader = true;
				}
				else if (head.id == kMCIN && head.size >= sizeof(out.info))
				{
					std::memcpy(out.info, data, sizeof(out.info));
					out.hasInfo = true;
				}

				TopChunk top;
				top.id = head.id;
				top.data.assign(data, data + head.size);
				out.order.push_back(std::move(top));
			}

			pos += kIffSize + head.size;
		}

		if (pos != size)
		{
			char message[128];
			std::snprintf(message, sizeof(message), "%u trailing bytes after the last chunk",
			    size - pos);
			error = message;
			return false;
		}

		if (!out.hasHeader)
		{
			error = "no MHDR";
			return false;
		}

		out.mcinSizeIncludesHeader = sizeWithoutHeader <= sizeWithHeader;
		out.chunks.resize(mcnkSeen);
		return true;
	}

	int32 ChunkIndexAtOffset(AdtDocument const& doc, uint32 offset)
	{
		uint32 pos = 0;
		for (TopChunk const& top : doc.order)
		{
			uint32 body = static_cast<uint32>(top.data.size());
			if (top.mcnkIndex >= 0)
			{
				body = kMcnkHeaderSize;
				for (SubChunk const& sub : doc.chunks[top.mcnkIndex].subs)
					body += kIffSize + static_cast<uint32>(sub.data.size());
			}

			uint32 total = kIffSize + body;
			if (offset < pos + total)
				return top.mcnkIndex;

			pos += total;
		}

		return -1;
	}

	std::string DescribeOffset(AdtDocument const& doc, uint32 offset)
	{
		uint32 pos = 0;
		for (TopChunk const& top : doc.order)
		{
			uint32 body = top.mcnkIndex >= 0 ? 0 : static_cast<uint32>(top.data.size());
			if (top.mcnkIndex >= 0)
			{
				Mcnk const& chunk = doc.chunks[top.mcnkIndex];
				body = kMcnkHeaderSize;
				for (SubChunk const& sub : chunk.subs)
					body += kIffSize + static_cast<uint32>(sub.data.size());
			}

			uint32 total = kIffSize + body;
			if (offset < pos + total)
			{
				uint32 inner = offset - pos;
				char message[192];

				if (top.mcnkIndex < 0)
				{
					std::snprintf(message, sizeof(message), "%s +0x%X",
					    ChunkName(top.id).c_str(), inner);
					return message;
				}

				Mcnk const& chunk = doc.chunks[top.mcnkIndex];
				if (inner < kIffSize + kMcnkHeaderSize)
				{
					std::snprintf(message, sizeof(message), "MCNK %d (%d,%d) header +0x%X",
					    top.mcnkIndex, chunk.header.index.x, chunk.header.index.y,
					    inner - kIffSize);
					return message;
				}

				uint32 sub = kIffSize + kMcnkHeaderSize;
				for (SubChunk const& entry : chunk.subs)
				{
					uint32 span = kIffSize + static_cast<uint32>(entry.data.size());
					if (inner < sub + span)
					{
						std::snprintf(message, sizeof(message), "MCNK %d (%d,%d) %s +0x%X",
						    top.mcnkIndex, chunk.header.index.x, chunk.header.index.y,
						    ChunkName(entry.id).c_str(), inner - sub - kIffSize);
						return message;
					}
					sub += span;
				}

				std::snprintf(message, sizeof(message), "MCNK %d +0x%X", top.mcnkIndex, inner);
				return message;
			}

			pos += total;
		}

		return "past the end";
	}
}
