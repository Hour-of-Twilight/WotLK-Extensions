#include "GroupFinderPackets.h"
#include "Packet.h"
#include <CustomPacket.h>
#include <CustomLua.h>
#include <XMLExtensions.h>
#include <SharedDefines.h>

// Long enough for the server side limits (title 63, comment 191) plus the terminator.
static constexpr uint32_t GF_STRING_BUFFER = 256;

static const char* LuaOptString(lua_State* L, int index)
{
	if (FrameScript::GetTop(L) < index)
		return "";

	const char* value = FrameScript::ToLString(L, index, false);
	return value ? value : "";
}

static uint32_t LuaOptNumber(lua_State* L, int index, uint32_t fallback = 0)
{
	if (FrameScript::GetTop(L) < index || !FrameScript::IsNumber(L, index))
		return fallback;

	return static_cast<uint32_t>(FrameScript::GetNumber(L, index));
}

GroupFinderPackets& GroupFinderPackets::Instance()
{
	static GroupFinderPackets instance;
	return instance;
}

const GroupFinderPackets::Activity* GroupFinderPackets::FindActivity(uint16_t activityId) const
{
	for (const Activity& activity : m_activities)
		if (activity.id == activityId)
			return &activity;

	return nullptr;
}

const GroupFinderPackets::SearchResult* GroupFinderPackets::FindSearchResult(uint32_t searchResultId) const
{
	for (const SearchResult& result : m_searchResults)
		if (result.searchResultId == searchResultId)
			return &result;

	return nullptr;
}

void GroupFinderPackets::SetApplicationStatus(uint32_t searchResultId, uint8_t status)
{
	// Status 0 means the application is gone, so drop it rather than keeping a dead row.
	for (size_t i = 0; i < m_applications.size(); ++i)
	{
		if (m_applications[i].searchResultId != searchResultId)
			continue;

		if (status == 0)
			m_applications.erase(m_applications.begin() + i);
		else
			m_applications[i].status = status;

		return;
	}

	if (status == 0)
		return;

	Application application;
	application.searchResultId = searchResultId;
	application.status = status;
	m_applications.push_back(application);
}

void GroupFinderPackets::ReadSearchResult(Packet& r, SearchResult& out)
{
	char buffer[GF_STRING_BUFFER];

	out.searchResultId = r.GetUInt32();
	out.activityId = r.GetUInt16();

	r.GetString(buffer, sizeof(buffer));
	out.name = buffer;
	r.GetString(buffer, sizeof(buffer));
	out.comment = buffer;
	r.GetString(buffer, sizeof(buffer));
	out.leaderName = buffer;

	out.leaderSubClass = r.GetUInt8();
	out.requiredItemLevel = r.GetUInt16();
	out.numMembers = r.GetUInt8();
	out.maxMembers = r.GetUInt8();
	out.numTanks = r.GetUInt8();
	out.numHealers = r.GetUInt8();
	out.numDamage = r.GetUInt8();

	uint8_t tallyCount = r.GetUInt8();
	out.subClassTally.clear();
	out.subClassTally.reserve(tallyCount);
	for (uint8_t i = 0; i < tallyCount; ++i)
	{
		uint8_t subClass = r.GetUInt8();
		uint8_t count = r.GetUInt8();
		out.subClassTally.emplace_back(subClass, count);
	}

	out.autoAccept = r.GetUInt8();
	out.hasSelf = r.GetUInt8();
	out.ageSeconds = r.GetUInt32();
	out.applicationStatus = r.GetUInt8();
}

