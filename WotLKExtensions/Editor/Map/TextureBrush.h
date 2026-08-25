#pragma once

#include <ClientData/MathTypes.h>
#include <Editor/Map/Adt/AdtDocument.h>
#include <Editor/Map/MapClient.h>
#include <Editor/Map/Sculpt.h>

#include <cstdint>

namespace MapEditor::TextureBrush
{
	using namespace ClientData;

	struct Stroke
	{
		C3Vector center{};
		float radius = 10.0f;
		Sculpt::Falloff falloff = Sculpt::Falloff::Smooth;

		// Which of the chunk's existing layers to push toward, 0 being the base. A chunk with
		// fewer layers than this is left alone rather than grown, since growing one is a
		// structural edit and lives in TextureLayers.
		int32_t layer = 1;

		// How far a texel under the full weight of the brush closes on its target this step,
		// 0 to 1. Negative takes the layer away instead of laying it down.
		float blend = 0.0f;
	};

	// Paints one step of a stroke. Returns how many texels changed.
	int32_t Apply(Stroke const& stroke);

	// Copies a chunk's layers and blend masks out of the document into the client's live copy,
	// rebuilds its layer textures and forgets whatever coverage the brush had cached for it. Adding
	// and removing layers comes through here too, so a structural edit cannot leave the brush
	// painting the old layer list.
	void PushChunk(int32_t tileX, int32_t tileY, int32_t chunkX, int32_t chunkY, CMapChunk* chunk,
	    Adt::Mcnk const& mcnk);

	// Forgets the MCLY and MCAL buffers the editor owns, along with the cached coverage. Only
	// safe once the tiles pointing at them are gone, so call it after a reload, never before.
	void Reset();
}
