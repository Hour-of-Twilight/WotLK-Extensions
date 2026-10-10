#pragma once

#include <Defines.h>

#include <SharedDefines.h>
#include <string>
#include <vector>
#include <unordered_map>

struct lua_State;
class Packet;

struct TalentLoadoutInfo
{
	uint32 id = 0;
	std::string name;
	uint32 nodeCount = 0;
};

struct NodeInfo
{
	uint32 index = 0;
	uint32 spellId = 0;
	double xOffset = 0.0;
	double yOffset = 0.0;
	uint32 mutex = 0;
	uint32 buttonType = 0;
	uint32 flagMask = 0;
	std::vector<uint32> childLinks;
};

class TalentFramePackets
{
public:
	static TalentFramePackets& Instance();

	TalentFramePackets(const TalentFramePackets&) = delete;
	TalentFramePackets& operator=(const TalentFramePackets&) = delete;

	void Apply();

private:
	TalentFramePackets() = default;

	static void ReadLearntTalents(Packet& r, std::vector<uint32>& out);
	static void Send(uint32 opcode);
	static void SendU32(uint32 opcode, uint32 value);

	static void Handler_SMSG_CUSTOM_TALENT_CACHE(void*, uint32, uint32, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_SMALL_CACHE(void*, uint32, uint32, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_ITEM_GRANTED_UPDATE(void*, uint32, uint32, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_POINTS(void*, uint32, uint32, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_LEVEL(void*, uint32, uint32, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_LEARN_RESPONSE(void*, uint32, uint32, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_UNLEARN_RESPONSE(void*, uint32, uint32, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_RESET(void*, uint32, uint32, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_INSPECT_RESPONSE(void*, uint32, uint32, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_NEW(void*, uint32, uint32, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_LEARNT_UPDATE(void*, uint32, uint32, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_LOADOUTS(void*, uint32, uint32, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_LOADOUT_RESULT(void*, uint32, uint32, CDataStore* a3);

	static int GetCustomTalentStorage(lua_State* L);
	static int GetTalentTreeVersion(lua_State* L);
	static int GetCachedTalentFreePoints(lua_State* L);
	static int GetCachedTalentLevel(lua_State* L);
	static int GetCachedXPTalentLevel(lua_State* L);
	static int GetCachedMaxTalentLevel(lua_State* L);
	static int GetCachedLearntTalents(lua_State* L);
	static int GetCachedItemGrantedTalents(lua_State* L);
	static int GetCachedInspectLearntTalents(lua_State* L);
	static int GetCachedInspectFreePoints(lua_State* L);
	static int GetCachedInspectTalentLevel(lua_State* L);
	static int GetCachedTalentLoadouts(lua_State* L);
	static int GetActiveTalentLoadout(lua_State* L);
	static int GetMaxTalentLoadouts(lua_State* L);

	static int RequestTalentCache(lua_State* L);
	static int RequestTalentSmallCache(lua_State* L);
	static int RequestTalentPoints(lua_State* L);
	static int LearnTalent(lua_State* L);
	static int UnlearnTalent(lua_State* L);
	static int ResetTalents(lua_State* L);
	static int InspectTalents(lua_State* L);
	static int SwitchTalentLoadout(lua_State* L);
	static int CreateTalentLoadout(lua_State* L);
	static int DeleteTalentLoadout(lua_State* L);
	static int RenameTalentLoadout(lua_State* L);
	static int RequestTalentLoadouts(lua_State* L);
	static int ImportTalentBuild(lua_State* L);

	static int TalentEditorSetLink(lua_State* L);
	static int TalentEditorDeleteLinks(lua_State* L);
	static int TalentEditorUpdateNode(lua_State* L);
	static int TalentEditorDeleteNode(lua_State* L);
	static int TalentEditorNewNode(lua_State* L);

	std::unordered_map<uint32, NodeInfo> m_nodes;
	uint32 m_version = 0;
	uint32 m_freePoints = 0;
	uint32 m_talentLevel = 0;
	uint32 m_xpTalentLevel = 0;
	uint32 m_maxTalentLevel = 0;
	std::vector<uint32> m_learntTalents;
	std::vector<uint32> m_itemGrantedTalents;
	std::vector<uint32> m_inspectLearntTalents;
	uint32 m_inspectFreePoints = 0;
	uint32 m_inspectTalentLevel = 0;
	std::vector<TalentLoadoutInfo> m_loadouts;
	uint32 m_activeLoadout = 0;
	uint32 m_maxLoadouts = 0;
};

#define sTalentFramePackets TalentFramePackets::Instance()
