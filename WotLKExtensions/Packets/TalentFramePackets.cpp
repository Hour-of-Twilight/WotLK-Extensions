#include <TalentFramePackets.h>
#include <Packets/Packet.h>
#include <CustomPacket.h>
#include <CustomLua.h>
#include <ClientData/ClientFunctions.h>
#include <XMLExtensions.h>
#include <SharedDefines.h>

TalentFramePackets& TalentFramePackets::Instance()
{
	static TalentFramePackets instance;
	return instance;
}

void TalentFramePackets::ReadLearntTalents(Packet& r, std::vector<uint32_t>& out)
{
	uint32_t count = r.GetUInt32();
	out.clear();
	out.reserve(count);
	for (uint32_t i = 0; i < count; ++i)
		out.push_back(r.GetUInt32());
}

void TalentFramePackets::Send(uint32_t opcode)
{
	Packet(opcode).Send();
}

void TalentFramePackets::SendU32(uint32_t opcode, uint32_t value)
{
	Packet(opcode).PutUInt32(value).Send();
}


void TalentFramePackets::Handler_SMSG_CUSTOM_TALENT_CACHE(void*, uint32_t, uint32_t, CDataStore* a3)
{
	Packet r(a3);
	uint32_t count = r.GetUInt32();

	auto& nodes = Instance().m_nodes;
	nodes.clear();
	nodes.reserve(count);

	for (uint32_t i = 0; i < count; ++i)
	{
		NodeInfo info;
		info.index = r.GetUInt32();
		info.spellId = r.GetUInt32();
		info.xOffset = r.GetDouble();
		info.yOffset = r.GetDouble();

		uint32_t linkCount = r.GetUInt32();
		info.childLinks.reserve(linkCount);
		for (uint32_t j = 0; j < linkCount; ++j)
			info.childLinks.push_back(r.GetUInt32());

		info.mutex = r.GetUInt32();
		info.buttonType = r.GetUInt32();
		info.flagMask = r.GetUInt32();

		nodes[info.index] = std::move(info);
	}

	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_TALENT_CACHE"), "");
}

void TalentFramePackets::Handler_SMSG_CUSTOM_TALENT_SMALL_CACHE(void*, uint32_t, uint32_t, CDataStore* a3)
{
	Packet r(a3);
	TalentFramePackets& self = Instance();
	self.m_version = r.GetUInt32();
	self.m_freePoints = r.GetUInt32();
	self.m_talentLevel = r.GetUInt32();
	ReadLearntTalents(r, self.m_learntTalents);
	ReadLearntTalents(r, self.m_itemGrantedTalents);
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_TALENT_SMALL_CACHE"), "");
}

void TalentFramePackets::Handler_SMSG_CUSTOM_TALENT_ITEM_GRANTED_UPDATE(void*, uint32_t, uint32_t, CDataStore* a3)
{
	Packet r(a3);
	ReadLearntTalents(r, Instance().m_itemGrantedTalents);
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_TALENT_ITEM_GRANTED_UPDATE"), "");
}

void TalentFramePackets::Handler_SMSG_CUSTOM_TALENT_POINTS(void*, uint32_t, uint32_t, CDataStore* a3)
{
	uint32_t freePoints = Packet(a3).GetUInt32();
	Instance().m_freePoints = freePoints;
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_TALENT_POINTS"), "%u", freePoints);
}

void TalentFramePackets::Handler_SMSG_CUSTOM_TALENT_LEVEL(void*, uint32_t, uint32_t, CDataStore* a3)
{
	uint32_t talentLevel = Packet(a3).GetUInt32();
	Instance().m_talentLevel = talentLevel;
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_TALENT_LEVEL"), "%u", talentLevel);
}

void TalentFramePackets::Handler_SMSG_CUSTOM_TALENT_LEARN_RESPONSE(void*, uint32_t, uint32_t, CDataStore* a3)
{
	Packet r(a3);
	uint32_t nodeId = r.GetUInt32();
	uint8_t canLearn = r.GetUInt8();
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_TALENT_LEARN_RESPONSE"), "%u%u", nodeId, (uint32_t)canLearn);
}

void TalentFramePackets::Handler_SMSG_CUSTOM_TALENT_UNLEARN_RESPONSE(void*, uint32_t, uint32_t, CDataStore* a3)
{
	Packet r(a3);
	uint32_t nodeId = r.GetUInt32();
	uint8_t result = r.GetUInt8();
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_TALENT_UNLEARN_RESPONSE"), "%u%u", nodeId, (uint32_t)result);
}

