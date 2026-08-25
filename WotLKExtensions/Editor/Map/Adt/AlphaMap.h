#pragma once

#include <Editor/Map/Adt/AdtDocument.h>
#include <Editor/Map/MapClient.h>

#include <cstdint>
#include <string>
#include <vector>

namespace MapEditor::Adt
{
	constexpr int32_t kAlphaSide = 64;
	constexpr int32_t kAlphaTexels = kAlphaSide * kAlphaSide;

	// MCLY::flags. Only these two matter for painting, the rest drive texture animation.
	constexpr uint32_t kLayerUseAlpha = 0x100;   // set on every layer except the base one
	constexpr uint32_t kLayerCompressed = 0x200; // RLE, and only legal alongside big alpha

	// SMChunk::flags. When this is clear the client rebuilds row and column 63 out of 62 as it
	// unpacks, so whatever the file stores there never reaches the screen.
	constexpr uint32_t kChunkDoNotFixAlpha = 0x8000;

	// How a chunk's MCAL block is stored. bigAlpha is the WDT's MPHD flag 0x4 and doNotFix is the
	// chunk header's own 0x8000, read by CMapRenderChunk::CreateLayerTexture (0x007B9890) and
	// UnpackAlphaBits (0x007B8E20) respectively.
	//
	// With doNotFix clear the client only accepts the 4 bit format, so big alpha in practice
	// implies doNotFix.
	struct AlphaFormat
	{
		bool bigAlpha = false;
		bool doNotFix = false;
	};

	// One layer's blend mask, 64x64, always widened to 8 bits whatever the file stored.
	struct AlphaMap
	{
		uint8_t texel[kAlphaTexels] = {};
	};

	// A chunk's texture layers, decoded into something paintable.
	struct ChunkAlpha
	{
		std::vector<SMLayer> layers;

		// One per layer, so the indices line up with MCLY. Index 0 stays zero because the base
		// layer has no mask, it is simply whatever shows through the others.
		std::vector<AlphaMap> maps;
	};

	bool ReadChunkAlpha(Mcnk const& mcnk, AlphaFormat const& format, ChunkAlpha& out,
	    std::string& error);

	// Re-encodes MCLY and MCAL from the decoded layers, resizing MCAL and inserting it when the
	// chunk had none. Always writes uncompressed: every client reads it, Noggit made the same
	// call after finding compression broke textures in game, and it keeps a painted chunk's MCAL
	// length stable so later strokes can patch the live buffer in place.
	bool WriteChunkAlpha(ChunkAlpha const& in, AlphaFormat const& format, Mcnk& mcnk,
	    std::string& error);

	// Bytes one layer's mask occupies once written.
	constexpr uint32_t AlphaStride(bool bigAlpha)
	{
		return bigAlpha ? 4096u : 2048u;
	}

	// Reads a map's MPHD flags out of its WDT. The client keeps the flags of the map it is
	// standing in at Access::sMapFlags, but a sweep over some other map has to go to that map's
	// own file or it would judge every tile by the wrong format.
	bool ReadMapFlags(char const* mapName, uint32_t& flags, std::string& error);

	// The format for one chunk, given the map flags its tile belongs to.
	AlphaFormat FormatFor(Mcnk const& mcnk, uint32_t mapFlags);
}
