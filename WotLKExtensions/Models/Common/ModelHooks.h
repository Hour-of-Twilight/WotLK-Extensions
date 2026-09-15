#pragma once

#include "Models/Common/ModelLog.h"

#include <ClientDetours.h>

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <source_location>

namespace ModernM2
{
	inline constexpr bool kEnabled = true;

	template <class Fn>
	inline bool HookAttach(const char* name, uintptr_t target, Fn* detour, Fn** original,
	    const std::source_location& where = std::source_location::current())
	{
		if (!target || !detour || !original)
			return false;
		*original = reinterpret_cast<Fn*>(target);
		ClientDetours::Add(name, original, reinterpret_cast<void*>(detour), where.file_name(), where.line());
		return true;
	}

	bool ConfigRaw(const char* name, char* buf, size_t cap);

	inline bool ConfigTruthy(const char* raw, bool fallback)
	{
		if (!raw || !*raw)
			return fallback;
		const char c = raw[0];
		if (c == '0' || c == 'n' || c == 'N' || c == 'f' || c == 'F')
			return false;
		return true;
	}

	inline bool ConfigFlag(const char* envName, const char* disableFile)
	{
		char value[16] = {};
		if (ConfigRaw(envName, value, sizeof value) && !ConfigTruthy(value, true))
			return false;
		if (disableFile && GetFileAttributesA(disableFile) != INVALID_FILE_ATTRIBUTES)
			return false;
		return true;
	}

	inline uint64_t ConfigU64(const char* name, uint64_t fallback, uint64_t minValue, uint64_t maxValue)
	{
		char value[32] = {};
		if (!ConfigRaw(name, value, sizeof value))
			return fallback;
		char* end = nullptr;
		const uint64_t parsed = std::strtoull(value, &end, 10);
		if (end == value)
			return fallback;
		if (parsed < minValue)
			return minValue;
		if (parsed > maxValue)
			return maxValue;
		return parsed;
	}

	inline uint32_t ConfigU32(const char* name, uint32_t fallback, uint32_t minValue, uint32_t maxValue)
	{
		return static_cast<uint32_t>(ConfigU64(name, fallback, minValue, maxValue));
	}

	inline uint32_t ConfigBytesMbKb(const char* envMb, const char* envKb, uint32_t defBytes, uint32_t minKb, uint32_t maxKb)
	{
		char value[32] = {};
		if (ConfigRaw(envMb, value, sizeof value))
		{
			char* end = nullptr;
			const uint64_t mb = std::strtoull(value, &end, 10);
			const uint64_t kb = mb * 1024ull;
			if (end != value && kb >= minKb && kb <= maxKb)
				return static_cast<uint32_t>(kb * 1024ull);
		}
		if (ConfigRaw(envKb, value, sizeof value))
		{
			char* end = nullptr;
			const uint64_t kb = std::strtoull(value, &end, 10);
			if (end != value && kb >= minKb && kb <= maxKb)
				return static_cast<uint32_t>(kb * 1024ull);
		}
		return defBytes;
	}

	const char* ResolveTexture(uint32_t fileDataId);
}
