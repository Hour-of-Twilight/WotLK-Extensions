// Ported from WarcraftXL (scripts/modern-m2/src/compat/ModernM2.cpp).
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

#include "Models/Compat/LiveM2.h"

#include "Models/Compat/AssetRegistry.h"
#include "Models/Compat/BoneBudget.h"
#include "Models/Compat/Skin.h"
#include "Models/Compat/Particles.h"
#include "Models/Compat/Ribbons.h"
#include "Models/Common/ModelHooks.h"
#include "Models/Client/M2Bindings.h"
#include "Models/Format/M2Format.h"
#include "Models/Format/M2Contract.h"

#include <windows.h>

#include <vector>

namespace ModernM2::Live
{
	namespace m2 = ModernM2::Client;
	namespace fmt = ModernM2::Format;
	namespace bn = ModernM2::Bones;

	namespace
	{
		Common::AssetRegistry g_registry;
	}

	void OnModelLoadPre(void* model)
	{
		g_registry.Forget(model);
	}

	/**
	 * @brief Splits any over-budget submesh, then rebuilds the material / texunit contract for the
	 *        models the native MD21 reader filled.
	 *
	 * The bone-budget split (BoneBudget.h) is a hard client-engine constraint, not a format concern, so
	 * it runs for every model whatever its origin. The shaderId decode + textureUnitLookup synth after it
	 * is scoped to registered models only, because it assumes the modern packed shaderId encoding that
	 * the native reader leaves on the live skin -- a stock v264 model already carries a resolved contract
	 * and only needs the structural repoint when a split happened.
	 */
	void OnSkinFinalize(void* model)
	{
		m2::M2Model wrapper(model);
		auto* md = wrapper.GetHeader();
		auto* sk = wrapper.GetSkin();
		if (!md || !sk)
			return;

		std::vector<bn::SplitSection> sections;
		std::vector<bn::SplitRun> splitMap;
		uint32_t splitCount = 0;
		const char* pathStem = wrapper.GetPathStem();
		const bool split = bn::SplitSubmeshes(md, sk, sections, splitMap, splitCount,
		                       pathStem ? pathStem : "") &&
		    splitCount > 0;
		if (split)
			WLOG_INFO("modern-assets: bone-splitter produced %u extra sub-draw(s)", splitCount);

		if (g_registry.Contains(model))
			Skin::Rebuild(md, sk, splitMap, pathStem ? pathStem : "");
		else if (split)
			bn::RepointBatchesAfterSplit(sk, splitMap);

		if (const uint32_t clamped = bn::ClampBoneBudget(md, sk))
			WLOG_WARN("modern-assets: '%s' had %u submesh(es) over the %u-bone draw budget, clamped",
			    pathStem ? pathStem : "(no stem)", clamped, bn::kMaxBonesPerDraw);
	}

	bool IsNativeLoaded(void* model)
	{
		return g_registry.Contains(model);
	}

	void OnSetupBatchAlpha(void* model, uint16_t blendMode)
	{
		// Alpha-key batches are a small minority of the scene; test the blend mode before paying
		// the registry lookup, which otherwise costs a shared-lock + hash find on EVERY batch of
		// every visible model.
		if (blendMode != Particles::kBlendAlphaKey)
			return;
		Particles::OnSetupBatchAlpha(blendMode, g_registry.Contains(model));
	}

	void OnRibbonDraw(uint32_t layerCount, bool* useMultiTexture)
	{
		Ribbons::OnRibbonDraw(layerCount, useMultiTexture);
	}

	void RegisterNativeLoaded(void* model)
	{
		if constexpr (ModernM2::kEnabled)
			g_registry.Remember(model, Common::AssetRegistry::kFlagHotReshaped);
		else
			(void)model;
	}

	void ForgetNativeLoaded(void* model)
	{
		if constexpr (ModernM2::kEnabled)
			g_registry.Forget(model);
		else
			(void)model;
	}
}
