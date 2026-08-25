#include <Editor/Map/BrushDecal.h>

#include <ClientData/SharedDefines.h>
#include <ClientData/WorldFrame.h>

#include <cstdint>

namespace MapEditor::BrushDecal
{
	using namespace ClientData;

	void Show(C3Vector const& center, float radius, bool valid)
	{
		// A real cast owns the decal, so yield rather than fight OnLayerTrackTerrain.
		if (Spell_C::IsTargeting())
			return;

		*TerrainDecal::sPosition = center;
		*TerrainDecal::sRadius = radius > 0.0f ? radius : 1.0f;
		*TerrainDecal::sMode = valid ? TerrainDecal::kModeAcceptable : TerrainDecal::kModeUnacceptable;
	}

	void Hide()
	{
		if (Spell_C::IsTargeting())
			return;

		*TerrainDecal::sMode = TerrainDecal::kModeHidden;
	}
}
