#pragma once

#include <ClientData/MathTypes.h>

#include <cstdint>
#include <string>
#include <vector>

// Adding and removing texture layers, which is the structural half of texture editing, since the
// paint brush only moves coverage between layers a chunk already has. Both work on every chunk the
// brush circle reaches rather than the one under the cursor, because a layer is only useful once it
// is on all the chunks you paint across.
namespace MapEditor::TextureLayers
{
	using namespace ClientData;

	// The client's CMapRenderChunk keeps its layers in a fixed array of four at renderChunk+0x34,
	// with the shader and shadow textures immediately after at +0x84. A fifth layer would write
	// straight over those, so this is a hard limit rather than a convention.
	constexpr int32_t kMaxLayers = 4;

	// Puts a texture down as a new top layer, covering nothing until it is painted, and returns how
	// many chunks gained it. Chunks already at four layers, or already carrying this texture, are
	// left alone and counted in `skipped`.
	int32_t Add(C3Vector const& center, float radius, char const* texture, int32_t& skipped,
	    std::string& error);

	// Drops a layer, handing its coverage back to whatever is underneath. Layer 0 cannot go,
	// since the base is what shows through everything else.
	int32_t Remove(C3Vector const& center, float radius, int32_t layer, int32_t& skipped,
	    std::string& error);

	// Every texture the hovered tile can offer a layer, which is its MTEX list.
	bool ListTileTextures(int32_t tileX, int32_t tileY, std::vector<std::string>& out,
	    std::string& error);

	// Forgets the texture names and MTXF buffers the editor owns. Same rule as the brush: only
	// once the tiles pointing at them have gone.
	void Reset();
}
