#include <ClientDetours.h>
#include <Macros.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace
{
	constexpr uintptr_t GLYPH_PAGE_UPLOAD_BUFFER = 0xC7D328;
	constexpr size_t GLYPH_PAGE_UPLOAD_BUFFER_SIZE = 256 * 256 * 2;
	constexpr uint32_t GLYPH_PAGE_WIDTH = 256;
	constexpr int GXTEX_COMMAND_LATCH = 1;
	constexpr int MAX_GEOMETRY_SETTLE_PASSES = 4;
	constexpr uintptr_t STRING_PAGES_USED_OFFSET = 0x60;
	constexpr uintptr_t STRING_PAGE_EVICTED_OFFSET = 0x64;
	constexpr uintptr_t STRING_LINE_COUNT_OFFSET = 0xB0;
	constexpr uintptr_t STRING_IDLE_TICKS_OFFSET = 0xD4;
	constexpr uintptr_t BATCH_STRING_LINK_OFFSET_OFFSET = 0x1C;
	constexpr uintptr_t BATCH_FIRST_STRING_OFFSET = 0x24;

	struct TSLink
	{
		TSLink* prevLink;
		void* next;
	};

	struct GlyphRow
	{
		uint32_t widestFreeSlot;
		uint32_t linkOffset;
		TSLink terminator;
	};

	struct GlyphDesc
	{
		uint8_t unused0[0x30];
		uint32_t startPixel;
		uint32_t endPixel;
	};

	static_assert(sizeof(GlyphRow) == 0x10, "TEXTURECACHEROW layout");
	static_assert(offsetof(GlyphDesc, startPixel) == 0x30, "CHARCODEDESC layout");

	CLIENT_FUNCTION(CGxString__CreateGeometry, 0x006C7B10, __thiscall, void, (void*))

	template <typename T>
	T& Field(void* object, uintptr_t offset)
	{
		return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
	}

	bool IsListNode(void const* pointer)
	{
		return pointer != nullptr && (reinterpret_cast<uintptr_t>(pointer) & 1) == 0;
	}

	void* NextListNode(void* node, uint32_t linkOffset)
	{
		return Field<TSLink>(node, linkOffset).next;
	}

	uint32_t ComputeWidestFreeSlot(GlyphRow* row)
	{
		uint32_t widest = 0;
		uint32_t cursor = 0;
		for (void* node = row->terminator.next; IsListNode(node); node = NextListNode(node, row->linkOffset))
		{
			auto* desc = static_cast<GlyphDesc*>(node);
			if (desc->startPixel > cursor && desc->startPixel - cursor > widest)
				widest = desc->startPixel - cursor;
			cursor = desc->endPixel + 1;
		}
		if (GLYPH_PAGE_WIDTH > cursor && GLYPH_PAGE_WIDTH - cursor > widest)
			widest = GLYPH_PAGE_WIDTH - cursor;
		return widest;
	}

	CLIENT_DETOUR_THISCALL(TEXTURECACHEROW__CreateNewDesc, 0x006C5120, void*, (void* glyphData, int rowNumber, int cellHeight))
	{
		auto* row = static_cast<GlyphRow*>(self);
		row->widestFreeSlot = ComputeWidestFreeSlot(row);
		return TEXTURECACHEROW__CreateNewDesc(self, glyphData, rowNumber, cellHeight);
	}

	CLIENT_DETOUR_THISCALL_NOARGS(CGxString__ClearInstanceData, 0x006C6B90, void)
	{
		CGxString__ClearInstanceData(self);
		Field<uint32_t>(self, STRING_PAGES_USED_OFFSET) = 0;
		Field<uint32_t>(self, STRING_PAGE_EVICTED_OFFSET) = 0;
	}

	bool RebuildGeometry(void* string)
	{
		if (Field<uint32_t>(string, STRING_PAGE_EVICTED_OFFSET) != 0)
			CGxString__ClearInstanceDataDetour(string, nullptr);
		CGxString__CreateGeometry(string);
		Field<uint32_t>(string, STRING_PAGE_EVICTED_OFFSET) = 0;
		Field<uint32_t>(string, STRING_IDLE_TICKS_OFFSET) = 0;
		return Field<uint32_t>(string, STRING_LINE_COUNT_OFFSET) != 0;
	}

	CLIENT_DETOUR_THISCALL_NOARGS(CGxString__CheckGeometry, 0x006C7480, bool)
	{
		return RebuildGeometry(self);
	}

	CLIENT_DETOUR_THISCALL_NOARGS(BATCHEDRENDERFONTDESC__RenderBatch, 0x006C4AD0, void)
	{
		uint32_t linkOffset = Field<uint32_t>(self, BATCH_STRING_LINK_OFFSET_OFFSET);
		for (int pass = 0; pass < MAX_GEOMETRY_SETTLE_PASSES; ++pass)
		{
			bool rebuilt = false;
			for (void* string = Field<void*>(self, BATCH_FIRST_STRING_OFFSET); IsListNode(string); string = NextListNode(string, linkOffset))
			{
				bool evicted = Field<uint32_t>(string, STRING_PAGE_EVICTED_OFFSET) != 0;
				if (!evicted && pass > 0)
					continue;
				RebuildGeometry(string);
				rebuilt = rebuilt || evicted;
			}
			if (!rebuilt)
				break;
		}
		BATCHEDRENDERFONTDESC__RenderBatch(self);
	}

	CLIENT_DETOUR(TextureCallback, 0x006C9F50, __cdecl, void, (int command, int width, int height, int face, int level, void* userArg, uint32_t* stride, void** texels))
	{
		if (command == GXTEX_COMMAND_LATCH)
			std::memset(reinterpret_cast<void*>(GLYPH_PAGE_UPLOAD_BUFFER), 0, GLYPH_PAGE_UPLOAD_BUFFER_SIZE);
		TextureCallback(command, width, height, face, level, userArg, stride, texels);
	}
}
