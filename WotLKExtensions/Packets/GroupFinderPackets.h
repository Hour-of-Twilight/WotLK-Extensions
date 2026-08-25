#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct lua_State;
struct CDataStore;

class GroupFinderPackets
{
public:
	static GroupFinderPackets& Instance();

	GroupFinderPackets(const GroupFinderPackets&) = delete;
	GroupFinderPackets& operator=(const GroupFinderPackets&) = delete;

	void Apply();

	// Called when the client drops back to the glue screen. Everything here is per-session,
	// including the activity version, which the server hands out as a plain counter.
	void Reset();

private:
	GroupFinderPackets() = default;

	struct ActivityGroup
	{
		uint16_t id = 0;
		uint8_t categoryId = 0;
		std::string name;
		uint16_t orderIndex = 0;
	};

	struct Activity
	{
		uint16_t id = 0;
		uint8_t categoryId = 0;
		uint16_t groupId = 0;
		std::string fullName;
		std::string shortName;
		uint8_t minLevel = 0;
		uint8_t maxPlayers = 5;
		uint16_t itemLevelSuggestion = 0;
		uint16_t mapId = 0;
		uint8_t difficultyId = 0;
		uint16_t orderIndex = 0;
		uint32_t flags = 0;
	};

	struct SearchResult
	{
		uint32_t searchResultId = 0;
		uint16_t activityId = 0;
		std::string name;
		std::string comment;
		std::string leaderName;
		uint8_t leaderSubClass = 0;
		uint16_t requiredItemLevel = 0;
		uint8_t numMembers = 0;
		uint8_t maxMembers = 0;
		uint8_t numTanks = 0;
		uint8_t numHealers = 0;
		uint8_t numDamage = 0;
		std::vector<std::pair<uint8_t, uint8_t>> subClassTally;
		uint8_t autoAccept = 0;
		uint8_t hasSelf = 0;
		uint32_t ageSeconds = 0;
		uint8_t applicationStatus = 0;
	};

	struct ApplicantMember
	{
		std::string name;
		uint8_t level = 0;
		uint8_t subClass = 0;
		uint8_t roleMask = 0;
		uint8_t assignedRole = 0;
		uint16_t itemLevel = 0;
	};

	struct Applicant
	{
		uint32_t applicantId = 0;
		uint8_t status = 0;
		std::string comment;
		std::vector<ApplicantMember> members;
	};

	struct Application
	{
		uint32_t searchResultId = 0;
		uint8_t status = 0;
	};

	struct ActiveEntry
	{
		bool valid = false;
		uint32_t listingId = 0;
		uint16_t activityId = 0;
		std::string title;
		std::string comment;
		uint16_t requiredItemLevel = 0;
		uint8_t autoAccept = 0;
		uint32_t ageSeconds = 0;
		uint32_t numApplications = 0;
	};

	static void ReadSearchResult(class Packet& r, SearchResult& out);
	static void ReadApplicant(class Packet& r, Applicant& out);

	const Activity* FindActivity(uint16_t activityId) const;
	const SearchResult* FindSearchResult(uint32_t searchResultId) const;
	void SetApplicationStatus(uint32_t searchResultId, uint8_t status);

	static void Handler_SMSG_LFG_LIST_ACTIVITY_CACHE(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_ACTIVE_ENTRY(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_ENTRY_RESULT(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_SEARCH_RESULTS(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_SEARCH_RESULT_UPDATE(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_APPLICATION_UPDATE(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_APPLICANT_LIST(void*, uint32_t, uint32_t, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_APPLICANT_UPDATE(void*, uint32_t, uint32_t, CDataStore* pkt);

	// Static activity data
	static int RequestActivities(lua_State* L);
	static int GetActivityVersion(lua_State* L);
	static int GetNumActivityGroups(lua_State* L);
	static int GetActivityGroupInfo(lua_State* L);
	static int GetNumActivities(lua_State* L);
	static int GetActivityInfo(lua_State* L);
	static int GetActivityInfoByID(lua_State* L);

	// Our own listing
	static int CreateListing(lua_State* L);
	static int UpdateListing(lua_State* L);
	static int RemoveListing(lua_State* L);
	static int GetActiveEntryInfo(lua_State* L);

	// Searching
	static int Search(lua_State* L);
	static int GetNumSearchResults(lua_State* L);
	static int GetSearchResultInfo(lua_State* L);
	static int GetSearchResultMemberCounts(lua_State* L);
	static int GetNumSearchResultSubClasses(lua_State* L);
	static int GetSearchResultSubClassInfo(lua_State* L);

	// Applying
	static int ApplyToGroup(lua_State* L);
	static int CancelApplication(lua_State* L);
	static int GetApplicationStatus(lua_State* L);
	static int GetNumApplications(lua_State* L);
	static int GetApplicationInfo(lua_State* L);

	// Leader side
	static int RefreshApplicants(lua_State* L);
	static int GetNumApplicants(lua_State* L);
	static int GetApplicantInfo(lua_State* L);
	static int GetApplicantMemberInfo(lua_State* L);
	static int InviteApplicant(lua_State* L);
	static int DeclineApplicant(lua_State* L);

	// Applicant side
	static int AnswerInvite(lua_State* L);

	uint32_t m_activityVersion = 0;
	bool m_hasActivities = false;
	std::vector<ActivityGroup> m_activityGroups;
	std::vector<Activity> m_activities;

	ActiveEntry m_activeEntry;
	std::vector<SearchResult> m_searchResults;
	std::vector<Application> m_applications;
	std::vector<Applicant> m_applicants;
};

#define sGroupFinderPackets GroupFinderPackets::Instance()
