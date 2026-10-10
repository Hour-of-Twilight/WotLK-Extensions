#pragma once

#include <Defines.h>

#include <cstdint>
#include <ClientData/CharComponent.h>
#include <ClientData/GameEnums.h>

namespace HDCharacters
{
	constexpr int kRaceCount = 12;

	void Apply();
	void SetEnabled(bool enabled);
	bool IsEnabled();
	bool IsAvailable();
	bool RefreshAvailability();
	bool IsHdComponent(const void* component);
	const char* RaceFolder(int raceId);
	const char* SexFolder(int sexId);
	bool FileExists(const char* path);
	void ApplySlots(void* component);
	bool ApplySkinExtra(void* component);
	void ApplyGeosets(void* component);
	void ApplyCustomizationGroups(void* component);
	void SnapshotHelmGroups(const void* component, int* out);
	void ApplyHelmHides(void* component, const int* before);
	void ApplyHelmFit(void* component);
	void ApplyHelmAttach(void* component);
	void ApplyRetailHelmHides(void* component, const int* before);
	const char* ModelStem(const void* model);
	float BakedScale(int race, int sex);
	bool IsHdModel(const void* model);
	float GlueScale(const void* model);
	bool InGlueTransform();

	struct GlueTransformScope
	{
		GlueTransformScope();
		~GlueTransformScope();
	};

	struct ComponentUnitScope
	{
		explicit ComponentUnitScope(void* unit);
		~ComponentUnitScope();

		ComponentUnitScope(const ComponentUnitScope&) = delete;
		ComponentUnitScope& operator=(const ComponentUnitScope&) = delete;

	private:
		void* m_previous;
	};

	namespace Component
	{
		using namespace ClientData::CharComponent;

		constexpr int kItemSlotHelm = 0;
		constexpr int kGroupHair = 0;
		constexpr int kHelmGroups[] = { 0, 1, 2, 3, 7, 16, 17 };
		constexpr int kHelmGroupCount = 7;
		constexpr int kGroupBoots = 5;
		constexpr int kGroupEyeGlow = 17;
		constexpr int kGroupFeet = 20;
		constexpr int kFirstUnmanagedGroup = 19;
		constexpr int kGeosetFeet = 2001;
		constexpr int kGeosetFeetBooted = 2002;
		constexpr uint32 kOffFaceUpper = 0x1A4;
		constexpr uint32 kOffFacialLower = 0x1AC;
		constexpr uint32 kOffFacialUpper = 0x1B0;
		constexpr uint32 kOffScalpLower = 0x1BC;
		constexpr uint32 kOffScalpUpper = 0x1C0;
		constexpr uint32 kOffPelvisUnderwear = 0x1C4;
		constexpr uint32 kOffTorsoUnderwear = 0x1C8;
		constexpr uint32 kOffItemSections = 0x1D0;
		constexpr uint32 kItemSectionStride = 0x3C;
		constexpr uint32 kItemSectionDisplayIds = 0x1C;
		constexpr uint32 kItemSectionMask = 0x38;
		constexpr int kItemSlotCount = 7;
		constexpr int kItemSectionCount = 10;
		constexpr uint32 kOffRequest = 0x52C;
		constexpr uint32 kFlagNeedsFullPrep = 0x01;
		constexpr uint32 kFlagGeosetsPrep = 0x04;
		constexpr uint32 kFlagRequestQueued = 0x08;
		constexpr uint32 kFlagRobe = 0x20;
		constexpr uint32 kFlags2Npc = 0x01;
		constexpr uint32 kCacheEntryPath = 0x24;
	}
}
