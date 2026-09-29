#include "DBCacheQueries.h"

#include <Logger.h>
#include <Util.h>

namespace DBCacheQueries
{
	static constexpr uint32_t FIRST_CACHE = 0x00C5D690;
	static constexpr uint32_t CACHE_STRIDE = 0x88;
	static constexpr uint32_t CACHE_COUNT = 15;

	static constexpr uint32_t OFFSET_QUERIES_PER_WINDOW = 0x48;
	static constexpr uint32_t OFFSET_QUERIES_SENT = 0x4C;
	static constexpr uint32_t OFFSET_WINDOW_EXPIRY = 0x50;

	static constexpr uint32_t UNTHROTTLED = 0;

	static const char* const CACHE_NAMES[CACHE_COUNT] = {
		"creaturecache",
		"gameobjectcache",
		"itemnamecache",
		"itemcache",
		"npccache",
		"namecache",
		"guildcache",
		"questcache",
		"pagetextcache",
		"petnamecache",
		"petitioncache",
		"itemtextcache",
		"wowcache",
		"arenateamcache",
		"dancecache"
	};

	void Apply()
	{
		for (uint32_t i = 0; i < CACHE_COUNT; ++i)
		{
			const uint32_t base = FIRST_CACHE + i * CACHE_STRIDE;
			const uint32_t perWindow = *reinterpret_cast<uint32_t*>(base + OFFSET_QUERIES_PER_WINDOW);
			if (perWindow == UNTHROTTLED)
				continue;

			Util::OverwriteValue<uint32_t>(base + OFFSET_QUERIES_PER_WINDOW, UNTHROTTLED);
			Util::OverwriteValue<uint32_t>(base + OFFSET_QUERIES_SENT, 0);
			Util::OverwriteValue<uint32_t>(base + OFFSET_WINDOW_EXPIRY, 0);

			LOG_INFO << "DBCacheQueries: " << CACHE_NAMES[i] << ".wdb was capped at " << perWindow
			         << " queries per 30s window, now unthrottled";
		}
	}
}
