// Native modern-M2 (MD21) reader plus the M2 render/compat pipeline it needs, ported from
// WarcraftXL (wxl-core's CGxDevice/CM2Shared client features + the wxl-modern-m2 extension) into
// one in-process subsystem. Every detour goes through ClientDetours, so Main::OnAttach must call
// Apply() before ClientDetours::Apply().
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

#include "Models/ModernM2.h"

#include "Models/Common/ModelHooks.h"
#include "Models/Client/M2Arena.h"
#include "Models/Client/WideIndices.h"

#include <windows.h>

namespace ModernM2
{
	bool InstallBlendStates();      // Client/BlendStates.cpp
	bool InstallM2Draw();           // Render/M2Draw.cpp
	bool InstallM2Memory();         // Load/M2Memory.cpp
	bool InstallM2CompatBones();    // Render/BonePalette.cpp
	bool InstallEmitterBlend();     // Compat/EmitterBlend.cpp
	bool InstallM2SceneHitTestSort(); // Render/HitTestSort.cpp
	bool InstallM2SetupBatchAlpha();  // Render/SetupMaterial.cpp
	bool InstallCombinerPatch();    // Render/CombinerPatch.cpp
	bool InstallAnimUnwrap();       // Load/AnimUnwrap.cpp
	bool InstallM2CompatLoader();   // Load/CompatLoader.cpp
	bool InstallM2Native();         // Load/NativeLoad.cpp
	bool InstallM2LodVariant();     // Load/LodModelVariant.cpp

	bool ConfigRaw(const char* name, char* buf, size_t cap)
	{
		if (!name || !buf || cap == 0)
			return false;
		const DWORD written = GetEnvironmentVariableA(name, buf, static_cast<DWORD>(cap));
		return written != 0 && written < cap;
	}

	void Apply()
	{
		// The arena wants its contiguous 32-bit reservation before world loading fragments the
		// address space, so it is taken here rather than at the first large allocation.
		Arena::Reserve();
		InstallBlendStates();

		// M2Draw owns the M2 batch-draw detour and the DrawIndexedPrimitive filter, and the wide-index
		// draw scope rides both, so it installs before anything else that touches the draw path.
		InstallM2Draw();
		WideIndices::Install();

		if constexpr (kEnabled)
		{
			InstallM2Memory();
			InstallM2CompatBones();
			InstallEmitterBlend();
			InstallM2SceneHitTestSort();
			InstallM2SetupBatchAlpha();
			InstallCombinerPatch();
			InstallAnimUnwrap();
			InstallM2CompatLoader();
			InstallM2Native();
			InstallM2LodVariant();
		}
	}
}