void GroupFinderPackets::ReadApplicant(Packet& r, Applicant& out)
{
	char buffer[GF_STRING_BUFFER];

	out.applicantId = r.GetUInt32();
	out.status = r.GetUInt8();
	r.GetString(buffer, sizeof(buffer));
	out.comment = buffer;

	uint8_t memberCount = r.GetUInt8();
	out.members.clear();
	out.members.reserve(memberCount);
	for (uint8_t i = 0; i < memberCount; ++i)
	{
		ApplicantMember member;
		r.GetString(buffer, sizeof(buffer));
		member.name = buffer;
		member.level = r.GetUInt8();
		member.subClass = r.GetUInt8();
		member.roleMask = r.GetUInt8();
		member.assignedRole = r.GetUInt8();
		member.itemLevel = r.GetUInt16();
		out.members.push_back(std::move(member));
	}
}

void GroupFinderPackets::Handler_SMSG_LFG_LIST_ACTIVITY_CACHE(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	char buffer[GF_STRING_BUFFER];

	GroupFinderPackets& self = Instance();
	self.m_activityVersion = r.GetUInt32();
	self.m_activityGroups.clear();
	self.m_activities.clear();

	uint32_t groupCount = r.GetUInt32();
	self.m_activityGroups.reserve(groupCount);
	for (uint32_t i = 0; i < groupCount; ++i)
	{
		ActivityGroup group;
		group.id = r.GetUInt16();
		group.categoryId = r.GetUInt8();
		r.GetString(buffer, sizeof(buffer));
		group.name = buffer;
		group.orderIndex = r.GetUInt16();
		self.m_activityGroups.push_back(std::move(group));
	}

	uint32_t activityCount = r.GetUInt32();
	self.m_activities.reserve(activityCount);
	for (uint32_t i = 0; i < activityCount; ++i)
	{
		Activity activity;
		activity.id = r.GetUInt16();
		activity.categoryId = r.GetUInt8();
		activity.groupId = r.GetUInt16();
		r.GetString(buffer, sizeof(buffer));
		activity.fullName = buffer;
		r.GetString(buffer, sizeof(buffer));
		activity.shortName = buffer;
		activity.minLevel = r.GetUInt8();
		activity.maxPlayers = r.GetUInt8();
		activity.itemLevelSuggestion = r.GetUInt16();
		activity.mapId = r.GetUInt16();
		activity.difficultyId = r.GetUInt8();
		activity.orderIndex = r.GetUInt16();
		activity.flags = r.GetUInt32();
		self.m_activities.push_back(std::move(activity));
	}

	self.m_hasActivities = true;
	FrameXMLExtensions::SignalEvent("HOT_LFG_LIST_ACTIVITIES_UPDATE", "");
}

void GroupFinderPackets::Handler_SMSG_LFG_LIST_ACTIVE_ENTRY(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	char buffer[GF_STRING_BUFFER];

	GroupFinderPackets& self = Instance();
	self.m_activeEntry = ActiveEntry();

	if (r.GetUInt8())
	{
		self.m_activeEntry.valid = true;
		self.m_activeEntry.listingId = r.GetUInt32();
		self.m_activeEntry.activityId = r.GetUInt16();
		r.GetString(buffer, sizeof(buffer));
		self.m_activeEntry.title = buffer;
		r.GetString(buffer, sizeof(buffer));
		self.m_activeEntry.comment = buffer;
		self.m_activeEntry.requiredItemLevel = r.GetUInt16();
		self.m_activeEntry.autoAccept = r.GetUInt8();
		self.m_activeEntry.ageSeconds = r.GetUInt32();
		self.m_activeEntry.numApplications = r.GetUInt32();
	}
	else
	{
		// Our listing is gone, so the applicant list it fed goes with it.
		self.m_applicants.clear();
	}

	FrameXMLExtensions::SignalEvent("HOT_LFG_LIST_ACTIVE_ENTRY_UPDATE", "");
}

void GroupFinderPackets::Handler_SMSG_LFG_LIST_ENTRY_RESULT(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	uint8_t result = Packet(pkt).GetUInt8();
	FrameXMLExtensions::SignalEvent("HOT_LFG_LIST_ENTRY_RESULT", "%u", (uint32_t)result);
}

