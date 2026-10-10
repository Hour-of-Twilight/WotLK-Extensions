#include <Defines.h>
#include <Streaming/LooseFiles.h>

#include <Logger.h>
#include <Macros.h>

#include <cstdint>

namespace LooseFiles
{
	namespace
	{
		CLIENT_ADDRESS(uint32, s_directAccessFlags, 0x00AB7B80)
		CLIENT_ADDRESS(uint8, s_looseIndexBuilt, 0x00B324A4)
		constexpr uint32 kLookupByPath = 1;
	}

	void Apply()
	{
		*s_directAccessFlags |= kLookupByPath;
		LOG_INFO << "loose files: direct access enabled, files under the game folder take precedence over archive content";
	}

	void Invalidate()
	{
		*s_looseIndexBuilt = 0;
	}

	bool IsEnabled()
	{
		return (*s_directAccessFlags & kLookupByPath) != 0;
	}
}
