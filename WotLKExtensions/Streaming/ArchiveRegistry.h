#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace Streaming
{
	struct ArchiveMount
	{
		std::wstring key;
		std::string openName;
		int priority = 0;
		int flags = 0;
		void* archive = nullptr;
		bool writable = false;
	};

	// Every archive the process has opened through SFile__OpenArchive, keyed by lowercased
	// absolute path, with the SArchive wrapper the client holds on to. Write access is taken by
	// swapping the raw mopaq handle inside that wrapper, so the client's own bookkeeping never
	// sees the archive close.
	class ArchiveRegistry
	{
	public:
		static ArchiveRegistry& Instance();

		ArchiveRegistry(const ArchiveRegistry&) = delete;
		ArchiveRegistry& operator=(const ArchiveRegistry&) = delete;

		// Attaches the open/close detours. Must run inside the DllMain detour transaction, before
		// the client opens its data archives.
		void Install();

		void Record(const char* openName, int priority, int flags, void* archive);
		void Forget(void* archive);

		bool Find(const std::wstring& absPath, ArchiveMount& out);
		size_t Count();

		bool EnsureWritable(const std::wstring& absPath, std::string* error = nullptr);
		bool Commit(const std::wstring& absPath, std::string* error = nullptr);

		static std::wstring KeyFor(const std::string& openName);
		static std::wstring KeyFor(const std::wstring& path);

	private:
		ArchiveRegistry() = default;

		bool Reopen(const std::wstring& absPath, bool writable, std::string* error);

		std::mutex m_mutex;
		std::vector<ArchiveMount> m_mounts;
	};
}

#define sArchiveRegistry Streaming::ArchiveRegistry::Instance()
