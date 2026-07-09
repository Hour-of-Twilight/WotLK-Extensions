#pragma once

#include <Macros.h>

#include <Windows.h>
#include <functional>
#include <filesystem>
namespace fs = std::filesystem;
class Util
{
public:
	static void SetByteAtAddress(void* address, uint8_t byte);
	static void OverwriteBytesAtAddress(uint32_t address, uint8_t byteArray[], size_t arraySize);
	static void OverwriteBytesAtAddress(void* address, uint8_t byte, size_t numRepeats);
	static void OverwriteUInt32AtAddress(uint32_t address, uint32_t newVal);
	template <typename T>
	static void OverwriteValue(uint32_t address, const T& value)
	{
		DWORD oldProtect;
		void* ptr = reinterpret_cast<void*>(address);
		VirtualProtect(ptr, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect);
		memcpy(ptr, &value, sizeof(T));
		VirtualProtect(ptr, sizeof(T), oldProtect, &oldProtect);
	}

	static void PercToScreenPos(float x, float y, float* resX, float* resY);
	static uint32_t CalculateAddress(uint32_t address1, uint32_t address2, bool backwards = false);
	static fs::path GetIniPath(bool useCustomConfig);
	static std::unordered_map<std::string, std::string> LoadIni(bool useCustomConfig);
	static void SaveIni(const std::unordered_map<std::string, std::string>& data, bool useCustomConfig);
	static std::string ReadIniValue(const std::string& key, const std::string& defaultValue, bool useCustomConfig);
	static void WriteIniValue(const std::string& key, const std::string& value, bool useCustomConfig);
	static const char* GetCVarValue(void* cvar)
	{
		return *(const char**)((uintptr_t)cvar + 0x28);
	}
	static void DebugOutput(const char* fmt, ...);
	static fs::path GetExeDir();
	static bool IsWine();
};
