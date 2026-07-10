#include "Util.h"
#include <SharedDefines.h>
#include <Logger.h>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>

void Util::SetByteAtAddress(void* address, uint8_t byte)
{
	DWORD flOldProtect;
	VirtualProtect(address, 0x1, PAGE_EXECUTE_READWRITE, &flOldProtect);
	memset(address, byte, 0x1);
	VirtualProtect(address, 0x1, flOldProtect, &flOldProtect);
}

void Util::OverwriteBytesAtAddress(void* address, uint8_t byte, size_t numRepeats)
{
	DWORD flOldProtect;
	VirtualProtect(address, numRepeats, PAGE_EXECUTE_READWRITE, &flOldProtect);
	memset(address, byte, numRepeats);
	VirtualProtect(address, numRepeats, flOldProtect, &flOldProtect);
}

void Util::OverwriteBytesAtAddress(uint32_t address, uint8_t byteArray[], size_t arraySize)
{
	void* vAddress = (void*)address;
	for (size_t i = 0; i < arraySize; i++)
		SetByteAtAddress((void*)(address + i), byteArray[i]);
}

void Util::OverwriteUInt32AtAddress(uint32_t address, uint32_t newVal)
{
	DWORD flOldProtect;
	void* vAddress = (void*)address;
	VirtualProtect(vAddress, sizeof(uint32_t), PAGE_EXECUTE_READWRITE, &flOldProtect);
	*(uint32_t*)address = newVal;
	VirtualProtect(vAddress, sizeof(uint32_t), flOldProtect, &flOldProtect);
}

void Util::PercToScreenPos(float x, float y, float* resX, float* resY)
{
	float g_UITexCoordAlphaMultiplier1 = *(float*)0x00AC0CB4;
	float g_UITexCoordAlphaMultiplier3 = *(float*)0x00AC0CBC;
	*resX = (x * (g_UITexCoordAlphaMultiplier3 * 1024.f)) / g_UITexCoordAlphaMultiplier1;
	*resY = (y * (g_UITexCoordAlphaMultiplier3 * 1024.f)) / g_UITexCoordAlphaMultiplier1;
}

uint32_t Util::CalculateAddress(uint32_t address1, uint32_t address2, bool backwards)
{
	if (!backwards)
		return address1 - address2;
	else
		return address2 - address1;
}

fs::path Util::GetExeDir()
{
	char buffer[MAX_PATH];
	GetModuleFileNameA(nullptr, buffer, MAX_PATH);
	return fs::path(buffer).parent_path();
}

bool Util::IsWine()
{
	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

static bool s_debugOutput = false;

void Util::SetDebugOutput(bool enabled)
{
	s_debugOutput = enabled;
}

void Util::DebugOutput(const char* fmt, ...)
{
	if (!s_debugOutput)
		return;

	va_list args;
	va_start(args, fmt);

	va_list copy;
	va_copy(copy, args);
	int len = std::vsnprintf(nullptr, 0, fmt, copy);
	va_end(copy);

	if (len < 0)
	{
		va_end(args);
		return;
	}

	char* buffer = static_cast<char*>(std::malloc(len + 1));
	if (!buffer)
	{
		va_end(args);
		return;
	}

	std::vsnprintf(buffer, len + 1, fmt, args);
	va_end(args);
	sLog.Write("DEBUG", "Util", buffer);
	std::free(buffer);
}
