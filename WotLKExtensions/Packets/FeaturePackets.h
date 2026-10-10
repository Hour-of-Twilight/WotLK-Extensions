#pragma once

#include <Defines.h>

#include <cstdint>

struct lua_State;
struct CDataStore;

class FeaturePackets
{
public:
	enum Feature : uint32
	{
		GEM_SYSTEM = 0,
		AOE_LOOT = 1,
		TRANSMOG = 2,
		PET_AUTO_LOOT = 3
	};

	static FeaturePackets& Instance();

	FeaturePackets(const FeaturePackets&) = delete;
	FeaturePackets& operator=(const FeaturePackets&) = delete;

	void Apply();
	bool IsEnabled(uint32 featureId) const;

private:
	FeaturePackets() = default;

	static void Handler_SMSG_FEATURE_TOGGLE(void*, uint32, uint32, CDataStore* pkt);
	static int RequestFeatureFlags(lua_State* L);
	static int HasFeatureFlags(lua_State* L);
	static int IsFeatureEnabled(lua_State* L);

	uint32 m_mask = 0;
	bool m_hasData = false;
};

#define sFeaturePackets FeaturePackets::Instance()
