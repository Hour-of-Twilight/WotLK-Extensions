#pragma once

#include <Defines.h>

#include <ClientData/ClientFunctions.h>
#include <ClientData/ObjectFields.h>
#include <ClientData/WowObjects.h>

#include <cstdint>

namespace ClientData::Containers
{
	constexpr int kBackpackFirstIndex = 23;
	constexpr int kBackpackLastIndex = 38;
	constexpr int kBankFirstIndex = 39;
	constexpr int kBankLastIndex = 66;
	constexpr int kKeyringFirstIndex = 86;
	constexpr int kKeyringLastIndex = 117;
	constexpr int kMaxEquippedBags = 4;
	constexpr int kMaxContainers = 10;

	inline bool IsItemLocked(void* item)
	{
		return WowItem(item).IsLocked();
	}

	inline void* GetBackpack()
	{
		return WowPlayer::Active().Backpack();
	}

	inline void* GetBag(CGObject_C* container)
	{
		return WowContainer(container).Bag();
	}

	inline uint32 GetNumSlots(CGObject_C* container)
	{
		return WowContainer(container).NumSlots();
	}

	inline void* GetLuaContainerItem(int luaBag, int luaSlot)
	{
		if (luaSlot < 1)
			return nullptr;

		int index = luaSlot - 1;
		void* container = nullptr;

		if (luaBag <= 0)
		{
			container = GetBackpack();
			if (!container)
				return nullptr;

			switch (luaBag)
			{
			case 0:
				index += kBackpackFirstIndex;
				if (index > kBackpackLastIndex)
					return nullptr;
				break;
			case -1:
				index += kBankFirstIndex;
				if (index > kBankLastIndex)
					return nullptr;
				break;
			case -2:
				index += kKeyringFirstIndex;
				if (index > kKeyringLastIndex)
					return nullptr;
				break;
			default:
				return nullptr;
			}
		}
		else
		{
			if (luaBag > kMaxContainers)
				return nullptr;

			WowContainer bag = WowContainer::FromGuid(CGContainerInfo::GetContainer(luaBag - 1));
			if (!bag || index >= static_cast<int>(bag.NumSlots()))
				return nullptr;

			container = bag.Bag();
			if (!container)
				return nullptr;
		}

		return CGBag_C::GetItemPointer(container, index);
	}
}

namespace ClientData
{
	inline WowItem WowItem::FromBagSlot(int luaBag, int luaSlot)
	{
		return WowItem(Containers::GetLuaContainerItem(luaBag, luaSlot));
	}
}
