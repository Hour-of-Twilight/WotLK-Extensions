// Interface to the 32-bit submesh index-start support. Implementation in WideIndices.cpp,
// ported from WarcraftXL (src/client/CM2Shared/WideIndices.cpp).
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

namespace ModernM2::WideIndices
{
	struct DrawBatchScope
	{
		const void* section;
		const void* skin;
	};

	bool Install();

	DrawBatchScope OnDrawBatchEnter(void* ctx);
	void OnDrawBatchLeave(const DrawBatchScope& saved);
	void RemapHitTestRange(uint16_t** indexBegin, uint16_t** indexEnd);
}
