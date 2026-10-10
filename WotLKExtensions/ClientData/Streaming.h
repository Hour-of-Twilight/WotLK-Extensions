#pragma once

#include <Defines.h>

#include <Macros.h>

#include <cstdint>
#include <vector>

namespace ClientData::Streaming
{
	CLIENT_FUNCTION(OpenArchive, 0x00421950, __stdcall, int, (const char* filename, int priority, int flags, void** outHandle))
	CLIENT_FUNCTION(CloseArchive, 0x00421720, __stdcall, char, (void* archive))
	CLIENT_FUNCTION(RebuildHash, 0x00423D70, __cdecl, void, ())

	CLIENT_FUNCTION(OpenFileEx, 0x00424B50, __stdcall, int, (void* hMpq, const char* name, int scope, void** phFile))
	CLIENT_FUNCTION(SFileRead, 0x00422530, __stdcall, int, (void* hFile, void* buffer, uint32 toRead, uint32* read, void* overlapped, uint32 unused))
	CLIENT_FUNCTION(GetFileSize, 0x004218C0, __stdcall, uint32, (void* hFile, uint32* highOut))
	CLIENT_FUNCTION(CloseFile, 0x00422910, __stdcall, int, (void* hFile))

	// Raw mopaq layer underneath the SArchive wrapper that OpenArchive hands out.
	CLIENT_FUNCTION(MopaqOpenArchive, 0x0045C480, __cdecl, char, (const char* filename, long priority, uint32 flags, void** outRaw))
	CLIENT_FUNCTION(MopaqCloseArchive, 0x00458980, __cdecl, char, (void* raw))
	CLIENT_FUNCTION(CreateFileInArchive, 0x00460010, __cdecl, char, (void* raw, const char* name, uint32 size, uint32 flags, void** outFile))
	CLIENT_FUNCTION(WriteFileInArchive, 0x00426FA0, __cdecl, char, (void* file, const void* data, uint32 size))
	CLIENT_FUNCTION(ReleaseHandle, 0x004321A0, __cdecl, char, (void* handle))

	CLIENT_FUNCTION(ZlibDecompress, 0x00453BB0, __cdecl, char, (void* out, uint32* inoutSize, const void* in, uint32 inSize))
	CLIENT_FUNCTION(DecompressSector, 0x004585F0, __cdecl, char, (void* out, uint32* inoutSize, const uint8* in, uint32 inSize))

	constexpr int ARCHIVE_OPEN_DEFAULT = 0xC00;
	constexpr int ARCHIVE_OPEN_WRITABLE = 0x10;
	constexpr uint32 ARCHIVE_ADD_REPLACE_EXISTING = 0x1;
	constexpr uint32 ARCHIVE_ADD_COMPRESS = 0x200;

	inline void* RawArchiveHandle(void* archive)
	{
		return archive ? static_cast<void**>(archive)[1] : nullptr;
	}

	inline void SetRawArchiveHandle(void* archive, void* raw)
	{
		if (archive)
			static_cast<void**>(archive)[1] = raw;
	}

	inline bool MountArchive(const char* filename, int priority, void** outHandle)
	{
		return OpenArchive(filename, priority, ARCHIVE_OPEN_DEFAULT, outHandle) != 0;
	}

	inline bool MountArchive(const char* filename, int priority = 0)
	{
		void* handle = nullptr;
		return MountArchive(filename, priority, &handle);
	}

	inline bool ReadWholeFile(void* hMpq, const char* name, std::vector<uint8>& out)
	{
		void* fh = nullptr;
		if (OpenFileEx(hMpq, name, 0, &fh) == 0 || !fh)
			return false;
		uint32 size = GetFileSize(fh, nullptr);
		bool ok = false;
		if (size != 0 && size != 0xFFFFFFFFu)
		{
			out.resize(size);
			uint32 read = 0;
			ok = SFileRead(fh, out.data(), size, &read, nullptr, 0) != 0 && read == size;
		}
		CloseFile(fh);
		return ok;
	}

	inline bool AddFile(void* raw, const char* name, const void* data, uint32 size, uint32 flags)
	{
		void* file = nullptr;
		if (!CreateFileInArchive(raw, name, size, flags, &file) || !file)
			return false;
		bool ok = size == 0 || WriteFileInArchive(file, data, size) != 0;
		ReleaseHandle(file);
		return ok;
	}
}
