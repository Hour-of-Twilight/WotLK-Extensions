#include "LocalDigests.h"

#include "MpqFile.h"
#include "Sha256.h"
#include "TextConv.h"

#include <Helpers/Util.h>

#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace Streaming::LocalDigests
{
	namespace
	{
		constexpr const char* kHeader = "hotdigests1";
		constexpr const char* kNoContentId = "-";
		constexpr size_t kKinds = 3;

		struct Entry
		{
			long long size = -1;
			long long mtime = -1;
			std::string digests[kKinds];
		};

		std::mutex g_mutex;
		std::mutex g_computeMutex;
		std::mutex g_saveMutex;
		std::map<std::wstring, Entry> g_entries;
		bool g_loaded = false;
		bool g_dirty = false;

		fs::path CacheFile()
		{
			return Util::GetExeDir() / "Cache" / "hotstream-local.txt";
		}

		bool Stat(const std::wstring& path, long long& size, long long& mtime)
		{
			std::error_code ec;
			const auto bytes = fs::file_size(path, ec);
			if (ec)
				return false;
			const auto written = fs::last_write_time(path, ec);
			if (ec)
				return false;
			size = (long long)bytes;
			mtime = (long long)written.time_since_epoch().count();
			return true;
		}

		std::vector<std::string> SplitFields(const std::string& line)
		{
			std::vector<std::string> fields;
			size_t start = 0;
			for (;;)
			{
				const size_t bar = line.find('|', start);
				if (bar == std::string::npos)
				{
					fields.push_back(line.substr(start));
					return fields;
				}
				fields.push_back(line.substr(start, bar - start));
				start = bar + 1;
			}
		}

		Kind LegacyKind(const std::wstring& key, const std::string& digest)
		{
			if (IsQuickDigest(digest))
				return Kind::Quick;
			const bool mpq = key.size() >= 4 && key.compare(key.size() - 4, 4, L".mpq") == 0;
			return mpq ? Kind::ContentId : Kind::Sha256;
		}

		void LoadLocked()
		{
			g_loaded = true;
			std::ifstream in(CacheFile(), std::ios::binary);
			std::string line;
			bool first = true;
			bool legacy = true;
			while (in && std::getline(in, line))
			{
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				if (first)
				{
					first = false;
					if (line == kHeader)
					{
						legacy = false;
						continue;
					}
				}

				const std::vector<std::string> f = SplitFields(line);
				if (f.size() != (legacy ? 4u : 3u + kKinds) || f.back().empty())
					continue;

				const std::wstring key = Text::LowerPath(legacy ? Text::WidenAcp(f.back()) : Text::WidenUtf8(f.back()));
				Entry e;
				e.size = _atoi64(f[0].c_str());
				e.mtime = _atoi64(f[1].c_str());
				if (legacy)
				{
					if (f[2].empty())
						continue;
					e.digests[(size_t)LegacyKind(key, f[2])] = f[2];
					g_dirty = true;
				}
				else
				{
					for (size_t k = 0; k < kKinds; ++k)
						e.digests[k] = f[2 + k];
				}
				g_entries[key] = std::move(e);
			}
		}

		bool Cached(const std::wstring& key, long long size, long long mtime, Kind kind, std::string& out)
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			if (!g_loaded)
				LoadLocked();
			const auto it = g_entries.find(key);
			if (it == g_entries.end() || it->second.size != size || it->second.mtime != mtime)
				return false;
			const std::string& digest = it->second.digests[(size_t)kind];
			if (digest.empty())
				return false;
			out = (digest == kNoContentId) ? std::string() : digest;
			return true;
		}

		void Store(const std::wstring& key, long long size, long long mtime, Kind kind, const std::string& digest)
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			if (!g_loaded)
				LoadLocked();
			Entry& e = g_entries[key];
			if (e.size != size || e.mtime != mtime)
			{
				e = Entry();
				e.size = size;
				e.mtime = mtime;
			}
			const std::string value = (kind == Kind::ContentId && digest.empty()) ? std::string(kNoContentId) : digest;
			if (e.digests[(size_t)kind] == value)
				return;
			e.digests[(size_t)kind] = value;
			g_dirty = true;
		}

		bool Compute(const std::wstring& path, Kind kind, std::string& out)
		{
			switch (kind)
			{
			case Kind::Quick:
				out = QuickDigestFile(path);
				return !out.empty();
			case Kind::Sha256:
				out = Sha256File(path);
				return !out.empty();
			case Kind::ContentId:
				return MpqFile::ContentId(path, out);
			}
			return false;
		}
	}

	bool Get(const std::wstring& path, Kind kind, std::string& out)
	{
		out.clear();
		long long size = 0;
		long long mtime = 0;
		if (!Stat(path, size, mtime))
			return false;

		const std::wstring key = Text::LowerPath(path);
		if (Cached(key, size, mtime, kind, out))
			return true;

		std::lock_guard<std::mutex> computing(g_computeMutex);
		if (Cached(key, size, mtime, kind, out))
			return true;
		if (!Compute(path, kind, out))
		{
			out.clear();
			return false;
		}
		Store(key, size, mtime, kind, out);
		return true;
	}

	void Remember(const std::wstring& path, Kind kind, const std::string& digest)
	{
		long long size = 0;
		long long mtime = 0;
		if (!digest.empty() && Stat(path, size, mtime))
			Store(Text::LowerPath(path), size, mtime, kind, digest);
	}

	void Save()
	{
		std::lock_guard<std::mutex> saving(g_saveMutex);
		std::vector<std::pair<std::wstring, Entry>> snapshot;
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			if (!g_dirty)
				return;
			snapshot.assign(g_entries.begin(), g_entries.end());
			g_dirty = false;
		}

		const fs::path file = CacheFile();
		const fs::path tmp = file.wstring() + L".tmp";
		std::error_code ec;
		fs::create_directories(file.parent_path(), ec);

		std::ofstream out(tmp, std::ios::trunc | std::ios::binary);
		if (out)
		{
			out << kHeader << "\r\n";
			for (const auto& kv : snapshot)
			{
				const Entry& e = kv.second;
				out << e.size << "|" << e.mtime;
				for (const std::string& digest : e.digests)
					out << "|" << digest;
				out << "|" << Text::NarrowUtf8(kv.first) << "\r\n";
			}
			out.close();
		}
		if (out && MoveFileExW(tmp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING))
			return;

		fs::remove(tmp, ec);
		std::lock_guard<std::mutex> lock(g_mutex);
		g_dirty = true;
	}
}
