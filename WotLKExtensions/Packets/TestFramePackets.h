#pragma once

#include <cstdint>
#include <vector>

struct lua_State;
struct CDataStore;

class TestFramePackets
{
public:
	static TestFramePackets& Instance();

	TestFramePackets(const TestFramePackets&) = delete;
	TestFramePackets& operator=(const TestFramePackets&) = delete;

	void Apply();

private:
	TestFramePackets() = default;

	static void Handler_SMSG_CUSTOM_TEST_FRAME(void*, uint32_t, uint32_t, CDataStore* a3);
	static int GetTestFrameData(lua_State* L);
	static int GetTestFrameAllowedStatGroups(lua_State* L);
	static int SendTestFrameReply(lua_State* L);

	char m_testName[256] = {};
	char m_testShortDesc[512] = {};
	char m_testLongDesc[4096] = {};
	uint32_t m_talentLevel = 0;
	uint32_t m_avgItemLevel = 0;
	std::vector<uint32_t> m_allowedStatGroups;
	bool m_hasData = false;
};

#define sTestFramePackets TestFramePackets::Instance()
