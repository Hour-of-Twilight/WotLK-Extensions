#pragma once

#include <Defines.h>

#include <Macros.h>
#include <ClientData/Object.h>
#include <ClientData/ObjectFields.h>

#include <cstdint>

namespace ClientData::ObjectManager
{
	CLIENT_FUNCTION(ObjectPtr, 0x4D4DB0, __cdecl, CGObject_C*, (uint64 guid, uint32 typeMask))
	CLIENT_FUNCTION(GetActivePlayerObject, 0x4038F0, __cdecl, CGObject_C*, ())

	inline CGObject_C* GetObject(uint64 guid, ObjectTypeMask typeMask)
	{
		return ObjectPtr(guid, static_cast<uint32>(typeMask));
	}
}
