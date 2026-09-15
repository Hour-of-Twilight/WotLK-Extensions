#include "ChatTags.h"

#include <Logger.h>
#include <Windows.h>
#include <cstring>

namespace ChatTags
{
	namespace
	{
		constexpr uintptr_t TagChainStart = 0x0050C06D;
		constexpr uintptr_t TagChainResume = 0x0050C073;
		constexpr uintptr_t TagChainEnd = 0x0050C0A6;

		constexpr uint8_t ExpectedPrologue[] = { 0x8A, 0x4D, 0xFE, 0xF6, 0xC1, 0x10 };

		const char DiscordTag[] = "DISCORD";

		uint8_t* _cave = nullptr;

		void WriteUInt32(uint8_t* at, uint32_t value)
		{
			std::memcpy(at, &value, sizeof(value));
		}

		bool BuildCave()
		{
			_cave = static_cast<uint8_t*>(VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
			if (!_cave)
				return false;

			uint8_t code[] = {
				0x8A, 0x4D, 0xFE,                               // mov  cl, [ebp-2]
				0xF6, 0xC1, 0x20,                               // test cl, 20h
				0x74, 0x11,                                     // jz   stock (+17 -> offset 25)
				0xF6, 0xC1, 0x14,                               // test cl, 14h     ; GM or DEV wins
				0x75, 0x0C,                                     // jnz  stock (+12 -> offset 25)
				0xC7, 0x45, 0xD8, 0x00, 0x00, 0x00, 0x00,       // mov  [ebp-28h], offset DiscordTag
				0xE9, 0x00, 0x00, 0x00, 0x00,                   // jmp  0050C0A6
				0xF6, 0xC1, 0x10,                               // stock: test cl, 10h
				0xE9, 0x00, 0x00, 0x00, 0x00                    // jmp  0050C073
			};

			WriteUInt32(code + 16, reinterpret_cast<uint32_t>(DiscordTag));
			WriteUInt32(code + 21, uint32_t(TagChainEnd - (reinterpret_cast<uintptr_t>(_cave) + 25)));
			WriteUInt32(code + 29, uint32_t(TagChainResume - (reinterpret_cast<uintptr_t>(_cave) + 33)));

			std::memcpy(_cave, code, sizeof(code));

			DWORD oldProtect;
			if (!VirtualProtect(_cave, 64, PAGE_EXECUTE_READ, &oldProtect))
			{
				VirtualFree(_cave, 0, MEM_RELEASE);
				_cave = nullptr;
				return false;
			}

			return true;
		}
	}

	bool Apply()
	{
		uint8_t* chain = reinterpret_cast<uint8_t*>(TagChainStart);
		if (std::memcmp(chain, ExpectedPrologue, sizeof(ExpectedPrologue)) != 0)
		{
			LOG_ERROR << "ChatTags: unexpected bytes at the chat tag chain, the Discord tag was not applied";
			return false;
		}

		if (!BuildCave())
		{
			LOG_ERROR << "ChatTags: could not allocate the chat tag cave";
			return false;
		}

		uint8_t patch[6] = { 0xE9, 0x00, 0x00, 0x00, 0x00, 0x90 };
		WriteUInt32(patch + 1, uint32_t(reinterpret_cast<uintptr_t>(_cave) - (TagChainStart + 5)));

		DWORD oldProtect;
		if (!VirtualProtect(chain, sizeof(patch), PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			LOG_ERROR << "ChatTags: could not unprotect the chat tag chain";
			return false;
		}

		std::memcpy(chain, patch, sizeof(patch));
		VirtualProtect(chain, sizeof(patch), oldProtect, &oldProtect);
		FlushInstructionCache(GetCurrentProcess(), chain, sizeof(patch));

		return true;
	}
}