void TalentFramePackets::Handler_SMSG_CUSTOM_TALENT_RESET(void*, uint32_t, uint32_t, CDataStore* a3)
{
	uint32_t freePoints = Packet(a3).GetUInt32();
	TalentFramePackets& self = Instance();
	self.m_freePoints = freePoints;
	self.m_learntTalents.clear();
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_TALENT_RESET"), "%u", freePoints);
}

void TalentFramePackets::Handler_SMSG_CUSTOM_TALENT_INSPECT_RESPONSE(void*, uint32_t, uint32_t, CDataStore* a3)
{
	Packet r(a3);
	TalentFramePackets& self = Instance();
	self.m_inspectFreePoints = r.GetUInt32();
	self.m_inspectTalentLevel = r.GetUInt32();
	ReadLearntTalents(r, self.m_inspectLearntTalents);
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_TALENT_INSPECT_RESPONSE"), "");
}

void TalentFramePackets::Handler_SMSG_CUSTOM_TALENT_NEW(void*, uint32_t, uint32_t, CDataStore* a3)
{
	uint32_t nodeId = Packet(a3).GetUInt32();
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_TALENT_NEW"), "%u", nodeId);
}

void TalentFramePackets::Handler_SMSG_CUSTOM_TALENT_LEARNT_UPDATE(void*, uint32_t, uint32_t, CDataStore* a3)
{
	Packet r(a3);
	uint32_t freePoints = r.GetUInt32();
	ReadLearntTalents(r, Instance().m_learntTalents);
	Instance().m_freePoints = freePoints;
	FrameScript::SignalEvent(FrameXMLExtensions::GetEventIdByName("HOT_TALENT_LEARNT_UPDATE"), "%u", freePoints);
}

int TalentFramePackets::GetCustomTalentStorage(lua_State* L)
{
	auto const& nodes = Instance().m_nodes;

	FrameScript::CreateTable(L, (int)nodes.size(), 0);
	int maintable = FrameScript::GetTop(L);

	for (auto const& itr : nodes)
	{
		NodeInfo const& node = itr.second;

		FrameScript::CreateTable(L, 7, 0);
		int subtable = FrameScript::GetTop(L);

		FrameScript::PushNumber(L, node.spellId);
		FrameScript::RawSetI(L, subtable, 1);

		FrameScript::PushNumber(L, node.xOffset);
		FrameScript::RawSetI(L, subtable, 2);

		FrameScript::PushNumber(L, node.yOffset);
		FrameScript::RawSetI(L, subtable, 3);

		FrameScript::CreateTable(L, (int)node.childLinks.size(), 0);
		int linktable = FrameScript::GetTop(L);
		for (int i = 0; i < (int)node.childLinks.size(); ++i)
		{
			FrameScript::PushNumber(L, node.childLinks[i]);
			FrameScript::RawSetI(L, linktable, i + 1);
		}
		FrameScript::RawSetI(L, subtable, 4);

		FrameScript::PushNumber(L, node.mutex);
		FrameScript::RawSetI(L, subtable, 5);

		FrameScript::PushNumber(L, node.buttonType);
		FrameScript::RawSetI(L, subtable, 6);

		FrameScript::PushNumber(L, node.flagMask);
		FrameScript::RawSetI(L, subtable, 7);

		FrameScript::RawSetI(L, maintable, node.index);
	}

	FrameScript::SetTop(L, maintable);
	return 1;
}

int TalentFramePackets::GetTalentTreeVersion(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_version);
	return 1;
}

int TalentFramePackets::GetCachedTalentFreePoints(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_freePoints);
	return 1;
}

int TalentFramePackets::GetCachedTalentLevel(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_talentLevel);
	return 1;
}

static int PushTalentList(lua_State* L, std::vector<uint32_t> const& talents)
{
	FrameScript::CreateTable(L, (int)talents.size(), 0);
	int tbl = FrameScript::GetTop(L);
	for (int i = 0; i < (int)talents.size(); ++i)
	{
		FrameScript::PushNumber(L, talents[i]);
		FrameScript::RawSetI(L, tbl, i + 1);
	}
	return 1;
}

int TalentFramePackets::GetCachedLearntTalents(lua_State* L)
{
	return PushTalentList(L, Instance().m_learntTalents);
}

