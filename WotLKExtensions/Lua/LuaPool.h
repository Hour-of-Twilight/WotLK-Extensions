#pragma once

#include <Defines.h>

#include <Windows.h>
#include <vector>

class LuaPool
{
public:
	static constexpr uint32 Magic = 0x4C554131;

	static bool Is(void const* ud)
	{
		return ud && *static_cast<uint32 const*>(ud) == Magic;
	}

	static void* __cdecl Realloc(void* ud, void* ptr, size_t osize, size_t nsize);

	LuaPool();
	~LuaPool();
	LuaPool(const LuaPool&) = delete;
	LuaPool& operator=(const LuaPool&) = delete;

	void* Reallocate(void* ptr, size_t osize, size_t nsize);

	uint64 BytesAllocated() const
	{
		return m_bytesAllocated;
	}
	uint64 AllocCalls() const
	{
		return m_allocCalls;
	}
	uint64 FreeCalls() const
	{
		return m_freeCalls;
	}
	size_t LiveBytes() const
	{
		return m_live;
	}
	size_t SlabBytes() const
	{
		return m_slabs.size() * SlabSize;
	}

private:
	static constexpr size_t SmallLimit = 2048;
	static constexpr size_t SlabSize = 256 * 1024;
	static constexpr uint32 ClassCount = 40;

	struct FreeBlock
	{
		FreeBlock* next;
	};

	struct SizeClass
	{
		FreeBlock* free;
		uint32 size;
	};

	uint32 ClassOf(size_t size) const
	{
		return m_classOf[(size + 15) >> 4];
	}

	void* Allocate(size_t size);
	void Free(void* ptr, size_t size);
	void* AllocateSmall(uint32 cls);
	bool Refill(uint32 cls);

	uint32 m_magic = Magic;
	SizeClass m_classes[ClassCount] = {};
	uint8 m_classOf[(SmallLimit >> 4) + 1] = {};
	std::vector<void*> m_slabs;
	HANDLE m_heap = nullptr;
	uint64 m_bytesAllocated = 0;
	uint64 m_allocCalls = 0;
	uint64 m_freeCalls = 0;
	size_t m_live = 0;
};
