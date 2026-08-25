#pragma once

#include <ClientData/MathTypes.h>
#include <Editor/Map/MapClient.h>

#include <cstdint>
#include <string>
#include <vector>

// Moving and deleting the M2 and WMO instances a tile already lists. Everything here treats the
// open AdtDocument as the truth for indices and transforms, and the client's live CMapDoodadDef /
// CMapObjDef purely as the thing to keep looking right on screen.
namespace MapEditor::Placements
{
	using namespace ClientData;

	enum class Kind
	{
		Doodad, // an MDDF entry, some M2
		MapObj, // a MODF entry, some WMO
	};

	// Where a placement sits in its tile's list. The index only means anything alongside the tile,
	// and only until something below it is deleted, so the uniqueId rides along and gets rechecked
	// before every edit.
	struct Ref
	{
		Kind kind = Kind::Doodad;
		int32_t tileX = -1;
		int32_t tileY = -1;
		int32_t index = -1;
		uint32_t uniqueId = 0;

		bool Valid() const { return index >= 0 && tileX >= 0 && tileY >= 0; }
	};

	// World position, Euler degrees on the MDDF/MODF axes, and a scale only doodads actually have.
	struct Transform
	{
		C3Vector position{};
		C3Vector rotation{};
		float scale = 1.0f;
	};

	struct Info
	{
		Ref ref;
		std::string name;
		Transform transform;
		CAaBox bounds{};            // world space, off the live instance
		float distance = 0.f;       // yards to the bounds, only filled in by ListNear
		float originDistance = 0.f; // yards to the placement's own origin, ListNear only
		bool live = false;          // whether the client currently has an instance for it
	};

	// Nearest placement the ray enters, tested against the live world bounds. Ours rather than the
	// client's: CMap::VectorIntersectDoodadDefs only flags candidates for a deferred scene query
	// and never reports a hit we could read back on the spot.
	bool Pick(C3Vector const& start, C3Vector const& end, Ref& out);

	bool Describe(Ref const& ref, Info& out);

	// Points a Ref back at the entry its uniqueId belongs to, since a delete shifts every index
	// above it and anything holding a Ref from before that is off by one. Needed by whatever hands
	// a Ref back in after a while, a list a panel scanned being the obvious one.
	bool Resolve(Ref& ref);

	// Position, rotation and scale in one call, because each of them rebuilds the same matrix. A
	// move that crosses a tile border migrates the entry to the new tile's MDDF or MODF keeping its
	// uniqueId, and the ref is rewritten to point at wherever it ended up.
	bool SetTransform(Ref& ref, Transform const& transform, std::string& error);

	// The live instance only, for showing a drag as it happens. Rewriting MCRF every frame would
	// mean 256 chunks of churn per mouse move, so the file side waits until the drag ends.
	bool Preview(Ref const& ref, Transform const& transform);

	// Drops the entry from MDDF or MODF, rewrites every chunk's MCRF around the hole, and shoves
	// the live instance under the world. Every index above this one shifts down by one, so any Ref
	// held elsewhere is stale afterwards.
	bool Remove(Ref const& ref, std::string& error);

	// Everything with a live instance whose bounds come within radius, nearest first. Measured to
	// the bounds rather than the origin, or a keep whose MODF origin sits hundreds of yards off
	// would sort behind the shrubs in front of it.
	int32_t ListNear(C3Vector const& center, float radius, std::vector<Info>& out);

	// Which list a file belongs in, from its extension. False for anything that is neither a model
	// nor a WMO, since guessing would put a .blp in MMDX and lose the tile.
	bool KindForPath(char const* path, Kind& out);

	// A brand new entry in whichever tile the position falls in, adding the model to that tile's
	// name list and minting a uniqueId nothing else in sight is using. Nothing appears on screen
	// until the tile is written out and reloaded, since the client's parsed arrays are fixed size.
	bool Add(Kind kind, char const* path, Transform const& transform, Ref& out, std::string& error);

	// The same model as an existing entry, at a new spot, with a new uniqueId. The path comes out
	// of the source tile's own name list, so this works on something the client cannot name, and a
	// WMO keeps the source's bounds rather than a guessed box.
	bool Clone(Ref const& ref, Transform const& transform, Ref& out, std::string& error);

	// Distinct model paths in use near a point, nearest first. There is no listfile to browse, so
	// what is already standing around you is the practical way to pick something to place.
	int32_t ListModels(C3Vector const& center, float radius, std::vector<std::string>& out);

	// Why a click picked what it picked: how many live defs of each kind the walk found, how many
	// matched back to a file entry, and the nearest few the ray crossed. Purely diagnostic, nothing
	// else calls it.
	void DebugRay(C3Vector const& start, C3Vector const& end, std::vector<std::string>& lines);
}