int TalentFramePackets::GetCachedItemGrantedTalents(lua_State* L)
{
	return PushTalentList(L, Instance().m_itemGrantedTalents);
}

int TalentFramePackets::GetCachedInspectLearntTalents(lua_State* L)
{
	return PushTalentList(L, Instance().m_inspectLearntTalents);
}

int TalentFramePackets::GetCachedInspectFreePoints(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_inspectFreePoints);
	return 1;
}

int TalentFramePackets::GetCachedInspectTalentLevel(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_inspectTalentLevel);
	return 1;
}

int TalentFramePackets::RequestTalentCache(lua_State*)
{
	Send(CMSG_CUSTOM_TALENT_CACHE_REQUEST);
	return 0;
}

int TalentFramePackets::RequestTalentSmallCache(lua_State*)
{
	Send(CMSG_CUSTOM_TALENT_SMALL_CACHE_REQ);
	return 0;
}

int TalentFramePackets::RequestTalentPoints(lua_State*)
{
	Send(CMSG_CUSTOM_TALENT_POINTS_REQUEST);
	return 0;
}

int TalentFramePackets::LearnTalent(lua_State* L)
{
	SendU32(CMSG_CUSTOM_TALENT_LEARN, (uint32_t)FrameScript::GetNumber(L, 1));
	return 0;
}

int TalentFramePackets::UnlearnTalent(lua_State* L)
{
	SendU32(CMSG_CUSTOM_TALENT_UNLEARN, (uint32_t)FrameScript::GetNumber(L, 1));
	return 0;
}

int TalentFramePackets::ResetTalents(lua_State*)
{
	Send(CMSG_CUSTOM_TALENT_RESET);
	return 0;
}

int TalentFramePackets::InspectTalents(lua_State* L)
{
	char* playerName = FrameScript::ToLString(L, 1, false);
	if (!playerName || playerName[0] == '\0')
		return 0;

	Packet(CMSG_CUSTOM_TALENT_INSPECT).PutString(playerName).Send();
	return 0;
}

int TalentFramePackets::SwitchTalentLoadout(lua_State*)
{
	Send(CMSG_CUSTOM_TALENT_LOADOUT_SWITCH);
	return 0;
}

int TalentFramePackets::ImportTalentBuild(lua_State* L)
{
	int count = FrameScript::ObjLen(L, 1);
	if (count <= 0)
		return 0;
	if (count > 1000)
		count = 1000;

	Packet pkt(CMSG_CUSTOM_TALENT_IMPORT);
	pkt.PutUInt32((uint32_t)count);

	int top = FrameScript::GetTop(L);
	for (int i = 1; i <= count; ++i)
	{
		FrameScript::RawGetI(L, 1, i);
		uint32_t nodeId = (uint32_t)FrameScript::GetNumber(L, top + 1);
		FrameScript::SetTop(L, top);
		pkt.PutUInt32(nodeId);
	}

	pkt.Send();
	return 0;
}

int TalentFramePackets::TalentEditorSetLink(lua_State* L)
{
	uint32_t nodeId = (uint32_t)FrameScript::GetNumber(L, 1);
	uint32_t linkTo = (uint32_t)FrameScript::GetNumber(L, 2);

	Packet(CMSG_TALENT_EDITOR_SET_LINK).PutUInt32(nodeId).PutUInt32(linkTo).Send();
	return 0;
}

int TalentFramePackets::TalentEditorDeleteLinks(lua_State* L)
{
	SendU32(CMSG_TALENT_EDITOR_DELETE_LINKS, (uint32_t)FrameScript::GetNumber(L, 1));
	return 0;
}

int TalentFramePackets::TalentEditorUpdateNode(lua_State* L)
{
	uint32_t nodeId = (uint32_t)FrameScript::GetNumber(L, 1);
	uint32_t spellId = (uint32_t)FrameScript::GetNumber(L, 2);
	double x = FrameScript::GetNumber(L, 3);
	double y = FrameScript::GetNumber(L, 4);
	uint32_t buttonType = (uint32_t)FrameScript::GetNumber(L, 5);
	uint32_t flagMask = (uint32_t)FrameScript::GetNumber(L, 6);

	Packet(CMSG_TALENT_EDITOR_UPDATE_NODE)
	    .PutUInt32(nodeId)
	    .PutUInt32(spellId)
	    .PutDouble(x)
	    .PutDouble(y)
	    .PutUInt32(buttonType)
	    .PutUInt32(flagMask)
	    .Send();
	return 0;
}

