#pragma once

#include <Defines.h>

#include <Macros.h>

#include <cstdint>

namespace ClientData::Party
{
	constexpr uint32 kSlots = 4;
	CLIENT_ADDRESS(uint64, s_memberGuids, 0x00BD1948)
	CLIENT_ADDRESS(uint64, s_leaderGuid, 0x00BD1968)

	CLIENT_FUNCTION(IsMember, 0x0052C680, __cdecl, int, (const uint64* guid))
	CLIENT_FUNCTION(NumMembers, 0x0052BD10, __cdecl, int, ())
}

namespace ClientData::Raid
{
	constexpr uint32 kMaxMembers = 40;
	constexpr uint32 kMemberGuid = 0x000;
	constexpr uint32 kMemberSubgroup = 0x008;
	constexpr uint32 kMemberRank = 0x00C;
	CLIENT_ADDRESS(uint8*, s_members, 0x00BEB568)
	CLIENT_ADDRESS(uint32, s_numMembers, 0x00BEB608)

	CLIENT_FUNCTION(GetMemberIndexByGuid, 0x00608130, __cdecl, int, (uint32 guidLow, uint32 guidHigh))
	CLIENT_FUNCTION(GetMember, 0x004FB0D0, __cdecl, uint8*, (uint32 index))

	inline uint32 Count()
	{
		uint32 count = *s_numMembers;
		return count < kMaxMembers ? count : kMaxMembers;
	}

	inline uint64 MemberGuid(uint32 index)
	{
		const uint8* member = s_members[index];
		return member ? *reinterpret_cast<const uint64*>(member + kMemberGuid) : 0;
	}
}

namespace ClientData::Friends
{
	constexpr uint32 kMaxFriends = 100;
	constexpr uint32 kFriendStride = 0x220;
	constexpr uint32 kFriendGuid = 0x208;
	constexpr uint32 kFriendStatus = 0x001;
	CLIENT_ADDRESS(uint8*, s_friendList, 0x00C79F98)

	CLIENT_FUNCTION(IsFriend, 0x006B3920, __thiscall, char, (void* self, uint32 guidLow, uint32 guidHigh))
	CLIENT_FUNCTION(GetFriend, 0x006B3510, __thiscall, uint8*, (void* self, uint64 guid))

	inline uint8* List()
	{
		return *s_friendList;
	}

	inline uint64 FriendGuid(const uint8* list, uint32 index)
	{
		return *reinterpret_cast<const uint64*>(list + index * kFriendStride + kFriendGuid);
	}
}
