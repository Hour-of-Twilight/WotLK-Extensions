#pragma once

#include <Editor/Map/MapClient.h>

#include <cstdint>
#include <string>
#include <vector>

namespace MapEditor::Adt
{
	// Chunk magics sit reversed on disk, so reading the four bytes as a little endian uint32
	// gives back the natural spelling. 'M','V','E','R' on disk is "REVM", which reads as
	// 0x4D564552, exactly what ChunkId("MVER") builds.
	constexpr uint32_t ChunkId(char const (&text)[5])
	{
		return static_cast<uint32_t>(static_cast<uint8_t>(text[0])) << 24 | static_cast<uint32_t>(static_cast<uint8_t>(text[1])) << 16 | static_cast<uint32_t>(static_cast<uint8_t>(text[2])) << 8 | static_cast<uint32_t>(static_cast<uint8_t>(text[3]));
	}

	constexpr uint32_t kMVER = ChunkId("MVER");
	constexpr uint32_t kMHDR = ChunkId("MHDR");
	constexpr uint32_t kMCIN = ChunkId("MCIN");
	constexpr uint32_t kMTEX = ChunkId("MTEX");
	constexpr uint32_t kMMDX = ChunkId("MMDX");
	constexpr uint32_t kMMID = ChunkId("MMID");
	constexpr uint32_t kMWMO = ChunkId("MWMO");
	constexpr uint32_t kMWID = ChunkId("MWID");
	constexpr uint32_t kMDDF = ChunkId("MDDF");
	constexpr uint32_t kMODF = ChunkId("MODF");
	constexpr uint32_t kMFBO = ChunkId("MFBO");
	constexpr uint32_t kMH2O = ChunkId("MH2O");
	constexpr uint32_t kMTXF = ChunkId("MTXF");
	constexpr uint32_t kMCNK = ChunkId("MCNK");

	constexpr uint32_t kMCVT = ChunkId("MCVT");
	constexpr uint32_t kMCNR = ChunkId("MCNR");
	constexpr uint32_t kMCLY = ChunkId("MCLY");
	constexpr uint32_t kMCRF = ChunkId("MCRF");
	constexpr uint32_t kMCAL = ChunkId("MCAL");
	constexpr uint32_t kMCSH = ChunkId("MCSH");
	constexpr uint32_t kMCSE = ChunkId("MCSE");
	constexpr uint32_t kMCLQ = ChunkId("MCLQ");
	constexpr uint32_t kMCCV = ChunkId("MCCV");

	// Four characters plus a terminator, for messages.
	std::string ChunkName(uint32_t id);

	// Bytes MCNR occupies beyond what its size field claims.
	constexpr uint32_t kMcnrPadding = 13;

	// MCVT holds a 9x9 outer grid interleaved with an 8x8 inner one, all heights relative to the
	// chunk header's position.z.
	constexpr int32_t kMcvtHeights = 9 * 9 + 8 * 8;

	// A sub-chunk's on-disk size field is frequently a lie, so `data` is what it really occupies
	// (worked out from where the next one starts) and `declared` is the untouched value the file
	// stores. `originalSize` lets the writer spot a sub-chunk whose length changed, the only case
	// where recomputing a size field is safe.
	struct SubChunk
	{
		uint32_t id = 0;
		std::vector<uint8_t> data;
		uint32_t declared = 0;
		uint32_t originalSize = 0;

		// Deliberately about length, not content. An edit that keeps the length, sculpting MCVT
		// heights for instance, wants the original size fields left exactly alone.
		bool Resized() const
		{
			return data.size() != originalSize;
		}

		// A sub-chunk that kept its length writes back exactly what the file said, however wrong
		// that is. Only a resized one gets a freshly computed value, and MCNR still hides its 13
		// trailing bytes from the count because the client expects to find them there.
		uint32_t DeclaredSize() const
		{
			if (!Resized())
				return declared;

			uint32_t size = static_cast<uint32_t>(data.size());
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

		SubChunk const* Find(uint32_t id) const;
		SubChunk* Find(uint32_t id);
	};

	// A top level entry. MCNK bodies live in AdtDocument::chunks, everything else is kept as
	// opaque bytes so unknown chunks survive a round-trip untouched.
	struct TopChunk
	{
		uint32_t id = 0;
		std::vector<uint8_t> data;
		int32_t mcnkIndex = -1;
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

		Mcnk* ChunkAt(int32_t chunkX, int32_t chunkY);
		Mcnk const* ChunkAt(int32_t chunkX, int32_t chunkY) const;
	};

	// Reads the whole file through SFile, so loose and MPQ resolution match what the client
	// itself would pick. Returns false and fills error on failure.
	bool ReadFileBytes(char const* path, std::vector<uint8_t>& out, std::string& error);

	bool Read(uint8_t const* bytes, uint32_t size, AdtDocument& out, std::string& error);
	bool Write(AdtDocument const& doc, std::vector<uint8_t>& out, std::string& error);

	// Builds "World\Maps\<map>\<map>_<tileX>_<tileY>.adt". The single argument form uses the
	// client's current map name, the other takes any map so a tile can be read without standing
	// on it.
	std::string TilePath(int32_t tileX, int32_t tileY);
	std::string TilePath(char const* mapName, int32_t tileX, int32_t tileY);

	// Human readable location of a byte offset within a parsed file, for diff reporting.
	std::string DescribeOffset(AdtDocument const& doc, uint32_t offset);

	// Which MCNK a byte offset falls in, or -1 if it lands in a top level chunk.
	int32_t ChunkIndexAtOffset(AdtDocument const& doc, uint32_t offset);
}
