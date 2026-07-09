#pragma once

#include <SharedDefines.h>
#include <vector>
#include <unordered_map>

struct lua_State;
class Packet;

struct NodeInfo
{
	uint32_t index = 0;
	uint32_t spellId = 0;
	double xOffset = 0.0;
	double yOffset = 0.0;
	uint32_t mutex = 0;
	uint32_t buttonType = 0;
	uint32_t flagMask = 0;
	std::vector<uint32_t> childLinks;
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

	static void ReadLearntTalents(Packet& r, std::vector<uint32_t>& out);
	static void Send(uint32_t opcode);
	static void SendU32(uint32_t opcode, uint32_t value);

	static void Handler_SMSG_CUSTOM_TALENT_CACHE(void*, uint32_t, uint32_t, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_SMALL_CACHE(void*, uint32_t, uint32_t, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_ITEM_GRANTED_UPDATE(void*, uint32_t, uint32_t, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_POINTS(void*, uint32_t, uint32_t, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_LEVEL(void*, uint32_t, uint32_t, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_LEARN_RESPONSE(void*, uint32_t, uint32_t, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_UNLEARN_RESPONSE(void*, uint32_t, uint32_t, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_RESET(void*, uint32_t, uint32_t, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_INSPECT_RESPONSE(void*, uint32_t, uint32_t, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_NEW(void*, uint32_t, uint32_t, CDataStore* a3);
	static void Handler_SMSG_CUSTOM_TALENT_LEARNT_UPDATE(void*, uint32_t, uint32_t, CDataStore* a3);

	static int GetCustomTalentStorage(lua_State* L);
	static int GetTalentTreeVersion(lua_State* L);
	static int GetCachedTalentFreePoints(lua_State* L);
	static int GetCachedTalentLevel(lua_State* L);
	static int GetCachedLearntTalents(lua_State* L);
	static int GetCachedItemGrantedTalents(lua_State* L);
	static int GetCachedInspectLearntTalents(lua_State* L);
	static int GetCachedInspectFreePoints(lua_State* L);
	static int GetCachedInspectTalentLevel(lua_State* L);

	static int RequestTalentCache(lua_State* L);
	static int RequestTalentSmallCache(lua_State* L);
	static int RequestTalentPoints(lua_State* L);
	static int LearnTalent(lua_State* L);
	static int UnlearnTalent(lua_State* L);
	static int ResetTalents(lua_State* L);
	static int InspectTalents(lua_State* L);
	static int SwitchTalentLoadout(lua_State* L);
	static int ImportTalentBuild(lua_State* L);

	static int TalentEditorSetLink(lua_State* L);
	static int TalentEditorDeleteLinks(lua_State* L);
	static int TalentEditorUpdateNode(lua_State* L);
	static int TalentEditorDeleteNode(lua_State* L);
	static int TalentEditorNewNode(lua_State* L);

	std::unordered_map<uint32_t, NodeInfo> m_nodes;
	uint32_t m_version = 0;
	uint32_t m_freePoints = 0;
	uint32_t m_talentLevel = 0;
	std::vector<uint32_t> m_learntTalents;
	std::vector<uint32_t> m_itemGrantedTalents;
	std::vector<uint32_t> m_inspectLearntTalents;
	uint32_t m_inspectFreePoints = 0;
	uint32_t m_inspectTalentLevel = 0;
};

#define sTalentFramePackets TalentFramePackets::Instance()
