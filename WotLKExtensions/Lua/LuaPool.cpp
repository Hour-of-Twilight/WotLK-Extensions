#include "LuaPool.h"

#include <cstring>

LuaPool::LuaPool()
{
	static_assert(offsetof(LuaPool, m_magic) == 0, "the magic has to lead the pool so LuaPool::Is can tell it from the client's pool array");

	uint32 cls = 0;
	for (uint32 size = 16; size <= 256; size += 16)
		m_classes[cls++] = { nullptr, size };
	for (uint32 size = 288; size <= 512; size += 32)
		m_classes[cls++] = { nullptr, size };
	for (uint32 size = 576; size <= 1024; size += 64)
		m_classes[cls++] = { nullptr, size };
	for (uint32 size = 1152; size <= 2048; size += 128)
		m_classes[cls++] = { nullptr, size };

	uint32 current = 0;
	for (uint32 bucket = 0; bucket <= (SmallLimit >> 4); ++bucket)
	{
		while (m_classes[current].size < bucket * 16)
			++current;
		m_classOf[bucket] = static_cast<uint8>(current);
	}

	m_heap = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);
}

LuaPool::~LuaPool()
{
	for (void* slab : m_slabs)
		VirtualFree(slab, 0, MEM_RELEASE);
	if (m_heap)
		HeapDestroy(m_heap);
}

void* __cdecl LuaPool::Realloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
	return static_cast<LuaPool*>(ud)->Reallocate(ptr, osize, nsize);
}

void* LuaPool::Reallocate(void* ptr, size_t osize, size_t nsize)
{
	if (nsize == 0)
	{
		if (ptr && osize)
		{
			Free(ptr, osize);
			++m_freeCalls;
			m_live -= osize;
		}
		return nullptr;
	}

	++m_allocCalls;
	m_bytesAllocated += nsize;

	if (!ptr || osize == 0)
	{
		void* block = Allocate(nsize);
		if (block)
			m_live += nsize;
		return block;
	}

	bool oldSmall = osize <= SmallLimit;
	bool newSmall = nsize <= SmallLimit;

	if (oldSmall && newSmall && ClassOf(osize) == ClassOf(nsize))
	{
		m_live += nsize - osize;
		return ptr;
	}

	if (!oldSmall && !newSmall)
	{
		void* block = HeapReAlloc(m_heap, 0, ptr, nsize);
		if (block)
			m_live += nsize - osize;
		return block;
	}

	void* block = Allocate(nsize);
	if (!block)
		return nullptr;
	memcpy(block, ptr, osize < nsize ? osize : nsize);
	Free(ptr, osize);
	m_live += nsize - osize;
	return block;
}

void* LuaPool::Allocate(size_t size)
{
	if (size > SmallLimit)
		return HeapAlloc(m_heap, 0, size);
	return AllocateSmall(ClassOf(size));
}

void LuaPool::Free(void* ptr, size_t size)
{
	if (size > SmallLimit)
	{
		HeapFree(m_heap, 0, ptr);
		return;
	}

	SizeClass& cls = m_classes[ClassOf(size)];
	FreeBlock* block = static_cast<FreeBlock*>(ptr);
	block->next = cls.free;
	cls.free = block;
}

void* LuaPool::AllocateSmall(uint32 cls)
{
	SizeClass& c = m_classes[cls];
	if (!c.free && !Refill(cls))
		return nullptr;

	FreeBlock* block = c.free;
	c.free = block->next;
	return block;
}

bool LuaPool::Refill(uint32 cls)
{
	void* slab = VirtualAlloc(nullptr, SlabSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!slab)
		return false;
	m_slabs.push_back(slab);

	SizeClass& c = m_classes[cls];
	size_t count = SlabSize / c.size;
	char* base = static_cast<char*>(slab);
	FreeBlock* head = c.free;
	for (size_t i = count; i-- > 0;)
	{
		FreeBlock* block = reinterpret_cast<FreeBlock*>(base + i * c.size);
		block->next = head;
		head = block;
	}
	c.free = head;
	return true;
}
