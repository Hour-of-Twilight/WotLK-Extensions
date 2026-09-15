// Ported from WarcraftXL (src/engine/assets/shared/models/m2/Ribbons.cpp).
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

#include "Models/Compat/Ribbons.h"

namespace ModernM2::Live::Ribbons
{
	// The engine draws an N-texture ribbon as N sequential single-texture passes, which cannot
	// reproduce a source ribbon's texture product; the caller applies the combine for >= 3 layers.
	void OnRibbonDraw(uint32_t /*layerCount*/, bool* useMultiTexture)
	{
		if (useMultiTexture)
			*useMultiTexture = true;
	}
}
