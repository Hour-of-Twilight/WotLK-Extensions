// Interface to the dedicated M2 buffer arena. Implementation in M2Arena.cpp and
// Models/Load/M2Memory.cpp, both ported from WarcraftXL (src/client/CM2Shared/Memory.cpp).
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

#include <Defines.h>

#include <cstdint>

namespace ModernM2::Arena
{
	void Reserve();
	void* Alloc(uint32 size, uint32* outOffset, uint32* outSize);
	void Free(uint32 offset, uint32 size);
	void LogAddressSpace(const char* reason);
}
