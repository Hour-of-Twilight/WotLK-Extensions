#pragma once

#include <Editor/Map/Adt/AdtDocument.h>

#include <cstdint>
#include <string>
#include <vector>

namespace MapEditor::Session
{
	// A tile open for editing. The document is what eventually gets written, the client's live
	// buffer is what the player sees, and an edit has to land in both: the document because the
	// live buffer is not a faithful copy of the file (the client rewrites MCNR/MCAL/MCLQ size
	// fields and an MCIN flag as it parses), the live buffer because otherwise nothing moves
	// until a reload.
	struct OpenTile
	{
		int32_t tileX = -1;
		int32_t tileY = -1;
		Adt::AdtDocument doc;
		bool dirty = false;
	};

	// Opens a tile for editing, reading it from disk on first use, and returns null with error
	// filled in if it cannot be read or parsed. Tiles stay open until saved or dropped.
	OpenTile* Open(int32_t tileX, int32_t tileY, std::string& error);

	// The open tile, or null if it was never opened. Does not read from disk.
	OpenTile* Find(int32_t tileX, int32_t tileY);

	// Every open tile, for a pass that has to look at all of them at once.
	std::vector<OpenTile*> AllOpen();

	int32_t DirtyCount();

	// Serializes and writes every dirty tile. Returns how many were written, and stops at the
	// first failure so a partial save is visible rather than silent.
	bool SaveDirty(int32_t& saved, std::string& error);

	// One tile, whether or not it is dirty. Adding a placement needs this: the only way the client
	// will build an instance for a brand new entry is to read the tile back off disk.
	bool SaveOne(int32_t tileX, int32_t tileY, std::string& error);

	// Restores every open tile from its backup and reloads it, throwing away the documents.
	int32_t RevertAll();

	// Drops the open documents without writing. The live buffers keep whatever was painted into
	// them until the tiles reload.
	void CloseAll();

	// Which map the open tiles belong to. Changing map drops them, since tile coordinates mean
	// something different there.
	char const* MapName();
	void OnMapChanged();
}
