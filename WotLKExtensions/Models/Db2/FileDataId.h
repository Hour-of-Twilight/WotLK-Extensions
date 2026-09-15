// FileDataID -> client path, backed by TextureFilePath.db2 / ModelFilePath.db2.
// Ported from WarcraftXL (scripts/wxl-db2/src/api/FdidResolver.*), trimmed to the two path tables
// the modern-M2 reader needs.
// Copyright (C) 2026 WarcraftXL
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <cstdint>

namespace ModernM2::Fdid
{
	// Loads the texture path table. Lazy, and safe to call while BeginWarm's thread is mid-load
	// (the caller waits for it).
	void EnsureLoaded();

	// Starts EnsureLoaded on a worker thread. Only call it where the archive set is known mounted.
	void BeginWarm();

	// FileDataID -> file path, cached (a node's c_str() is stable for the process once resolved).
	// ResolveTexture reads the texture table alone; ResolveModel pulls in the model table and falls
	// back to the texture one, since the two share a FileDataID space.
	const char* ResolveTexture(uint32_t fileDataId);
	const char* ResolveModel(uint32_t fileDataId);

	// Model path -> its FileDataID, or 0. Case- and separator-insensitive, extension ignored.
	// The first call builds a stem index over the whole model table, so it is not a cheap one.
	uint32_t ResolveModelId(const char* modelPath);
}
