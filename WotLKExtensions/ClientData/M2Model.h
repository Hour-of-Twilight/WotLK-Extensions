#pragma once

#include <Macros.h>

#include <cstdint>

namespace ClientData::M2Model
{
	// (firstId, lastId, visible) over the model's geosets. With kFlagLoaded clear it queues a heap
	// record instead of writing anything, so callers that run every frame have to check first.
	CLIENT_FUNCTION(SetGeometryVisible, 0x0082C7C0, __thiscall, void,
	    (void* self, uint32_t firstId, uint32_t lastId, int visible))

	constexpr uint32_t kFlagsOffset = 0x10;
	constexpr uint32_t kFlagLoaded = 0x1;

	inline bool IsLoaded(void* model)
	{
		return model && (*(reinterpret_cast<uint8_t*>(model) + kFlagsOffset) & kFlagLoaded) != 0;
	}
}