int TalentFramePackets::TalentEditorDeleteNode(lua_State* L)
{
	SendU32(CMSG_TALENT_EDITOR_DELETE_NODE, (uint32_t)FrameScript::GetNumber(L, 1));
	return 0;
}

int TalentFramePackets::TalentEditorNewNode(lua_State*)
{
	Send(CMSG_TALENT_EDITOR_NEW_NODE);
	return 0;
}

void TalentFramePackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_CUSTOM_TALENT_CACHE, &Handler_SMSG_CUSTOM_TALENT_CACHE);
	sCustomPacket.RegisterHandler(SMSG_CUSTOM_TALENT_SMALL_CACHE, &Handler_SMSG_CUSTOM_TALENT_SMALL_CACHE);
	sCustomPacket.RegisterHandler(SMSG_CUSTOM_TALENT_POINTS, &Handler_SMSG_CUSTOM_TALENT_POINTS);
	sCustomPacket.RegisterHandler(SMSG_CUSTOM_TALENT_LEVEL, &Handler_SMSG_CUSTOM_TALENT_LEVEL);
	sCustomPacket.RegisterHandler(SMSG_CUSTOM_TALENT_LEARN_RESPONSE, &Handler_SMSG_CUSTOM_TALENT_LEARN_RESPONSE);
	sCustomPacket.RegisterHandler(SMSG_CUSTOM_TALENT_UNLEARN_RESPONSE, &Handler_SMSG_CUSTOM_TALENT_UNLEARN_RESPONSE);
	sCustomPacket.RegisterHandler(SMSG_CUSTOM_TALENT_RESET, &Handler_SMSG_CUSTOM_TALENT_RESET);
	sCustomPacket.RegisterHandler(SMSG_CUSTOM_TALENT_INSPECT_RESPONSE, &Handler_SMSG_CUSTOM_TALENT_INSPECT_RESPONSE);
	sCustomPacket.RegisterHandler(SMSG_CUSTOM_TALENT_NEW, &Handler_SMSG_CUSTOM_TALENT_NEW);
	sCustomPacket.RegisterHandler(SMSG_CUSTOM_TALENT_LEARNT_UPDATE, &Handler_SMSG_CUSTOM_TALENT_LEARNT_UPDATE);
	sCustomPacket.RegisterHandler(SMSG_CUSTOM_TALENT_ITEM_GRANTED_UPDATE, &Handler_SMSG_CUSTOM_TALENT_ITEM_GRANTED_UPDATE);

	sLua.RegisterFunction("GetCustomTalentStorage", &GetCustomTalentStorage, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetTalentTreeVersion", &GetTalentTreeVersion, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetCachedTalentFreePoints", &GetCachedTalentFreePoints, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetCachedTalentLevel", &GetCachedTalentLevel, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetCachedLearntTalents", &GetCachedLearntTalents, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetCachedItemGrantedTalents", &GetCachedItemGrantedTalents, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetCachedInspectLearntTalents", &GetCachedInspectLearntTalents, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetCachedInspectFreePoints", &GetCachedInspectFreePoints, LuaFunctionState::FRAME);
	sLua.RegisterFunction("GetCachedInspectTalentLevel", &GetCachedInspectTalentLevel, LuaFunctionState::FRAME);

	sLua.RegisterFunction("CustomRequestTalentCache", &RequestTalentCache, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CustomRequestTalentSmallCache", &RequestTalentSmallCache, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CustomRequestTalentPoints", &RequestTalentPoints, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CustomLearnTalent", &LearnTalent, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CustomUnlearnTalent", &UnlearnTalent, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CustomResetTalents", &ResetTalents, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CustomInspectTalents", &InspectTalents, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CustomSwitchTalentLoadout", &SwitchTalentLoadout, LuaFunctionState::FRAME);
	sLua.RegisterFunction("CustomImportTalentBuild", &ImportTalentBuild, LuaFunctionState::FRAME);

	sLua.RegisterFunction("TalentEditorSetLink", &TalentEditorSetLink, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TalentEditorDeleteLinks", &TalentEditorDeleteLinks, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TalentEditorUpdateNode", &TalentEditorUpdateNode, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TalentEditorDeleteNode", &TalentEditorDeleteNode, LuaFunctionState::FRAME);
	sLua.RegisterFunction("TalentEditorNewNode", &TalentEditorNewNode, LuaFunctionState::FRAME);
}