void GroupFinderPackets::Handler_SMSG_LFG_LIST_SEARCH_RESULTS(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);

	GroupFinderPackets& self = Instance();
	uint32_t count = r.GetUInt32();
	self.m_searchResults.clear();
	self.m_searchResults.reserve(count);

	for (uint32_t i = 0; i < count; ++i)
	{
		SearchResult result;
		ReadSearchResult(r, result);
		self.m_searchResults.push_back(std::move(result));
	}

	FrameXMLExtensions::SignalEvent("HOT_LFG_LIST_SEARCH_RESULTS_RECEIVED", "%u", count);
}

void GroupFinderPackets::Handler_SMSG_LFG_LIST_SEARCH_RESULT_UPDATE(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint8_t delisted = r.GetUInt8();

	SearchResult result;
	ReadSearchResult(r, result);

	GroupFinderPackets& self = Instance();
	uint32_t searchResultId = result.searchResultId;

	for (size_t i = 0; i < self.m_searchResults.size(); ++i)
	{
		if (self.m_searchResults[i].searchResultId != searchResultId)
			continue;

		if (delisted)
			self.m_searchResults.erase(self.m_searchResults.begin() + i);
		else
			self.m_searchResults[i] = std::move(result);

		FrameXMLExtensions::SignalEvent("HOT_LFG_LIST_SEARCH_RESULT_UPDATED",
		    "%u%u", searchResultId, (uint32_t)delisted);
		return;
	}

	if (delisted)
		return;

	self.m_searchResults.push_back(std::move(result));
	FrameXMLExtensions::SignalEvent("HOT_LFG_LIST_SEARCH_RESULT_UPDATED",
	    "%u%u", searchResultId, (uint32_t)0);
}

void GroupFinderPackets::Handler_SMSG_LFG_LIST_APPLICATION_UPDATE(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);
	uint32_t searchResultId = r.GetUInt32();
	uint8_t status = r.GetUInt8();
	uint8_t listingRemoved = r.GetUInt8();

	GroupFinderPackets& self = Instance();
	self.SetApplicationStatus(searchResultId, status);

	// Keep the browser row in step so the button state matches without a refresh.
	for (size_t i = 0; i < self.m_searchResults.size(); ++i)
	{
		if (self.m_searchResults[i].searchResultId != searchResultId)
			continue;

		if (listingRemoved)
			self.m_searchResults.erase(self.m_searchResults.begin() + i);
		else
			self.m_searchResults[i].applicationStatus = status;

		break;
	}

	FrameXMLExtensions::SignalEvent("HOT_LFG_LIST_APPLICATION_STATUS_UPDATED",
	    "%u%u%u", searchResultId, (uint32_t)status, (uint32_t)listingRemoved);
}

void GroupFinderPackets::Handler_SMSG_LFG_LIST_APPLICANT_LIST(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);

	GroupFinderPackets& self = Instance();
	uint32_t count = r.GetUInt32();
	self.m_applicants.clear();
	self.m_applicants.reserve(count);

	for (uint32_t i = 0; i < count; ++i)
	{
		Applicant applicant;
		ReadApplicant(r, applicant);
		self.m_applicants.push_back(std::move(applicant));
	}

	FrameXMLExtensions::SignalEvent("HOT_LFG_LIST_APPLICANT_LIST_UPDATED", "%u", count);
}

void GroupFinderPackets::Handler_SMSG_LFG_LIST_APPLICANT_UPDATE(void*, uint32_t, uint32_t, CDataStore* pkt)
{
	Packet r(pkt);

	Applicant applicant;
	ReadApplicant(r, applicant);

	GroupFinderPackets& self = Instance();
	uint32_t applicantId = applicant.applicantId;
	uint32_t status = applicant.status;

	bool found = false;
	for (Applicant& existing : self.m_applicants)
	{
		if (existing.applicantId != applicantId)
			continue;

		existing = std::move(applicant);
		found = true;
		break;
	}

	if (!found)
		self.m_applicants.push_back(std::move(applicant));

	FrameXMLExtensions::SignalEvent("HOT_LFG_LIST_APPLICANT_UPDATED",
	    "%u%u", applicantId, status);
}

