#pragma once

#include <Defines.h>

#include <Macros.h>

#include <cstddef>
#include <cstdint>

namespace ClientData::M2Model
{
	// (firstId, lastId, visible) over the model's geosets. With kFlagLoaded clear it queues a heap
	// record instead of writing anything, so callers that run every frame have to check first.
	CLIENT_FUNCTION(SetGeometryVisible, 0x0082C7C0, __thiscall, void,
	    (void* self, uint32 firstId, uint32 lastId, int visible))

	CLIENT_FUNCTION(IsLoadedNative, 0x00824F00, __thiscall, int, (void* model, int a2, int a3))
	CLIENT_FUNCTION(HasAttachment, 0x008273D0, __thiscall, bool, (void* model, unsigned int attachment))
	CLIENT_FUNCTION(DetachFromParent, 0x008274F0, __thiscall, void, (void* model))
	CLIENT_FUNCTION(AttachToParent, 0x00831630, __thiscall, int, (void* model, void* parent, unsigned int attachment, float* transform, int flags))
	CLIENT_FUNCTION(Release, 0x00824ED0, __thiscall, int, (void* model))
	CLIENT_FUNCTION(SetAnimating, 0x00823F10, __thiscall, void, (void* model, int animating))
	CLIENT_FUNCTION(ReplaceTexture, 0x00825260, __thiscall, void, (void* model, int textureType, void* texture))

	enum Attachment : int
	{
		ATTACHMENT_SHIELD = 0,
		ATTACHMENT_HAND_RIGHT = 1,
		ATTACHMENT_HAND_LEFT = 2,
		ATTACHMENT_HELM = 11,
	};

	constexpr uint32 kFlagsOffset = 0x10;
	constexpr uint32 kFlagLoaded = 0x1;
	constexpr uint32 kSharedOffset = 0x2C;
	constexpr uint32 kAttachParentOffset = 0x48;
	constexpr uint32 kAttachmentIdOffset = 0x50;
	constexpr uint32 kFirstChildOffset = 0x58;
	constexpr uint32 kNextSiblingOffset = 0x60;
	constexpr uint32 kPlacementOffset = 0xB4;

	constexpr uint32 kSharedFilePathOffset = 0x3C;
	constexpr uint32 kSharedDataOffset = 0x150;

	constexpr uint32 kDataAttachmentsOffset = 0xF0;
	constexpr uint32 kDataAttachmentLookupOffset = 0xF8;
	constexpr size_t kAttachmentStride = 0x28;
	constexpr uint32 kAttachmentPositionOffset = 0x08;
	constexpr uint16 kNoAttachmentIndex = 0xFFFF;
	constexpr int kBaseSkinTextureSlot = 1;
	constexpr uint32 kFlagDrawRoot = 0x8;
	constexpr uint32 kFlagEventsRoot = 0x10000;
	constexpr uint32 kFlagDrawChild = 0x80;
	constexpr uint32 kFlagEventsChild = 0x20000;

	inline bool IsLoaded(void* model)
	{
		return model && (*(reinterpret_cast<uint8*>(model) + kFlagsOffset) & kFlagLoaded) != 0;
	}

	inline void* AttachParent(void* model)
	{
		return model ? *reinterpret_cast<void**>(reinterpret_cast<uint8*>(model) + kAttachParentOffset) : nullptr;
	}

	inline uint32 AttachmentId(const void* model)
	{
		return model ? *reinterpret_cast<const uint32*>(static_cast<const uint8*>(model) + kAttachmentIdOffset) : 0;
	}

	inline void* FirstChild(const void* model)
	{
		return model ? *reinterpret_cast<void* const*>(static_cast<const uint8*>(model) + kFirstChildOffset) : nullptr;
	}

	inline void* NextSibling(const void* model)
	{
		return model ? *reinterpret_cast<void* const*>(static_cast<const uint8*>(model) + kNextSiblingOffset) : nullptr;
	}

	inline float* Placement(void* model)
	{
		return model ? reinterpret_cast<float*>(static_cast<uint8*>(model) + kPlacementOffset) : nullptr;
	}

	inline const float* Placement(const void* model)
	{
		return model ? reinterpret_cast<const float*>(static_cast<const uint8*>(model) + kPlacementOffset) : nullptr;
	}

	inline void* Shared(const void* model)
	{
		return model ? *reinterpret_cast<void* const*>(static_cast<const uint8*>(model) + kSharedOffset) : nullptr;
	}

	inline const char* SharedFilePath(const void* model)
	{
		const void* shared = Shared(model);
		return shared ? reinterpret_cast<const char*>(static_cast<const uint8*>(shared) + kSharedFilePathOffset) : nullptr;
	}

	inline uintptr_t SharedData(const void* model)
	{
		const void* shared = Shared(model);
		return shared ? *reinterpret_cast<const uintptr_t*>(static_cast<const uint8*>(shared) + kSharedDataOffset) : 0;
	}
}
