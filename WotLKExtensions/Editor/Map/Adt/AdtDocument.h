#pragma once

#include <Defines.h>

#include <Editor/Map/MapClient.h>

#include <cstdint>
#include <string>
#include <vector>

namespace MapEditor::Adt
{
	// Chunk magics sit reversed on disk, so reading the four bytes as a little endian uint32
	// gives back the natural spelling. 'M','V','E','R' on disk is "REVM", which reads as
	// 0x4D564552, exactly what ChunkId("MVER") builds.
	constexpr uint32 ChunkId(char const (&text)[5])
	{
		return static_cast<uint32>(static_cast<uint8>(text[0])) << 24 | static_cast<uint32>(static_cast<uint8>(text[1])) << 16 | static_cast<uint32>(static_cast<uint8>(text[2])) << 8 | static_cast<uint32>(static_cast<uint8>(text[3]));
	}

	constexpr uint32 kMVER = ChunkId("MVER");
	constexpr uint32 kMHDR = ChunkId("MHDR");
	constexpr uint32 kMCIN = ChunkId("MCIN");
	constexpr uint32 kMTEX = ChunkId("MTEX");
	constexpr uint32 kMMDX = ChunkId("MMDX");
	constexpr uint32 kMMID = ChunkId("MMID");
	constexpr uint32 kMWMO = ChunkId("MWMO");
	constexpr uint32 kMWID = ChunkId("MWID");
	constexpr uint32 kMDDF = ChunkId("MDDF");
	constexpr uint32 kMODF = ChunkId("MODF");
	constexpr uint32 kMFBO = ChunkId("MFBO");
	constexpr uint32 kMH2O = ChunkId("MH2O");
	constexpr uint32 kMTXF = ChunkId("MTXF");
	constexpr uint32 kMCNK = ChunkId("MCNK");

	constexpr uint32 kMCVT = ChunkId("MCVT");
	constexpr uint32 kMCNR = ChunkId("MCNR");
	constexpr uint32 kMCLY = ChunkId("MCLY");
	constexpr uint32 kMCRF = ChunkId("MCRF");
	constexpr uint32 kMCAL = ChunkId("MCAL");
	constexpr uint32 kMCSH = ChunkId("MCSH");
	constexpr uint32 kMCSE = ChunkId("MCSE");
	constexpr uint32 kMCLQ = ChunkId("MCLQ");
	constexpr uint32 kMCCV = ChunkId("MCCV");

	// Four characters plus a terminator, for messages.
	std::string ChunkName(uint32 id);

	// Bytes MCNR occupies beyond what its size field claims.
	constexpr uint32 kMcnrPadding = 13;

	// MCVT holds a 9x9 outer grid interleaved with an 8x8 inner one, all heights relative to the
	// chunk header's position.z.
	constexpr int32 kMcvtHeights = 9 * 9 + 8 * 8;

	// A sub-chunk's on-disk size field is frequently a lie, so `data` is what it really occupies
	// (worked out from where the next one starts) and `declared` is the untouched value the file
	// stores. `originalSize` lets the writer spot a sub-chunk whose length changed, the only case
	// where recomputing a size field is safe.
	struct SubChunk
	{
		uint32 id = 0;
		std::vector<uint8> data;
		uint32 declared = 0;
		uint32 originalSize = 0;

		// Deliberately about length, not content. An edit that keeps the length, sculpting MCVT
		// heights for instance, wants the original size fields left exactly alone.
		bool Resized() const
		{
			return data.size() != originalSize;
		}

		// A sub-chunk that kept its length writes back exactly what the file said, however wrong
		// that is. Only a resized one gets a freshly computed value, and MCNR still hides its 13
		// trailing bytes from the count because the client expects to find them there.
		uint32 DeclaredSize() const
		{
			if (!Resized())
				return declared;

			uint32 size = static_cast<uint32>(data.size());
			if (id == kMCNR && size >= kMcnrPadding)
				size -= kMcnrPadding;

			return size;
		}
	};

	// One terrain chunk: the fixed 128 byte header followed by its sub-chunks in file order.
	struct Mcnk
	{
		SMChunk header{};
		std::vector<SubChunk> subs;

		SubChunk const* Find(uint32 id) const;
		SubChunk* Find(uint32 id);
	};

	// A top level entry. MCNK bodies live in AdtDocument::chunks, everything else is kept as
	// opaque bytes so unknown chunks survive a round-trip untouched.
	struct TopChunk
	{
		uint32 id = 0;
		std::vector<uint8> data;
		int32 mcnkIndex = -1;
	};

	struct AdtDocument
	{
		// Top level chunks in the order the file had them, which the writer reproduces exactly.
		std::vector<TopChunk> order;
		std::vector<Mcnk> chunks;

		// MHDR data. Every offset field is recomputed on write, the rest is carried through.
		SMMapHeader header{};
		bool hasHeader = false;

		// MCIN flags and asyncId per entry, carried through rather than assumed zero.
		SMChunkInfo info[256]{};
		bool hasInfo = false;

		// Whether SMChunkInfo::size counts the MCNK's 8 byte IFF header. Most tiles say yes and
		// three Azeroth tiles say no, so the reader measures which one this file uses and a rewrite
		// keeps it.
		bool mcinSizeIncludesHeader = true;

		Mcnk* ChunkAt(int32 chunkX, int32 chunkY);
		Mcnk const* ChunkAt(int32 chunkX, int32 chunkY) const;
	};

	// Reads the whole file through SFile, so loose and MPQ resolution match what the client
	// itself would pick. Returns false and fills error on failure.
	bool ReadFileBytes(char const* path, std::vector<uint8>& out, std::string& error);

	bool Read(uint8 const* bytes, uint32 size, AdtDocument& out, std::string& error);
	bool Write(AdtDocument const& doc, std::vector<uint8>& out, std::string& error);

	// Builds "World\Maps\<map>\<map>_<tileX>_<tileY>.adt". The single argument form uses the
	// client's current map name, the other takes any map so a tile can be read without standing
	// on it.
	std::string TilePath(int32 tileX, int32 tileY);
	std::string TilePath(char const* mapName, int32 tileX, int32 tileY);

	// Human readable location of a byte offset within a parsed file, for diff reporting.
	std::string DescribeOffset(AdtDocument const& doc, uint32 offset);

	// Which MCNK a byte offset falls in, or -1 if it lands in a top level chunk.
	int32 ChunkIndexAtOffset(AdtDocument const& doc, uint32 offset);
}
