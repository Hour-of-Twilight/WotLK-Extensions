#include <Defines.h>
#include <ClientDetours.h>
#include <Lua/LuaRuntime.h>

#include <algorithm>
#include <vector>

namespace ResizeList
{
	namespace Offsets
	{
		constexpr uintptr_t PendingLink = 0x4;
		constexpr uintptr_t Points = 0xC;
		constexpr uintptr_t AnchoredHead = 0x38;
		constexpr uintptr_t Flags = 0x40;
		constexpr uintptr_t PointRelative = 0x8;
		constexpr uintptr_t PointFlags = 0xC;
		constexpr uintptr_t PendingTerminator = 0xAC101C;
	}

	constexpr uint32 PointUnused = 0x800;
	constexpr uint32 PointCount = 9;
	constexpr uint8 RetryBudget = 6;

	static std::vector<uintptr_t> s_candidates;

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

	static bool DependsOn(uintptr_t frame, uintptr_t target)
	{
		for (uint32 i = 0; i < PointCount; ++i)
		{
			uintptr_t point = Read(frame + Offsets::Points + 4 * i);
			if (point && (Read(point + Offsets::PointFlags) & PointUnused) == 0 && Read(point + Offsets::PointRelative) == target)
				return true;
		}
		return false;
	}

	static void InsertBefore(uintptr_t frame, uintptr_t before)
	{
		uintptr_t link = frame + Offsets::PendingLink;
		uintptr_t prevLink = Read(before + Offsets::PendingLink);
		Write(link, prevLink);
		Write(link + 4, Read(prevLink + 4));
		Write(prevLink + 4, frame);
		Write(before + Offsets::PendingLink, link);
	}

	static void Append(uintptr_t frame)
	{
		uintptr_t link = frame + Offsets::PendingLink;
		uintptr_t tailLink = Read(Offsets::PendingTerminator);
		Write(link, tailLink);
		Write(link + 4, Read(tailLink + 4));
		Write(tailLink + 4, frame);
		Write(Offsets::PendingTerminator, link);
	}

	static void AddToResizeList(uintptr_t frame)
	{
		if (Read(frame + Offsets::PendingLink + 4) != 0)
			return;

		s_candidates.clear();
		for (uint32 node = Read(frame + Offsets::AnchoredHead); !IsEnd(node); node = Read(node + 4))
		{
			uintptr_t anchored = Read(node + 8);
			if (anchored && Read(anchored + Offsets::PendingLink + 4) != 0 && DependsOn(anchored, frame))
				s_candidates.push_back(anchored);
		}

		uintptr_t before = 0;
		if (s_candidates.size() == 1)
			before = s_candidates[0];
		else if (s_candidates.size() > 1)
		{
			for (uintptr_t link = Read(Offsets::PendingTerminator); link && link != Offsets::PendingTerminator; link = Read(link))
			{
				uintptr_t candidate = link - Offsets::PendingLink;
				if (std::find(s_candidates.begin(), s_candidates.end(), candidate) != s_candidates.end())
				{
					before = candidate;
					break;
				}
			}
		}

		if (before)
			InsertBefore(frame, before);
		else
			Append(frame);
		*reinterpret_cast<uint8*>(frame + Offsets::Flags) = RetryBudget;
	}

	CLIENT_DETOUR_THISCALL_NOARGS(CLayoutFrame_AddToResizeList, 0x00489710, void*)
	{
		if (!self)
			return CLayoutFrame_AddToResizeList(self);

		bool stats = sLuaRuntime.StatsEnabled();
		int64 start = stats ? LuaRuntime::Now() : 0;
		AddToResizeList(reinterpret_cast<uintptr_t>(self));
		if (stats)
		{
			LuaRuntime::FrameStats& cur = sLuaRuntime.Cur();
			cur.addResizeMs += LuaRuntime::ToMs(LuaRuntime::Now() - start);
			++cur.addResizes;
		}
		return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(self) + Offsets::PendingLink);
	}
}
