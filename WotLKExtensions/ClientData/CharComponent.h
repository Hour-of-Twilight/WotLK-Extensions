#pragma once

#include <Defines.h>

#include <cstddef>
#include <cstdint>

namespace ClientData::CharComponent
{
	constexpr uint32 kOffFlags = 0x08;
	constexpr uint32 kOffDirty = 0x0C;
	constexpr uint32 kOffComponentData = 0x18;

	constexpr uint32 kOffDataRace = 0x00;
	constexpr uint32 kOffDataSex = 0x04;
	constexpr uint32 kOffDataClass = 0x08;
	constexpr uint32 kOffDataHairColor = 0x0C;
	constexpr uint32 kOffDataSkinColor = 0x10;
	constexpr uint32 kOffDataFace = 0x14;
	constexpr uint32 kOffDataFacialStyle = 0x18;
	constexpr uint32 kOffDataHairStyle = 0x1C;
	constexpr uint32 kOffDataModel = 0x20;
	constexpr uint32 kOffDataFlags = 0x24;
	constexpr uint32 kOffDataBakePath = 0x28;
	constexpr uint32 kOffDataGeosetGroups = 0x12C;
	constexpr size_t kDataBakePathSize = 260;
	constexpr uint32 kDataFlagBaked = 0x01;

	constexpr uint32 kOffRace = kOffComponentData + kOffDataRace;
	constexpr uint32 kOffSex = kOffComponentData + kOffDataSex;
	constexpr uint32 kOffClass = kOffComponentData + kOffDataClass;
	constexpr uint32 kOffHairColor = kOffComponentData + kOffDataHairColor;
	constexpr uint32 kOffSkinColor = kOffComponentData + kOffDataSkinColor;
	constexpr uint32 kOffFace = kOffComponentData + kOffDataFace;
	constexpr uint32 kOffFacialStyle = kOffComponentData + kOffDataFacialStyle;
	constexpr uint32 kOffHairStyle = kOffComponentData + kOffDataHairStyle;
	constexpr uint32 kOffModel = kOffComponentData + kOffDataModel;
	constexpr uint32 kOffFlags2 = kOffComponentData + kOffDataFlags;
	constexpr uint32 kOffGeosetGroups = kOffComponentData + kOffDataGeosetGroups;
	constexpr uint32 kOffBaseTexture = 0x190;
	constexpr uint32 kOffSkin = 0x194;
	constexpr uint32 kOffFaceLower = 0x1A0;
	constexpr uint32 kOffItemDisplayIds = 0x428;

	static_assert(kOffModel == 0x38 && kOffGeosetGroups == 0x144, "CCharacterComponent layout drifted");

	inline void* Model(const void* component)
	{
		return component ? *reinterpret_cast<void* const*>(static_cast<const uint8*>(component) + kOffModel) : nullptr;
	}
}