// ---------------------------------------------------------------------------
// Static activity data
// ---------------------------------------------------------------------------

int GroupFinderPackets::RequestActivities(lua_State*)
{
	// The server skips the payload when our version already matches.
	Packet(CMSG_LFG_LIST_REQUEST_ACTIVITIES).PutUInt32(Instance().m_activityVersion).Send();
	return 0;
}

int GroupFinderPackets::GetActivityVersion(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_activityVersion);
	return 1;
}

int GroupFinderPackets::GetNumActivityGroups(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_activityGroups.size());
	return 1;
}

// Takes a 1 based index. Returns: id, categoryID, name, orderIndex
int GroupFinderPackets::GetActivityGroupInfo(lua_State* L)
{
	GroupFinderPackets& self = Instance();
	uint32_t index = LuaOptNumber(L, 1);

	if (index == 0 || index > self.m_activityGroups.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const ActivityGroup& group = self.m_activityGroups[index - 1];
	FrameScript::PushNumber(L, group.id);
	FrameScript::PushNumber(L, group.categoryId);
	FrameScript::PushString(L, group.name.c_str());
	FrameScript::PushNumber(L, group.orderIndex);
	return 4;
}

int GroupFinderPackets::GetNumActivities(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_activities.size());
	return 1;
}

// Takes a 1 based index. Returns the same values as GetActivityInfoByID.
int GroupFinderPackets::GetActivityInfo(lua_State* L)
{
	GroupFinderPackets& self = Instance();
	uint32_t index = LuaOptNumber(L, 1);

	if (index == 0 || index > self.m_activities.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const Activity& activity = self.m_activities[index - 1];
	FrameScript::PushNumber(L, activity.id);
	FrameScript::PushNumber(L, activity.categoryId);
	FrameScript::PushNumber(L, activity.groupId);
	FrameScript::PushString(L, activity.fullName.c_str());
	FrameScript::PushString(L, activity.shortName.c_str());
	FrameScript::PushNumber(L, activity.minLevel);
	FrameScript::PushNumber(L, activity.maxPlayers);
	FrameScript::PushNumber(L, activity.itemLevelSuggestion);
	FrameScript::PushNumber(L, activity.mapId);
	FrameScript::PushNumber(L, activity.difficultyId);
	FrameScript::PushNumber(L, activity.orderIndex);
	FrameScript::PushNumber(L, activity.flags);
	return 12;
}

// Returns: id, categoryID, groupID, fullName, shortName, minLevel, maxPlayers,
//          itemLevelSuggestion, mapID, difficultyID, orderIndex, flags
int GroupFinderPackets::GetActivityInfoByID(lua_State* L)
{
	GroupFinderPackets& self = Instance();
	const Activity* activity = self.FindActivity(static_cast<uint16_t>(LuaOptNumber(L, 1)));

	if (!activity)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, activity->id);
	FrameScript::PushNumber(L, activity->categoryId);
	FrameScript::PushNumber(L, activity->groupId);
	FrameScript::PushString(L, activity->fullName.c_str());
	FrameScript::PushString(L, activity->shortName.c_str());
	FrameScript::PushNumber(L, activity->minLevel);
	FrameScript::PushNumber(L, activity->maxPlayers);
	FrameScript::PushNumber(L, activity->itemLevelSuggestion);
	FrameScript::PushNumber(L, activity->mapId);
	FrameScript::PushNumber(L, activity->difficultyId);
	FrameScript::PushNumber(L, activity->orderIndex);
	FrameScript::PushNumber(L, activity->flags);
	return 12;
}

