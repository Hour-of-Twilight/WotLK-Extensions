#pragma once

#include <Defines.h>

#include <Helpers/Util.h>

#include <cstdint>
#include <string>
#include <vector>

namespace MapEditor::Adt
{
	// Where an edited tile lands: the loose tree under the exe directory, which the client
	// already prefers over the MPQs for per-tile overrides.
	fs::path DiskTilePath(char const* mapName, int32 tileX, int32 tileY);

	// Where the pre-edit copy of that tile is kept.
	fs::path BackupTilePath(char const* mapName, int32 tileX, int32 tileY);

	// Writes bytes over the loose copy of a tile, taking a backup first if none exists yet. The
	// backup comes from whatever SFile resolves, and a save is refused rather than performed if
	// that read fails.
	bool SaveTile(char const* mapName, int32 tileX, int32 tileY,
	    std::vector<uint8> const& bytes, std::string& error);

	// Puts the backup back over the loose copy. Leaves the backup in place, so restoring twice is
	// harmless and a later save still has its original to fall back on.
	bool RestoreTile(char const* mapName, int32 tileX, int32 tileY, std::string& error);
}
