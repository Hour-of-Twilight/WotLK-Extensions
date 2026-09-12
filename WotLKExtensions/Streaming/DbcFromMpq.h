#pragma once

#include <string>
#include <vector>

namespace DbcFromMpq
{
	struct RefreshResult
	{
		bool spellDataChanged = false;
		bool achievementDataChanged = false;
		bool interfaceFiles = false;
		bool artFiles = false;
	};

	// Every name a mounted archive holds, from its (hotmanifest) or, failing that, its (listfile).
	void CollectArchiveNames(void* hMpq, std::vector<std::string>& out);

	// Re-applies each named DBC by reading it through the client's normal lookup, so whichever
	// archive now wins for that name is what lands in memory. Flags UI and art names as well.
	RefreshResult RefreshNames(const std::vector<std::string>& names);
}
