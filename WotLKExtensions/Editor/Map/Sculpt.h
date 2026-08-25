#pragma once

#include <ClientData/MathTypes.h>

#include <cstdint>

namespace MapEditor::Sculpt
{
	using namespace ClientData;

	enum class Falloff
	{
		Smooth,  // smoothstep, soft edge and a flat-ish centre
		Linear,  // straight cone
		Flat,    // no falloff at all, a cylinder
	};

	enum class Mode
	{
		Raise,    // signed height offset, negative to sink
		Flatten,  // converge on targetHeight
		Smooth,   // converge on the local average
		Noise,    // per-vertex jitter, keyed on position so it is stable and seam-safe
	};

	struct Stroke
	{
		C3Vector center{};
		float radius = 10.0f;
		Falloff falloff = Falloff::Smooth;
		Mode mode = Mode::Raise;

		// Yards, signed. Drives Raise and Noise, which move by a step.
		float amount = 0.0f;

		// How much of the remaining distance to close, 0 to 1. Drives Flatten and Smooth, which
		// converge on a target instead of stepping.
		float blend = 0.0f;

		// What Flatten pulls towards, in world z. Sampled once when the stroke starts, so the
		// plane does not creep upward as the cursor drifts over ground it already levelled.
		float targetHeight = 0.0f;
	};

	// Applies a stroke to every loaded chunk the brush touches, across chunk and tile borders, and
	// returns how many vertices moved. Everything is decided from world position rather than vertex
	// index, so each chunk holding a copy of a shared vertex gets the same answer and the seam
	// closes on its own.
	//
	// Tiles that are not resident are skipped, since the fast path needs the client's live buffer.
	int32_t Apply(Stroke const& stroke);
}
