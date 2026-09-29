#pragma once

#include <cstdint>

struct lua_State;
struct CDataStore;

class FeaturePackets
{
public:
	enum Feature : uint32_t
	{
		GEM_SYSTEM = 0,
		AOE_LOOT = 1,
		TRANSMOG = 2
	};

	static FeaturePackets& Instance();

	FeaturePackets(const FeaturePackets&) = delete;
	FeaturePackets& operator=(const FeaturePackets&) = delete;

	void Apply();
	bool IsEnabled(uint32_t featureId) const;

private:
	FeaturePackets() = default;

	static void Handler_SMSG_FEATURE_TOGGLE(void*, uint32_t, uint32_t, CDataStore* pkt);
	static int RequestFeatureFlags(lua_State* L);
	static int HasFeatureFlags(lua_State* L);
	static int IsFeatureEnabled(lua_State* L);

	uint32_t m_mask = 0;
	bool m_hasData = false;
};

#define sFeaturePackets FeaturePackets::Instance()