// ---------------------------------------------------------------------------
// Our own listing
// ---------------------------------------------------------------------------

static void SendListing(lua_State* L, uint32_t opcode)
{
	uint16_t activityId = static_cast<uint16_t>(LuaOptNumber(L, 1));
	const char* title = LuaOptString(L, 2);
	const char* comment = LuaOptString(L, 3);
	uint16_t requiredItemLevel = static_cast<uint16_t>(LuaOptNumber(L, 4));
	uint8_t autoAccept = FrameScript::ToBoolean(L, 5) ? 1 : 0;

	Packet(opcode)
	    .PutUInt16(activityId)
	    .PutString(title)
	    .PutString(comment)
	    .PutUInt16(requiredItemLevel)
	    .PutUInt8(autoAccept)
	    .Send();
}

int GroupFinderPackets::CreateListing(lua_State* L)
{
	SendListing(L, CMSG_LFG_LIST_CREATE_LISTING);
	return 0;
}

int GroupFinderPackets::UpdateListing(lua_State* L)
{
	SendListing(L, CMSG_LFG_LIST_UPDATE_LISTING);
	return 0;
}

int GroupFinderPackets::RemoveListing(lua_State*)
{
	Packet(CMSG_LFG_LIST_REMOVE_LISTING).Send();
	return 0;
}

// Returns: listingID, activityID, title, comment, requiredItemLevel,
//          autoAccept, ageSeconds, numApplications
int GroupFinderPackets::GetActiveEntryInfo(lua_State* L)
{
	GroupFinderPackets& self = Instance();
	if (!self.m_activeEntry.valid)
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, self.m_activeEntry.listingId);
	FrameScript::PushNumber(L, self.m_activeEntry.activityId);
	FrameScript::PushString(L, self.m_activeEntry.title.c_str());
	FrameScript::PushString(L, self.m_activeEntry.comment.c_str());
	FrameScript::PushNumber(L, self.m_activeEntry.requiredItemLevel);
	FrameScript::PushBoolean(L, self.m_activeEntry.autoAccept != 0);
	FrameScript::PushNumber(L, self.m_activeEntry.ageSeconds);
	FrameScript::PushNumber(L, self.m_activeEntry.numApplications);
	return 8;
}

// ---------------------------------------------------------------------------
// Searching
// ---------------------------------------------------------------------------

int GroupFinderPackets::Search(lua_State* L)
{
	uint8_t categoryId = static_cast<uint8_t>(LuaOptNumber(L, 1));
	uint16_t activityId = static_cast<uint16_t>(LuaOptNumber(L, 2));
	const char* text = LuaOptString(L, 3);
	uint8_t filterMask = static_cast<uint8_t>(LuaOptNumber(L, 4));

	Packet(CMSG_LFG_LIST_SEARCH)
	    .PutUInt8(categoryId)
	    .PutUInt16(activityId)
	    .PutString(text)
	    .PutUInt8(filterMask)
	    .Send();
	return 0;
}

int GroupFinderPackets::GetNumSearchResults(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_searchResults.size());
	return 1;
}

