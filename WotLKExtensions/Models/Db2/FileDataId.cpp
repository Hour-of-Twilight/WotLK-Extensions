// Ported from WarcraftXL (scripts/wxl-db2/src/api/FdidResolver.cpp).
// Copyright (C) 2026 WarcraftXL
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

// Each of the three tables here runs to hundreds of thousands of rows, so each sits behind its own
// once_flag and is built only by the entry point that needs it. Resolving a model's textures touches
// the texture table alone, and BeginWarm moves even that onto a worker thread.

#include "Models/Db2/FileDataId.h"

#include "Models/Db2/DB2File.h"
#include "Models/Common/ModelHooks.h"
#include "Models/Common/Timing.h"

#include <cctype>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace ModernM2::Fdid
{
	namespace
	{
		// Both *FilePath.db2 decode to {uint32 id; int32 path}.
		struct PathRow
		{
			uint32_t id;
			int32_t path;
		};

		struct Tables
		{
			Db2::DB2Table<PathRow> tex;
			Db2::DB2Table<PathRow> model;
			std::unordered_map<std::string, uint32_t> modelIdByStem;

			std::once_flag onceTex;
			std::once_flag onceModel;
			std::once_flag onceStems;
			bool texReady = false;
			bool modelReady = false;

			// fdid -> path, an empty string being a cached miss. Node pointers survive rehash, which
			// is what lets a resolved c_str() be handed straight to the client.
			std::mutex cacheMutex;
			std::unordered_map<uint32_t, std::string> texCache;
			std::unordered_map<uint32_t, std::string> modelCache;
		};

		/// Leaked on purpose: BeginWarm's thread can still be loading at process teardown.
		Tables& T()
		{
			static Tables* tables = new Tables();
			return *tables;
		}

		/// Lowercases, folds forward slashes to backslashes and drops any extension, so the one
		/// spelling the path table stores and the many a caller can hold collapse to the same key.
		std::string NormalizeStem(const char* path)
		{
			std::string s;
			for (const char* p = path; *p; ++p)
			{
				const char c = *p;
				s.push_back(c == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
			}
			const size_t dot = s.find_last_of('.');
			const size_t sep = s.find_last_of('\\');
			if (dot != std::string::npos && (sep == std::string::npos || dot > sep))
				s.resize(dot);
			return s;
		}

		void LoadTexTable()
		{
			T().tex.Load("TextureFilePath.db2");
			T().texReady = T().tex.RowCount() != 0;
			if (!T().texReady)
				WLOG_WARN("db2-fdid: TextureFilePath.db2 unavailable, texture FileDataID resolution disabled");
		}

		void LoadModelTable()
		{
			T().model.Load("ModelFilePath.db2");
			T().modelReady = T().model.RowCount() != 0;
			if (!T().modelReady)
				WLOG_WARN("db2-fdid: ModelFilePath.db2 unavailable, model FileDataID resolution disabled");
		}

		void EnsureModelTable()
		{
			std::call_once(T().onceModel, LoadModelTable);
		}

		void BuildStemIndex()
		{
			if (!T().modelReady)
				return;
			const long long t0 = TickNow();
			T().modelIdByStem.reserve(T().model.RowCount());
			for (uint32_t i = 0; i < T().model.RowCount(); ++i)
			{
				const PathRow* r = T().model.At(i);
				if (!r)
					continue;
				const char* path = T().model.Str(static_cast<uint32_t>(r->path));
				// First spelling wins: two rows collapsing to one stem differ only by container
				// extension, and either id resolves to the same asset for every consumer here.
				if (path && *path)
					T().modelIdByStem.emplace(NormalizeStem(path), r->id);
			}
			WLOG_INFO("db2-fdid: model stem index built (%zu stems in %.0f ms)",
			    T().modelIdByStem.size(), TickMs(t0, TickNow()));
		}

		bool LookupTexPath(uint32_t fileDataId, std::string& outPath)
		{
			const PathRow* r = T().tex.Find(static_cast<int32_t>(fileDataId));
			const char* path = r ? T().tex.Str(static_cast<uint32_t>(r->path)) : nullptr;
			if (!path || !*path)
				return false;
			outPath = path;
			return true;
		}

		bool LookupModelPath(uint32_t fileDataId, std::string& outPath)
		{
			if (T().modelReady)
			{
				const PathRow* r = T().model.Find(static_cast<int32_t>(fileDataId));
				const char* path = r ? T().model.Str(static_cast<uint32_t>(r->path)) : nullptr;
				if (path && *path)
				{
					outPath = path;
					return true;
				}
			}
			return T().texReady && LookupTexPath(fileDataId, outPath);
		}

		const char* Cached(std::unordered_map<uint32_t, std::string>& cache, uint32_t fdid,
		    bool (*lookup)(uint32_t, std::string&))
		{
			std::lock_guard<std::mutex> lock(T().cacheMutex);
			auto it = cache.find(fdid);
			if (it == cache.end())
			{
				std::string path;
				lookup(fdid, path); // leaves path empty on a miss
				it = cache.emplace(fdid, std::move(path)).first;
			}
			return it->second.empty() ? nullptr : it->second.c_str();
		}
	}

	void EnsureLoaded()
	{
		std::call_once(T().onceTex, LoadTexTable);
	}

	void BeginWarm()
	{
		static std::once_flag started;
		std::call_once(started, []
		{
			try
			{
				std::thread([]
				{
					const long long t0 = TickNow();
					EnsureLoaded();
					WLOG_INFO("db2-fdid: texture path table prefetched in %.0f ms", TickMs(t0, TickNow()));
				}).detach();
			}
			catch (...)
			{
				WLOG_WARN("db2-fdid: prefetch thread could not start, the table loads on first use");
			}
		});
	}

	const char* ResolveTexture(uint32_t fileDataId)
	{
		if (fileDataId == 0)
			return nullptr;
		EnsureLoaded();
		if (!T().texReady)
			return nullptr;
		return Cached(T().texCache, fileDataId, LookupTexPath);
	}

	const char* ResolveModel(uint32_t fileDataId)
	{
		if (fileDataId == 0)
			return nullptr;
		EnsureLoaded();
		EnsureModelTable();
		if (!T().texReady && !T().modelReady)
			return nullptr;
		return Cached(T().modelCache, fileDataId, LookupModelPath);
	}

	uint32_t ResolveModelId(const char* modelPath)
	{
		if (!modelPath || !*modelPath)
			return 0;
		EnsureModelTable();
		std::call_once(T().onceStems, BuildStemIndex);
		if (T().modelIdByStem.empty())
			return 0;

		const auto it = T().modelIdByStem.find(NormalizeStem(modelPath));
		return it == T().modelIdByStem.end() ? 0 : it->second;
	}
}

namespace ModernM2
{
	const char* ResolveTexture(uint32_t fileDataId)
	{
		return Fdid::ResolveTexture(fileDataId);
	}
}
