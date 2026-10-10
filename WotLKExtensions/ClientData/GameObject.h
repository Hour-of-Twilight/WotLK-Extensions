#pragma once

#include <Defines.h>

#include <ClientData/MathTypes.h>
#include <ClientData/Object.h>
#include <cstddef>
#include <cstdint>

namespace ClientData
{
	struct CPassenger
	{
		uint32 unk_0000;
		uint32 unk_0004;
		uint64 transportGuid;
		C3Vector position;
		uint32 unk_001C;
		uint64 compressedRotation;
		uint64 guid2;
		uint32 unk_002C;
	};

	class CGGameObject_C : public CGObject_C
	{
	public:
		uint32 unk_00D0;
		uint32 unk_00D4;
		CPassenger m_passenger;
	};

	inline CGGameObject_C* AsClientGameObject(CGObject_C* object)
	{
		if (!object || object->GetTypeID() != TYPEID_GAMEOBJECT)
			return nullptr;

		return reinterpret_cast<CGGameObject_C*>(object);
	}
}