// Takes a 1 based index. Returns: searchResultID, activityID, name, comment, leaderName,
//   leaderSubClass, requiredItemLevel, ageSeconds, autoAccept, hasSelf, applicationStatus
int GroupFinderPackets::GetSearchResultInfo(lua_State* L)
{
	GroupFinderPackets& self = Instance();
	uint32_t index = LuaOptNumber(L, 1);

	if (index == 0 || index > self.m_searchResults.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const SearchResult& result = self.m_searchResults[index - 1];
	FrameScript::PushNumber(L, result.searchResultId);
	FrameScript::PushNumber(L, result.activityId);
	FrameScript::PushString(L, result.name.c_str());
	FrameScript::PushString(L, result.comment.c_str());
	FrameScript::PushString(L, result.leaderName.c_str());
	FrameScript::PushNumber(L, result.leaderSubClass);
	FrameScript::PushNumber(L, result.requiredItemLevel);
	FrameScript::PushNumber(L, result.ageSeconds);
	FrameScript::PushBoolean(L, result.autoAccept != 0);
	FrameScript::PushBoolean(L, result.hasSelf != 0);
	FrameScript::PushNumber(L, result.applicationStatus);
	return 11;
}

// Takes a 1 based index. Returns: numMembers, maxMembers, numTanks, numHealers, numDamage
int GroupFinderPackets::GetSearchResultMemberCounts(lua_State* L)
{
	GroupFinderPackets& self = Instance();
	uint32_t index = LuaOptNumber(L, 1);

	if (index == 0 || index > self.m_searchResults.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const SearchResult& result = self.m_searchResults[index - 1];
	FrameScript::PushNumber(L, result.numMembers);
	FrameScript::PushNumber(L, result.maxMembers);
	FrameScript::PushNumber(L, result.numTanks);
	FrameScript::PushNumber(L, result.numHealers);
	FrameScript::PushNumber(L, result.numDamage);
	return 5;
}

int GroupFinderPackets::GetNumSearchResultSubClasses(lua_State* L)
{
	GroupFinderPackets& self = Instance();
	uint32_t index = LuaOptNumber(L, 1);

	if (index == 0 || index > self.m_searchResults.size())
	{
		FrameScript::PushNumber(L, 0);
		return 1;
	}

	FrameScript::PushNumber(L, self.m_searchResults[index - 1].subClassTally.size());
	return 1;
}

// Takes a 1 based result index and a 1 based tally index. Returns: subClass, count
int GroupFinderPackets::GetSearchResultSubClassInfo(lua_State* L)
{
	GroupFinderPackets& self = Instance();
	uint32_t index = LuaOptNumber(L, 1);
	uint32_t tallyIndex = LuaOptNumber(L, 2);

	if (index == 0 || index > self.m_searchResults.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const SearchResult& result = self.m_searchResults[index - 1];
	if (tallyIndex == 0 || tallyIndex > result.subClassTally.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, result.subClassTally[tallyIndex - 1].first);
	FrameScript::PushNumber(L, result.subClassTally[tallyIndex - 1].second);
	return 2;
}

// ---------------------------------------------------------------------------
// Applying
// ---------------------------------------------------------------------------

int GroupFinderPackets::ApplyToGroup(lua_State* L)
{
	uint32_t searchResultId = LuaOptNumber(L, 1);
	const char* comment = LuaOptString(L, 2);
	uint8_t roleMask = static_cast<uint8_t>(LuaOptNumber(L, 3));

	Packet(CMSG_LFG_LIST_APPLY)
	    .PutUInt32(searchResultId)
	    .PutString(comment)
	    .PutUInt8(roleMask)
	    .Send();
	return 0;
}

int GroupFinderPackets::CancelApplication(lua_State* L)
{
	Packet(CMSG_LFG_LIST_CANCEL_APPLICATION).PutUInt32(LuaOptNumber(L, 1)).Send();
	return 0;
}

int GroupFinderPackets::GetApplicationStatus(lua_State* L)
{
	GroupFinderPackets& self = Instance();
	uint32_t searchResultId = LuaOptNumber(L, 1);

	for (const Application& application : self.m_applications)
	{
		if (application.searchResultId != searchResultId)
			continue;

		FrameScript::PushNumber(L, application.status);
		return 1;
	}

	FrameScript::PushNumber(L, 0);
	return 1;
}

int GroupFinderPackets::GetNumApplications(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_applications.size());
	return 1;
}

// Takes a 1 based index. Returns: searchResultID, status
int GroupFinderPackets::GetApplicationInfo(lua_State* L)
{
	GroupFinderPackets& self = Instance();
	uint32_t index = LuaOptNumber(L, 1);

	if (index == 0 || index > self.m_applications.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	FrameScript::PushNumber(L, self.m_applications[index - 1].searchResultId);
	FrameScript::PushNumber(L, self.m_applications[index - 1].status);
	return 2;
}

// ---------------------------------------------------------------------------
// Leader side
// ---------------------------------------------------------------------------

int GroupFinderPackets::RefreshApplicants(lua_State*)
{
	Packet(CMSG_LFG_LIST_REFRESH_APPLICANTS).Send();
	return 0;
}

int GroupFinderPackets::GetNumApplicants(lua_State* L)
{
	FrameScript::PushNumber(L, Instance().m_applicants.size());
	return 1;
}

// Takes a 1 based index. Returns: applicantID, status, comment, numMembers
int GroupFinderPackets::GetApplicantInfo(lua_State* L)
{
	GroupFinderPackets& self = Instance();
	uint32_t index = LuaOptNumber(L, 1);

	if (index == 0 || index > self.m_applicants.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const Applicant& applicant = self.m_applicants[index - 1];
	FrameScript::PushNumber(L, applicant.applicantId);
	FrameScript::PushNumber(L, applicant.status);
	FrameScript::PushString(L, applicant.comment.c_str());
	FrameScript::PushNumber(L, applicant.members.size());
	return 4;
}

// Takes a 1 based applicant index and a 1 based member index.
// Returns: name, level, subClass, roleMask, assignedRole, itemLevel
int GroupFinderPackets::GetApplicantMemberInfo(lua_State* L)
{
	GroupFinderPackets& self = Instance();
	uint32_t index = LuaOptNumber(L, 1);
	uint32_t memberIndex = LuaOptNumber(L, 2);

	if (index == 0 || index > self.m_applicants.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const Applicant& applicant = self.m_applicants[index - 1];
	if (memberIndex == 0 || memberIndex > applicant.members.size())
	{
		FrameScript::PushNil(L);
		return 1;
	}

	const ApplicantMember& member = applicant.members[memberIndex - 1];
	FrameScript::PushString(L, member.name.c_str());
	FrameScript::PushNumber(L, member.level);
	FrameScript::PushNumber(L, member.subClass);
	FrameScript::PushNumber(L, member.roleMask);
	FrameScript::PushNumber(L, member.assignedRole);
	FrameScript::PushNumber(L, member.itemLevel);
	return 6;
}

// LFGListInviteApplicant(applicantID [, role1, role2, ...]) - one role per applicant member,
// in the order GetApplicantMemberInfo returns them. Omitted roles keep what they applied as.
int GroupFinderPackets::InviteApplicant(lua_State* L)
{
	uint32_t applicantId = LuaOptNumber(L, 1);

	int top = FrameScript::GetTop(L);
	uint8_t roleCount = top > 1 ? static_cast<uint8_t>(top - 1) : 0;

	Packet packet(CMSG_LFG_LIST_INVITE_APPLICANT);
	packet.PutUInt32(applicantId);
	packet.PutUInt8(roleCount);
	for (uint8_t i = 0; i < roleCount; ++i)
		packet.PutUInt8(static_cast<uint8_t>(LuaOptNumber(L, 2 + i)));

	packet.Send();
	return 0;
}

int GroupFinderPackets::DeclineApplicant(lua_State* L)
{
	Packet(CMSG_LFG_LIST_DECLINE_APPLICANT).PutUInt32(LuaOptNumber(L, 1)).Send();
	return 0;
}

// LFGListAnswerInvite(searchResultID, accepted) - accepting is what joins the group, there is
// no party invite. The id pins which invite is being answered when several are outstanding.
int GroupFinderPackets::AnswerInvite(lua_State* L)
{
	Packet(CMSG_LFG_LIST_ANSWER_INVITE)
	    .PutUInt32(LuaOptNumber(L, 1))
	    .PutUInt8(FrameScript::ToBoolean(L, 2) ? 1 : 0)
	    .Send();
	return 0;
}

void GroupFinderPackets::Reset()
{
	m_activityVersion = 0;
	m_hasActivities = false;
	m_activityGroups.clear();
	m_activities.clear();
	m_activeEntry = ActiveEntry();
	m_searchResults.clear();
	m_applications.clear();
	m_applicants.clear();
}

void GroupFinderPackets::Apply()
{
	sCustomPacket.RegisterHandler(SMSG_LFG_LIST_ACTIVITY_CACHE, &Handler_SMSG_LFG_LIST_ACTIVITY_CACHE);
	sCustomPacket.RegisterHandler(SMSG_LFG_LIST_ACTIVE_ENTRY, &Handler_SMSG_LFG_LIST_ACTIVE_ENTRY);
	sCustomPacket.RegisterHandler(SMSG_LFG_LIST_ENTRY_RESULT, &Handler_SMSG_LFG_LIST_ENTRY_RESULT);
	sCustomPacket.RegisterHandler(SMSG_LFG_LIST_SEARCH_RESULTS, &Handler_SMSG_LFG_LIST_SEARCH_RESULTS);
	sCustomPacket.RegisterHandler(SMSG_LFG_LIST_SEARCH_RESULT_UPDATE, &Handler_SMSG_LFG_LIST_SEARCH_RESULT_UPDATE);
	sCustomPacket.RegisterHandler(SMSG_LFG_LIST_APPLICATION_UPDATE, &Handler_SMSG_LFG_LIST_APPLICATION_UPDATE);
	sCustomPacket.RegisterHandler(SMSG_LFG_LIST_APPLICANT_LIST, &Handler_SMSG_LFG_LIST_APPLICANT_LIST);
	sCustomPacket.RegisterHandler(SMSG_LFG_LIST_APPLICANT_UPDATE, &Handler_SMSG_LFG_LIST_APPLICANT_UPDATE);

	sLua.RegisterFunction("LFGListRequestActivities", &RequestActivities, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetActivityVersion", &GetActivityVersion, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetNumActivityGroups", &GetNumActivityGroups, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetActivityGroupInfo", &GetActivityGroupInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetNumActivities", &GetNumActivities, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetActivityInfo", &GetActivityInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetActivityInfoByID", &GetActivityInfoByID, LuaFunctionState::FRAME);

	sLua.RegisterFunction("LFGListCreateListing", &CreateListing, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListUpdateListing", &UpdateListing, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListRemoveListing", &RemoveListing, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetActiveEntryInfo", &GetActiveEntryInfo, LuaFunctionState::FRAME);

	sLua.RegisterFunction("LFGListSearch", &Search, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetNumSearchResults", &GetNumSearchResults, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetSearchResultInfo", &GetSearchResultInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetSearchResultMemberCounts", &GetSearchResultMemberCounts, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetNumSearchResultSubClasses", &GetNumSearchResultSubClasses, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetSearchResultSubClassInfo", &GetSearchResultSubClassInfo, LuaFunctionState::FRAME);

	sLua.RegisterFunction("LFGListApplyToGroup", &ApplyToGroup, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListCancelApplication", &CancelApplication, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetApplicationStatus", &GetApplicationStatus, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetNumApplications", &GetNumApplications, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetApplicationInfo", &GetApplicationInfo, LuaFunctionState::FRAME);

	sLua.RegisterFunction("LFGListRefreshApplicants", &RefreshApplicants, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetNumApplicants", &GetNumApplicants, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetApplicantInfo", &GetApplicantInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListGetApplicantMemberInfo", &GetApplicantMemberInfo, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListInviteApplicant", &InviteApplicant, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListDeclineApplicant", &DeclineApplicant, LuaFunctionState::FRAME);
	sLua.RegisterFunction("LFGListAnswerInvite", &AnswerInvite, LuaFunctionState::FRAME);
}
