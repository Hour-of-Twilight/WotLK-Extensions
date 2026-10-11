#include <Defines.h>
#include <ClientDetours.h>
#include <Lua/LuaRuntime.h>
#include <Macros.h>

#include <vector>

namespace FrameStrata
{
	namespace Offsets
	{
		constexpr uintptr_t TopFramesLinkOffset = 0xCCC;
		constexpr uintptr_t TopFramesHead = 0xCD4;
		constexpr uintptr_t TopStrata = 0xCE4;
		constexpr uintptr_t StrataTopLevel = 0x8;
		constexpr uintptr_t StrataLevelCount = 0x10;
		constexpr uintptr_t StrataLevelData = 0x14;
		constexpr uintptr_t NodeFramesHead = 0x14;
		constexpr uintptr_t FrameStrataIndex = 0xD0;
		constexpr uintptr_t FrameLevel = 0xD4;
	}

	CLIENT_FUNCTION(CSimpleFrame__SetFrameLevel, 0x004910A0, __thiscall, uint32, (void* frame, int level, int recursive))

	static std::vector<uint8> s_used;
	static std::vector<uint32> s_remap;
	static std::vector<uint32> s_bucketStart;
	static std::vector<uintptr_t> s_frames;
	static std::vector<uintptr_t> s_sorted;

	static uint32 Read(uintptr_t address)
	{
		return *reinterpret_cast<uint32 const*>(address);
	}

	static void Write(uintptr_t address, uint32 value)
	{
		*reinterpret_cast<uint32*>(address) = value;
	}

	static bool IsEnd(uint32 link)
	{
		return link == 0 || (link & 1) != 0;
	}

	static void CompressStrata(uintptr_t top, uint32 strataIndex)
	{
		if (strataIndex > 8)
			return;
		uintptr_t strata = Read(top + Offsets::TopStrata + 4 * strataIndex);
		if (!strata)
			return;
		uint32 topLevel = Read(strata + Offsets::StrataTopLevel);
		if (topLevel == 0)
			return;
		uint32 levelCount = Read(strata + Offsets::StrataLevelCount);
		uintptr_t levelData = Read(strata + Offsets::StrataLevelData);

		s_used.assign(topLevel, 0);
		for (uint32 level = 0; level < topLevel && level < levelCount; ++level)
		{
			uintptr_t node = Read(levelData + 4 * level);
			if (node && !IsEnd(Read(node + Offsets::NodeFramesHead)))
				s_used[level] = 1;
		}

		s_frames.clear();
		uint32 linkOffset = Read(top + Offsets::TopFramesLinkOffset);
		for (uint32 frame = Read(top + Offsets::TopFramesHead); !IsEnd(frame); frame = Read(frame + linkOffset + 4))
		{
			if (Read(frame + Offsets::FrameStrataIndex) != strataIndex)
				continue;
			uint32 level = Read(frame + Offsets::FrameLevel);
			if (level >= topLevel)
				continue;
			s_used[level] = 1;
			s_frames.push_back(frame);
		}

		s_remap.resize(topLevel);
		uint32 used = 0;
		for (uint32 level = 0; level < topLevel; ++level)
		{
			s_remap[level] = used;
			if (s_used[level])
				++used;
		}
		if (used == topLevel)
			return;

		s_bucketStart.assign(topLevel + 1, 0);
		for (uintptr_t frame : s_frames)
			++s_bucketStart[Read(frame + Offsets::FrameLevel) + 1];
		for (uint32 level = 0; level < topLevel; ++level)
			s_bucketStart[level + 1] += s_bucketStart[level];
		s_sorted.resize(s_frames.size());
		for (uintptr_t frame : s_frames)
			s_sorted[s_bucketStart[Read(frame + Offsets::FrameLevel)]++] = frame;

		for (uintptr_t frame : s_sorted)
		{
			uint32 level = Read(frame + Offsets::FrameLevel);
			uint32 target = s_remap[level];
			if (target != level)
				CSimpleFrame__SetFrameLevel(reinterpret_cast<void*>(frame), (int)target, 0);
		}
		Write(strata + Offsets::StrataTopLevel, used);
	}

	CLIENT_DETOUR_THISCALL(CSimpleTop_CompressStrata, 0x00495060, void, (uint32 strataIndex))
	{
		bool stats = sLuaRuntime.StatsEnabled();
		int64 start = stats ? LuaRuntime::Now() : 0;
		CompressStrata(reinterpret_cast<uintptr_t>(self), strataIndex);
		if (stats)
		{
			LuaRuntime::FrameStats& cur = sLuaRuntime.Cur();
			cur.compressMs += LuaRuntime::ToMs(LuaRuntime::Now() - start);
			++cur.compresses;
		}
	}

	CLIENT_DETOUR_THISCALL(CFrameStrata_TestFrameOcclusion, 0x00494D20, int, (void* frame))
	{
		if (!sLuaRuntime.StatsEnabled())
			return CFrameStrata_TestFrameOcclusion(self, frame);

		int64 start = LuaRuntime::Now();
		int result = CFrameStrata_TestFrameOcclusion(self, frame);
		sLuaRuntime.Cur().occludeMs += LuaRuntime::ToMs(LuaRuntime::Now() - start);
		return result;
	}

	CLIENT_DETOUR_THISCALL(CSimpleTop_RaiseFrame, 0x004951A0, int, (void* frame, int testOcclusion))
	{
		if (!sLuaRuntime.StatsEnabled())
			return CSimpleTop_RaiseFrame(self, frame, testOcclusion);

		int64 start = LuaRuntime::Now();
		int result = CSimpleTop_RaiseFrame(self, frame, testOcclusion);
		LuaRuntime::FrameStats& cur = sLuaRuntime.Cur();
		cur.raiseMs += LuaRuntime::ToMs(LuaRuntime::Now() - start);
		++cur.raises;
		return result;
	}
}
