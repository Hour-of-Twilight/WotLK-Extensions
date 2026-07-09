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

//@todo swap out this temporary ini for a proper config as this is fucking retarded.
fs::path Util::GetIniPath(bool useCustomConfig)
{
	if (useCustomConfig)
		return GetExeDir() / "WTF" / "customconfig.ini";
	return GetExeDir() / "WTF" / "tempvars.ini";
}

std::unordered_map<std::string, std::string> Util::LoadIni(bool useCustomConfig)
{
	std::unordered_map<std::string, std::string> data;
	std::ifstream file(GetIniPath(useCustomConfig));

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == ';' || line[0] == '#')
			continue;

		size_t eqPos = line.find('=');
		if (eqPos != std::string::npos)
		{
			std::string key = line.substr(0, eqPos);
			std::string value = line.substr(eqPos + 1);
			data[key] = value;
		}
	}

	return data;
}

void Util::SaveIni(const std::unordered_map<std::string, std::string>& data, bool useCustomConfig)
{
	fs::create_directories(GetIniPath(useCustomConfig).parent_path());
	std::ofstream file(GetIniPath(useCustomConfig));
	for (const auto& [key, value] : data)
	{
		file << key << "=" << value << "\n";
	}
}

std::string Util::ReadIniValue(const std::string& key, const std::string& defaultValue, bool useCustomConfig)
{
	auto data = LoadIni(useCustomConfig);
	auto it = data.find(key);
	return (it != data.end()) ? it->second : defaultValue;
}

void Util::WriteIniValue(const std::string& key, const std::string& value, bool useCustomConfig)
{
	auto data = LoadIni(useCustomConfig);
	data[key] = value;
	SaveIni(data, useCustomConfig);
}

void Util::DebugOutput(const char* fmt, ...)
{
	if (Util::ReadIniValue("debugOutput", "0", true) == "1")
	{
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
}
