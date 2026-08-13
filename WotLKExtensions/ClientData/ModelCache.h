#pragma once

#include <Macros.h>

#include <cstdint>

namespace ClientData::ModelCache
{
	// One CM2Cache for the whole client. Every CM2Scene, world and UI alike, is built with a
	// pointer to it, so this covers model frames as well as the world.
	CLIENT_ADDRESS(uint8_t, s_m2Cache, 0x00D3FCF0)

	// force = 0 drops entries idle for 10s (what CM2Scene::AdvanceTime does each frame),
	// force = 1 drops every unreferenced entry. Anything still attached to a model stays.
	CLIENT_FUNCTION(GarbageCollect, 0x0081C290, __thiscall, void, (void* self, int force))

	// CM2Cache: 0x3FD inline hash buckets at +0x10, chained through CM2Shared.
	constexpr uint32_t kBuckets = 0x3FD;
	constexpr uint32_t kBucketsOffset = 0x10;

	// CM2Shared: +0x144 points at whichever slot points back at us, +0x148 is the next entry.
	constexpr uint32_t kSharedPrevSlot = 0x144;
	constexpr uint32_t kSharedNext = 0x148;

	// Unlink every parsed model from the name hash. Live models keep working off their own
	// pointer, but nothing can look them up again, so the next load re-reads the file. A shared
	// with no hash link is deleted outright by CM2Shared::Release instead of going on the idle
	// list, so this does not leak. Call after mounting an archive that replaces art.
	inline void Invalidate()
	{
		uint8_t** buckets = reinterpret_cast<uint8_t**>(s_m2Cache + kBucketsOffset);
		for (uint32_t i = 0; i < kBuckets; ++i)
		{
			for (uint8_t* shared = buckets[i]; shared;)
			{
				uint8_t* next = *reinterpret_cast<uint8_t**>(shared + kSharedNext);
				*reinterpret_cast<uint8_t**>(shared + kSharedPrevSlot) = nullptr;
				*reinterpret_cast<uint8_t**>(shared + kSharedNext) = nullptr;
				shared = next;
			}
			buckets[i] = nullptr;
		}
	}

	inline void Purge()
	{
		GarbageCollect(s_m2Cache, 1);
	}
}
