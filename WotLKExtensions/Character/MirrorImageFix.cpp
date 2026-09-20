#include <ClientDetours.h>
#include <Macros.h>
#include <cstdint>

// Stock client bug: SMSG_MIRRORIMAGE_DATA leaks the unit's character component.
//
// CGUnit_C::OnMirrorImageData (0x00730290) allocates a new character component and stores it
// at unit+0xB4C without freeing the one already there. The stock flow expects the component to
// have been freed first: whenever the unit's model reloads (0x00730100) the component is freed
// and one CMSG_GET_MIRRORIMAGE_DATA is sent. But other paths (0x00714BD0, 0x0073E63C) free the
// component and clear the "request pending" flag while a request is still in flight, so a model
// reload mid-request (mounting, a display change) sends a second request and the second reply
// overwrites the first reply's component.
//
// The leaked component stays attached to the model with its composited sheet texture still
// registered on the device (callback 0x004EFDF0) and its texture cache entries still linked.
// Nothing frees them, and at exit the device thread (0x004EFE21) or the texture cache's atexit
// cleanup (0x007CECDF) walks into memory that has already been torn down. Server spawned
// player look-alikes (Eluna "mirror image" creatures) reload models far more often than a
// retail Mirror Image does, which is what makes the race common.
//
// Fix: when the handler allocates a component for a unit that still has one, free the old one
// first through the stock free, which detaches its sheet texture's callback (0x00683320) and
// releases its texture cache references. The allocation only happens after the handler has
// accepted the packet, so a dropped packet never costs the unit the look it already has.

namespace MirrorImageFix
{
	CLIENT_FUNCTION(FreeCharComponent, 0x004F16C0, __cdecl, void, (void* component))

	constexpr uintptr_t kOffUnitCharComponent = 0xB4C;

	// Set only while the handler runs, on the main thread. The allocator is shared with
	// every other character, so outside the handler it must behave exactly as stock.
	static uint8_t* sMirrorImageUnit = nullptr;
}

CLIENT_DETOUR(CharAllocComponent, 0x004F0980, __cdecl, void*, (void))
{
	using namespace MirrorImageFix;
	if (sMirrorImageUnit)
	{
		void** slot = reinterpret_cast<void**>(sMirrorImageUnit + kOffUnitCharComponent);
		if (*slot)
		{
			FreeCharComponent(*slot);
			*slot = nullptr;
		}
	}
	return CharAllocComponent();
}

CLIENT_DETOUR_THISCALL(CGUnit_C__OnMirrorImageData, 0x00730290, void, (void* packet))
{
	using namespace MirrorImageFix;
	sMirrorImageUnit = static_cast<uint8_t*>(self);
	CGUnit_C__OnMirrorImageData(self, packet);
	sMirrorImageUnit = nullptr;
}
