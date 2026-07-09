#include "TestFramePackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <CustomLua.h>
#include <XMLExtensions.h>
#include <SharedDefines.h>

TestFramePackets& TestFramePackets::Instance()
{
	static TestFramePackets instance;
	return instance;
}

void TestFramePackets::Handler_SMSG_CUSTOM_TEST_FRAME(void*, uint32_t, uint32_t, CDataStore* a3)
{
	Packet r(a3);
	TestFramePackets& self = Instance();
	r.GetString(self.m_testName, sizeof(self.m_testName));
	r.GetString(self.m_testShortDesc, sizeof(self.m_testShortDesc));
	r.GetString(self.m_testLongDesc, sizeof(self.m_testLongDesc));
	self.m_talentLevel = r.GetUInt32();
	self.m_avgItemLevel = r.GetUInt32();

	uint32_t numGroups = r.GetUInt32();
	self.m_allowedStatGroups.clear();
	self.m_allowedStatGroups.reserve(numGroups);
	for (uint32_t i = 0; i < numGroups; ++i)
		self.m_allowedStatGroups.push_back(r.GetUInt32());

	self.m_hasData = true;
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_TEST_FRAME"), "");
}

int TestFramePackets::GetTestFrameData(lua_State* L)
{
	TestFramePackets& self = Instance();
	if (!self.m_hasData)
	{
		FrameScript::PushNil(L);
		return 1;
	}
	FrameScript::PushString(L, self.m_testName);
	FrameScript::PushString(L, self.m_testShortDesc);
	FrameScript::PushString(L, self.m_testLongDesc);
	FrameScript::PushNumber(L, self.m_talentLevel);
	FrameScript::PushNumber(L, self.m_avgItemLevel);
	return 5;
}

int TestFramePackets::GetTestFrameAllowedStatGroups(lua_State* L)
{
	TestFramePackets& self = Instance();
	FrameScript::CreateTable(L, (int)self.m_allowedStatGroups.size(), 0);
	int t = FrameScript::GetTop(L);
	for (int i = 0; i < (int)self.m_allowedStatGroups.size(); ++i)
	{
		FrameScript::PushNumber(L, self.m_allowedStatGroups[i]);
		FrameScript::RawSetI(L, t, i + 1);
	}
	return 1;
}

int TestFramePackets::SendTestFrameReply(lua_State* L)
{
	uint32_t statGroup = (uint32_t)FrameScript::GetNumber(L, 1);
	uint32_t mainWeaponId = (uint32_t)FrameScript::GetNumber(L, 2);
	uint32_t offhandId = (uint32_t)FrameScript::GetNumber(L, 3);
	uint32_t rangedId = (uint32_t)FrameScript::GetNumber(L, 4);
	uint32_t armorType = (uint32_t)FrameScript::GetNumber(L, 5);

	Packet(CMSG_CUSTOM_TEST_FRAME_REPLY)
	    .PutUInt32(statGroup)
	    .PutUInt32(mainWeaponId)
	    .PutUInt32(offhandId)
	    .PutUInt32(rangedId)
	    .PutUInt32(armorType)
	    .Send();
	return 0;
}

void TestFramePackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_CUSTOM_TEST_FRAME, &Handler_SMSG_CUSTOM_TEST_FRAME);

	sLua.RegisterFunction("GetTestFrameData", &GetTestFrameData, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetTestFrameAllowedStatGroups", &GetTestFrameAllowedStatGroups, LuaFunctionState::FRAME);
	sLua.RegisterFunction("SendTestFrameReply", &SendTestFrameReply, LuaFunctionState::FRAME);
}
