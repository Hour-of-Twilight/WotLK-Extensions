#pragma once

#include <ClientData/ClientFunctions.h>
#include <ClientData/Object.h>
#include <ClientData/ObjectFields.h>
#include <ClientData/ObjectManager.h>

#include <cstdint>

namespace ClientData::Containers
{
	constexpr uint32_t kContainerFieldNumSlots = 64;
	constexpr uint32_t kPlayerBackpackOffset = 0x18F0;
	constexpr int kBackpackFirstIndex = 23;
	constexpr int kBackpackLastIndex = 38;
	constexpr int kBankFirstIndex = 39;
	constexpr int kBankLastIndex = 66;
	constexpr int kKeyringFirstIndex = 86;
	constexpr int kKeyringLastIndex = 117;
	constexpr int kMaxEquippedBags = 4;
	constexpr int kMaxContainers = 10;
	constexpr uint32_t kBagVTableSlot = 10;
	constexpr uint32_t kItemLockFlagOffset = 0x394;

	inline bool IsItemLocked(void* item)
	{
		return (*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(item) + kItemLockFlagOffset) & 1) != 0;
	}

	inline void* GetBackpack()
	{
		CGObject_C* player = ObjectManager::GetActivePlayerObject();
		if (!player)
			return nullptr;

		return reinterpret_cast<uint8_t*>(player) + kPlayerBackpackOffset;
	}

	inline void* GetBag(CGObject_C* container)
	{
		typedef void*(__fastcall * GetBagFn)(void* self, void* edx);
		void** vtable = *reinterpret_cast<void***>(container);
		return reinterpret_cast<GetBagFn>(vtable[kBagVTableSlot])(container, nullptr);
	}

	inline uint32_t GetNumSlots(CGObject_C* container)
	{
		return container->GetValue<uint32_t>(kContainerFieldNumSlots);
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

			uint64_t bagGuid = CGContainerInfo::GetContainer(luaBag - 1);
			if (!bagGuid)
				return nullptr;

			CGObject_C* bagObject = ObjectManager::ObjectPtr(bagGuid, TYPEMASK_CONTAINER);
			if (!bagObject)
				return nullptr;

			if (index >= static_cast<int>(GetNumSlots(bagObject)))
				return nullptr;

			container = GetBag(bagObject);
			if (!container)
				return nullptr;
		}

		return CGBag_C::GetItemPointer(container, index);
	}
}
