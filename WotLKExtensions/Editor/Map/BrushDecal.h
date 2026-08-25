#pragma once

#include <ClientData/MathTypes.h>

// The brush footprint is the client's own AoE spell targeting decal, the projected circle it
// drapes over the terrain while a ground-targeted spell is being aimed. We only drive its
// globals, the client's existing draw calls do the rest, so it looks and projects exactly like
// the real thing.
namespace MapEditor::BrushDecal
{
	// Must be called before CGWorldFrame::OnWorldRender for the same frame.
	void Show(ClientData::C3Vector const& center, float radius, bool valid);
	void Hide();
}
