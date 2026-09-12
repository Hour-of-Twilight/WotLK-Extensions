#include "ArchiveRegistry.h"

#include "TextConv.h"

#include <ClientData/Streaming.h>
#include <Helpers/Util.h>

#include <windows.h>
#include <detours.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace Streaming
{
	namespace
	{
		using OpenArchiveFn = int(__stdcall*)(const char*, int, int, void**);
		using CloseArchiveFn = char(__stdcall*)(void*);

		OpenArchiveFn s_origOpen = nullptr;
		CloseArchiveFn s_origClose = nullptr;

		int __stdcall OpenArchiveHook(const char* name, int priority, int flags, void** out)
		{
			int r = s_origOpen(name, priority, flags, out);
			if (r != 0 && name && out && *out)
				ArchiveRegistry::Instance().Record(name, priority, flags, *out);
			return r;
		}

		char __stdcall CloseArchiveHook(void* archive)
		{
			ArchiveRegistry::Instance().Forget(archive);
			return s_origClose(archive);
		}

		bool Fail(std::string* error, const std::string& what)
		{
			if (error)
				*error = what;
			return false;
		}
	}

	ArchiveRegistry& ArchiveRegistry::Instance()
	{
		static ArchiveRegistry instance;
		return instance;
	}

	void ArchiveRegistry::Install()
	{
		if (s_origOpen)
			return;
		s_origOpen = reinterpret_cast<OpenArchiveFn>(0x00421950);
		s_origClose = reinterpret_cast<CloseArchiveFn>(0x00421720);
		DetourAttach(&(PVOID&)s_origOpen, OpenArchiveHook);
		DetourAttach(&(PVOID&)s_origClose, CloseArchiveHook);
	}

	std::wstring ArchiveRegistry::KeyFor(const std::string& openName)
	{
		return KeyFor(Text::WidenAcp(openName));
	}

	std::wstring ArchiveRegistry::KeyFor(const std::wstring& path)
	{
		fs::path p(path);
		if (p.is_relative())
			p = Util::GetExeDir() / p;
		return Text::LowerPath(p.lexically_normal().wstring());
	}

	void ArchiveRegistry::Record(const char* openName, int priority, int flags, void* archive)
	{
		ArchiveMount m;
		m.key = KeyFor(std::string(openName));
		m.openName = openName;
		m.priority = priority;
		m.flags = flags;
		m.archive = archive;

		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto it = m_mounts.begin(); it != m_mounts.end();)
			it = (it->archive == archive || it->key == m.key) ? m_mounts.erase(it) : it + 1;
		m_mounts.push_back(std::move(m));
	}

	void ArchiveRegistry::Forget(void* archive)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto it = m_mounts.begin(); it != m_mounts.end();)
			it = (it->archive == archive) ? m_mounts.erase(it) : it + 1;
	}

	bool ArchiveRegistry::Find(const std::wstring& absPath, ArchiveMount& out)
	{
		const std::wstring key = KeyFor(absPath);
		std::lock_guard<std::mutex> lock(m_mutex);
		for (const ArchiveMount& m : m_mounts)
			if (m.key == key)
			{
				out = m;
				return true;
			}
		return false;
	}

	size_t ArchiveRegistry::Count()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_mounts.size();
	}

	bool ArchiveRegistry::Reopen(const std::wstring& absPath, bool writable, std::string* error)
	{
		ArchiveMount m;
		if (!Find(absPath, m))
			return Fail(error, "archive is not mounted");
		if (m.writable == writable)
			return true;

		using namespace ClientData::Streaming;
		void* raw = RawArchiveHandle(m.archive);
		if (raw)
			MopaqCloseArchive(raw);
		SetRawArchiveHandle(m.archive, nullptr);

		const int wanted = writable ? (m.flags | ARCHIVE_OPEN_WRITABLE) : m.flags;
		void* fresh = nullptr;
		bool ok = MopaqOpenArchive(m.openName.c_str(), m.priority, (uint32_t)wanted, &fresh) != 0 && fresh;
		if (!ok)
		{
			fresh = nullptr;
			bool restored = MopaqOpenArchive(m.openName.c_str(), m.priority, (uint32_t)m.flags, &fresh) != 0 && fresh;
			SetRawArchiveHandle(m.archive, restored ? fresh : nullptr);
			std::lock_guard<std::mutex> lock(m_mutex);
			for (ArchiveMount& e : m_mounts)
				if (e.key == m.key)
					e.writable = false;
			return Fail(error, restored ? (writable ? "cannot reopen archive for writing" : "cannot reopen archive read-only")
			                            : "archive reopen failed and the read-only handle could not be restored");
		}

		SetRawArchiveHandle(m.archive, fresh);
		std::lock_guard<std::mutex> lock(m_mutex);
		for (ArchiveMount& e : m_mounts)
			if (e.key == m.key)
				e.writable = writable;
		return true;
	}

	bool ArchiveRegistry::EnsureWritable(const std::wstring& absPath, std::string* error)
	{
		return Reopen(absPath, true, error);
	}

	bool ArchiveRegistry::Commit(const std::wstring& absPath, std::string* error)
	{
		return Reopen(absPath, false, error);
	}
}
