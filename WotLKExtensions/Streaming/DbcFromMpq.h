#pragma once

namespace DbcFromMpq
{
	struct RefreshResult
	{
		bool spellDataChanged = false;
		bool achievementDataChanged = false;
		bool interfaceFiles = false;
	};

	RefreshResult RefreshFromArchive(void* hMpq);
}
