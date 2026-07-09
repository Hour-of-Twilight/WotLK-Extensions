#include "FeaturePackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <CustomLua.h>
#include <XMLExtensions.h>
#include <SharedDefines.h>

FeaturePackets& FeaturePackets::Instance()
{
	static FeaturePackets instance;
	return instance;
}

void FeaturePackets::Handler_SMSG_FEATURE_TOGGLE(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	uint32_t mask = Packet(pkt).GetUInt32();

	Instance().m_mask = mask;
	Instance().m_hasData = true;
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_FEATURE_UPDATE"), "");
}

int FeaturePackets::RequestFeatureFlags(lua_State*)
{
	Packet(CMSG_FEATURE_TOGGLE_REQUEST).Send();
	return 0;
}

int FeaturePackets::HasFeatureFlags(lua_State* L)
{
	FrameScript::PushBoolean(L, Instance().m_hasData);
	return 1;
}

int FeaturePackets::IsFeatureEnabled(lua_State* L)
{
	uint32_t featureId = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));
	bool enabled = Instance().m_hasData && (Instance().m_mask & (1u << featureId)) != 0;
	FrameScript::PushBoolean(L, enabled);
	return 1;
}

void FeaturePackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_FEATURE_TOGGLE, &Handler_SMSG_FEATURE_TOGGLE);

	sLua.RegisterFunction("RequestFeatureFlags", &RequestFeatureFlags, LuaFunctionState::FRAME);
	sLua.RegisterFunction("HasFeatureFlags", &HasFeatureFlags, LuaFunctionState::FRAME);
	sLua.RegisterFunction("IsFeatureEnabled", &IsFeatureEnabled, LuaFunctionState::FRAME);
}
