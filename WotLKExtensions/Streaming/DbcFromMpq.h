#pragma once

namespace DbcFromMpq
{
	struct RefreshResult
	{
		bool spellDataChanged = false;
		bool interfaceFiles = false;
	};

	RefreshResult RefreshFromArchive(void* hMpq);
}
