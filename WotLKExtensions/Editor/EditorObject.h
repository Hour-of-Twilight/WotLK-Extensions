#pragma once

#include <Defines.h>

#include <ClientData/GameObject.h>
#include <ClientData/ObjectFields.h>
#include <ClientData/ObjectManager.h>

#include <cstdint>
#include <string>

namespace EditorObject
{
	struct DecodedGuid
	{
		uint64 full = 0;
		uint16 type = 0;
		uint32 entry = 0;
		uint32 low = 0;
	};

	inline DecodedGuid DecodeClientGuid(uint32 rawLow, uint32 rawHigh)
	{
		DecodedGuid guid{};
		guid.full = (uint64(rawHigh) << 32) | rawLow;
		guid.type = uint16(guid.full >> 48);
		guid.entry = uint32((guid.full >> 24) & 0xFFFFFF);
		guid.low = uint32(guid.full & 0xFFFFFF);
		return guid;
	}

	inline std::string GuidToString(uint64 guid)
	{
		return std::to_string(guid);
	}

	inline ClientData::CGGameObject_C* GameObjectByGuid(uint64 guid)
	{
		return ClientData::AsClientGameObject(
		    ClientData::ObjectManager::GetObject(guid, ClientData::TYPEMASK_OBJECT));
	}
}
