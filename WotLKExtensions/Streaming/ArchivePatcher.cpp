#include "ArchivePatcher.h"

#include "ArchiveManifest.h"
#include "ArchiveRegistry.h"
#include "StreamLog.h"
#include "TextConv.h"

#include <ClientData/Streaming.h>

#include <windows.h>

#include <fstream>

namespace Streaming::ArchivePatcher
{
	namespace
	{
		bool ReadBinaryFile(const std::wstring& path, std::string& out)
		{
			std::ifstream f(path, std::ios::binary | std::ios::ate);
			if (!f)
				return false;
			std::streamsize size = f.tellg();
			if (size < 0)
				return false;
			out.resize((size_t)size);
			f.seekg(0, std::ios::beg);
			return size == 0 || (bool)f.read(out.data(), size);
		}
	}

	ArchivePatchResult Apply(const ArchivePatchJob& job)
	{
		using namespace ClientData::Streaming;
		ArchivePatchResult res;

		ArchiveMount mount;
		const bool registered = sArchiveRegistry.Find(job.archivePath, mount);
		void* raw = nullptr;
		std::string transientName;

		if (registered)
		{
			if (!sArchiveRegistry.EnsureWritable(job.archivePath, &res.error))
				return res;
			sArchiveRegistry.Find(job.archivePath, mount);
			raw = RawArchiveHandle(mount.archive);
			if (!raw)
			{
				res.error = "writable handle missing after reopen";
				return res;
			}
		}
		else
		{
			transientName = Text::NarrowAcp(job.archivePath);
			if (!MopaqOpenArchive(transientName.c_str(), 0, (uint32_t)(ARCHIVE_OPEN_DEFAULT | ARCHIVE_OPEN_WRITABLE), &raw) || !raw)
			{
				res.error = "cannot open unmounted archive for writing";
				return res;
			}
		}

		bool ok = true;
		std::string data;
		for (const ArchiveWrite& w : job.writes)
		{
			if (!ReadBinaryFile(w.blobPath, data) || (long long)data.size() != w.size)
			{
				res.error = "blob missing for " + w.name;
				ok = false;
				break;
			}
			if (!AddFile(raw, w.name.c_str(), data.data(), (uint32_t)data.size(),
			        ARCHIVE_ADD_REPLACE_EXISTING | ARCHIVE_ADD_COMPRESS))
			{
				res.error = "write failed for " + w.name;
				ok = false;
				break;
			}
			res.writtenNames.push_back(w.name);
		}

		if (ok && !AddFile(raw, ArchiveManifest::kFileName, job.targetManifest.data(),
		              (uint32_t)job.targetManifest.size(), ARCHIVE_ADD_REPLACE_EXISTING | ARCHIVE_ADD_COMPRESS))
		{
			res.error = "manifest write failed";
			ok = false;
		}

		if (registered)
		{
			std::string commitError;
			if (!sArchiveRegistry.Commit(job.archivePath, &commitError))
			{
				ok = false;
				if (res.error.empty())
					res.error = commitError;
			}
		}
		else
			MopaqCloseArchive(raw);

		res.ok = ok;
		if (ok)
			for (const ArchiveWrite& w : job.writes)
				DeleteFileW(w.blobPath.c_str());
		return res;
	}
}
