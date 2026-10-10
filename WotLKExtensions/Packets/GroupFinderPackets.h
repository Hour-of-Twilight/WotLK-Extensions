#pragma once

#include <Defines.h>

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
		uint16 id = 0;
		uint8 categoryId = 0;
		std::string name;
		uint16 orderIndex = 0;
	};

	struct Activity
	{
		uint16 id = 0;
		uint8 categoryId = 0;
		uint16 groupId = 0;
		std::string fullName;
		std::string shortName;
		uint8 minLevel = 0;
		uint8 maxPlayers = 5;
		uint16 itemLevelSuggestion = 0;
		uint16 mapId = 0;
		uint8 difficultyId = 0;
		uint16 orderIndex = 0;
		uint32 flags = 0;
	};

	struct SearchResult
	{
		uint32 searchResultId = 0;
		uint16 activityId = 0;
		std::string name;
		std::string comment;
		std::string leaderName;
		uint8 leaderSubClass = 0;
		uint16 requiredItemLevel = 0;
		uint8 numMembers = 0;
		uint8 maxMembers = 0;
		uint8 numTanks = 0;
		uint8 numHealers = 0;
		uint8 numDamage = 0;
		std::vector<std::pair<uint8, uint8>> subClassTally;
		uint8 autoAccept = 0;
		uint8 hasSelf = 0;
		uint32 ageSeconds = 0;
		uint8 applicationStatus = 0;
	};

	struct ApplicantMember
	{
		std::string name;
		uint8 level = 0;
		uint8 subClass = 0;
		uint8 roleMask = 0;
		uint8 assignedRole = 0;
		uint16 itemLevel = 0;
	};

	struct Applicant
	{
		uint32 applicantId = 0;
		uint8 status = 0;
		std::string comment;
		std::vector<ApplicantMember> members;
	};

	struct Application
	{
		uint32 searchResultId = 0;
		uint8 status = 0;
	};

	struct ActiveEntry
	{
		bool valid = false;
		uint32 listingId = 0;
		uint16 activityId = 0;
		std::string title;
		std::string comment;
		uint16 requiredItemLevel = 0;
		uint8 autoAccept = 0;
		uint32 ageSeconds = 0;
		uint32 numApplications = 0;
	};

	static void ReadSearchResult(class Packet& r, SearchResult& out);
	static void ReadApplicant(class Packet& r, Applicant& out);

	const Activity* FindActivity(uint16 activityId) const;
	const SearchResult* FindSearchResult(uint32 searchResultId) const;
	void SetApplicationStatus(uint32 searchResultId, uint8 status);

	static void Handler_SMSG_LFG_LIST_ACTIVITY_CACHE(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_ACTIVE_ENTRY(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_ENTRY_RESULT(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_SEARCH_RESULTS(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_SEARCH_RESULT_UPDATE(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_APPLICATION_UPDATE(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_APPLICANT_LIST(void*, uint32, uint32, CDataStore* pkt);
	static void Handler_SMSG_LFG_LIST_APPLICANT_UPDATE(void*, uint32, uint32, CDataStore* pkt);

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

	uint32 m_activityVersion = 0;
	bool m_hasActivities = false;
	std::vector<ActivityGroup> m_activityGroups;
	std::vector<Activity> m_activities;

	ActiveEntry m_activeEntry;
	std::vector<SearchResult> m_searchResults;
	std::vector<Application> m_applications;
	std::vector<Applicant> m_applicants;
};

#define sGroupFinderPackets GroupFinderPackets::Instance()
