#include <Editor/Map/Adt/AdtDocument.h>

#include <cstdio>
#include <cstring>

namespace MapEditor::Adt
{
	namespace
	{
		constexpr uint32_t kIffSize = 8;
		constexpr uint32_t kMcnkHeaderSize = 128;

		void Append(std::vector<uint8_t>& out, void const* data, size_t size)
		{
			uint8_t const* begin = static_cast<uint8_t const*>(data);
			out.insert(out.end(), begin, begin + size);
		}

		void AppendU32(std::vector<uint8_t>& out, uint32_t value)
		{
			Append(out, &value, sizeof(value));
		}

		void PatchU32(std::vector<uint8_t>& out, uint32_t at, uint32_t value)
		{
			std::memcpy(out.data() + at, &value, sizeof(value));
		}

		// Rebuilds the header's sub-chunk offsets, measured from the MCNK IFF header, for the
		// sub-chunks that are actually present. What a header carries for an absent sub-chunk
		// varies by whichever tool produced the tile, so it is left alone and an edit that removes
		// a sub-chunk has to clear those fields itself.
		//
		// Size fields are only touched when the sub-chunk's length actually changed. The last
		// sub-chunk in a chunk absorbs any trailing bytes the tile carries, so recomputing
		// unconditionally would write a size the source file never had.
		void RelayoutMcnkHeader(Mcnk const& chunk, SMChunk& header)
		{
			header = chunk.header;

			uint32_t at = kIffSize + kMcnkHeaderSize;
			for (SubChunk const& sub : chunk.subs)
			{
				uint32_t span = kIffSize + static_cast<uint32_t>(sub.data.size());

				switch (sub.id)
				{
					case kMCVT: header.ofsHeight = at; break;
					case kMCNR: header.ofsNormal = at; break;
					case kMCLY: header.ofsLayer = at; break;
					case kMCRF: header.ofsRefs = at; break;
					case kMCAL:
						header.ofsAlpha = at;
						if (sub.Resized())
							header.sizeAlpha = span;
						break;
					case kMCSH:
						header.ofsShadow = at;
						// sizeShadow counts the data alone, unlike sizeAlpha and sizeLiquid which
						// both include the 8 byte sub-chunk header. Noggit writes a bare 0x200
						// here next to a sizeAlpha of 8 + length, so the asymmetry is real rather
						// than a quirk of one toolchain.
						if (sub.Resized())
							header.sizeShadow = static_cast<uint32_t>(sub.data.size());
						break;
					case kMCSE: header.ofsSndEmitters = at; break;
					case kMCLQ:
						header.ofsLiquid = at;
						if (sub.Resized())
							header.sizeLiquid = span;
						break;
					case kMCCV: header.ofsMCCV = at; break;
					default: break;
				}

				at += span;
			}
		}
	}

	bool Write(AdtDocument const& doc, std::vector<uint8_t>& out, std::string& error)
	{
		out.clear();

		// Where each top level chunk's IFF header starts, so MHDR and MCIN can be patched once
		// the real layout is known.
		uint32_t mhdrData = 0;
		uint32_t offsets[12] = {};  // mcin, mtex, mmdx, mmid, mwmo, mwid, mddf, modf, mfbo, mh2o, mtxf
		uint32_t mcinData = 0;
		bool sawMhdr = false;
		bool sawMcin = false;

		uint32_t mcnkPos[256] = {};
		uint32_t mcnkSize[256] = {};
		int32_t mcnkCount = 0;

		for (TopChunk const& top : doc.order)
		{
			uint32_t start = static_cast<uint32_t>(out.size());

			if (top.mcnkIndex >= 0)
			{
				if (top.mcnkIndex >= static_cast<int32_t>(doc.chunks.size()))
				{
					error = "MCNK index out of range";
					return false;
				}

				Mcnk const& chunk = doc.chunks[top.mcnkIndex];
				SMChunk header{};
				RelayoutMcnkHeader(chunk, header);

				AppendU32(out, kMCNK);
				AppendU32(out, 0);
				Append(out, &header, kMcnkHeaderSize);

				for (SubChunk const& sub : chunk.subs)
				{
					AppendU32(out, sub.id);
					AppendU32(out, sub.DeclaredSize());
					if (!sub.data.empty())
						Append(out, sub.data.data(), sub.data.size());
				}

				uint32_t total = static_cast<uint32_t>(out.size()) - start;
				PatchU32(out, start + 4, total - kIffSize);

				if (mcnkCount < 256)
				{
					mcnkPos[mcnkCount] = start;
					mcnkSize[mcnkCount] = total;
				}
				++mcnkCount;
				continue;
			}

			AppendU32(out, top.id);
			AppendU32(out, static_cast<uint32_t>(top.data.size()));
			if (!top.data.empty())
				Append(out, top.data.data(), top.data.size());

			switch (top.id)
			{
				case kMHDR:
					mhdrData = start + kIffSize;
					sawMhdr = true;
					break;
				case kMCIN:
					offsets[0] = start;
					mcinData = start + kIffSize;
					sawMcin = true;
					break;
				case kMTEX: offsets[1] = start; break;
				case kMMDX: offsets[2] = start; break;
				case kMMID: offsets[3] = start; break;
				case kMWMO: offsets[4] = start; break;
				case kMWID: offsets[5] = start; break;
				case kMDDF: offsets[6] = start; break;
				case kMODF: offsets[7] = start; break;
				case kMFBO: offsets[8] = start; break;
				case kMH2O: offsets[9] = start; break;
				case kMTXF: offsets[10] = start; break;
				default: break;
			}
		}

		if (!sawMhdr)
		{
			error = "document has no MHDR";
			return false;
		}

		// MHDR offsets are relative to MHDR's data start and point at the target's IFF header.
		// A chunk that is not present stays zero.
		SMMapHeader header = doc.header;
		uint32_t* fields[11] = {&header.mcin, &header.mtex, &header.mmdx, &header.mmid,
		    &header.mwmo, &header.mwid, &header.mddf, &header.modf, &header.mfbo, &header.mh2o,
		    &header.mtxf};

		for (int i = 0; i < 11; ++i)
			*fields[i] = offsets[i] ? offsets[i] - mhdrData : 0;

		std::memcpy(out.data() + mhdrData, &header, sizeof(header));

		// MCIN offsets are absolute from file start. The size normally counts the whole MCNK
		// including its 8 byte IFF header, but the reader measured which convention this file uses,
		// so follow it rather than impose one.
		if (sawMcin)
		{
			for (int32_t i = 0; i < mcnkCount && i < 256; ++i)
			{
				SMChunkInfo entry = doc.info[i];
				entry.offset = mcnkPos[i];
				entry.size = doc.mcinSizeIncludesHeader ? mcnkSize[i] : mcnkSize[i] - kIffSize;
				std::memcpy(out.data() + mcinData + i * sizeof(SMChunkInfo), &entry, sizeof(entry));
			}
		}

		return true;
	}
}

