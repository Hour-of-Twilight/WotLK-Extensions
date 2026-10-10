#pragma once

#include <Defines.h>

#include <Macros.h>

#include <Windows.h>
#include <filesystem>
namespace fs = std::filesystem;
class Util
{
public:
	static void SetByteAtAddress(void* address, uint8 byte);
	static void OverwriteBytesAtAddress(uint32 address, uint8 byteArray[], size_t arraySize);
	static void OverwriteBytesAtAddress(void* address, uint8 byte, size_t numRepeats);
	static void OverwriteUInt32AtAddress(uint32 address, uint32 newVal);
	template <typename T>
	static void OverwriteValue(uint32 address, const T& value)
	{
		DWORD oldProtect;
		void* ptr = reinterpret_cast<void*>(address);
		VirtualProtect(ptr, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect);
		memcpy(ptr, &value, sizeof(T));
		VirtualProtect(ptr, sizeof(T), oldProtect, &oldProtect);
	}

	static void PercToScreenPos(float x, float y, float* resX, float* resY);
	static uint32 CalculateAddress(uint32 address1, uint32 address2, bool backwards = false);
	static void SetDebugOutput(bool enabled);
	static void DebugOutput(const char* fmt, ...);
	static fs::path GetExeDir();
	static bool IsWine();
};
