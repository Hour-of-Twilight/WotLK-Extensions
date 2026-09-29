#pragma once

#include <Macros.h>

#include <cstdint>

namespace ClientData::M2Model
{
	// (firstId, lastId, visible) over the model's geosets. With kFlagLoaded clear it queues a heap
	// record instead of writing anything, so callers that run every frame have to check first.
	CLIENT_FUNCTION(SetGeometryVisible, 0x0082C7C0, __thiscall, void,
	    (void* self, uint32_t firstId, uint32_t lastId, int visible))

	CLIENT_FUNCTION(IsLoadedNative, 0x00824F00, __thiscall, int, (void* model, int a2, int a3))
	CLIENT_FUNCTION(HasAttachment, 0x008273D0, __thiscall, bool, (void* model, unsigned int attachment))
	CLIENT_FUNCTION(DetachFromParent, 0x008274F0, __thiscall, void, (void* model))
	CLIENT_FUNCTION(AttachToParent, 0x00831630, __thiscall, int, (void* model, void* parent, unsigned int attachment, float* transform, int flags))
	CLIENT_FUNCTION(Release, 0x00824ED0, __thiscall, int, (void* model))

	constexpr uint32_t kFlagsOffset = 0x10;
	constexpr uint32_t kFlagLoaded = 0x1;

	inline bool IsLoaded(void* model)
	{
		return model && (*(reinterpret_cast<uint8_t*>(model) + kFlagsOffset) & kFlagLoaded) != 0;
	}
}
