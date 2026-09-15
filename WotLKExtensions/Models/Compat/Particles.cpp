// Ported from WarcraftXL (src/engine/assets/shared/models/m2/Particles.cpp).
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

#include "Models/Compat/Particles.h"

#include "Models/Client/M2Bindings.h"

namespace ModernM2::Live::Particles
{
	namespace
	{
		// Where source leaf / foliage coverage alpha sits; blend mode 1 (alpha key) is the only mode
		// the lowered reference applies to.
		constexpr float kSourceAlphaKeyRef = 0.5f;
	}

	void OnSetupBatchAlpha(uint16_t blendMode, bool downported)
	{
		if (downported && blendMode == kBlendAlphaKey)
			ModernM2::Client::PushAlphaRef(kSourceAlphaKeyRef);
	}
}
