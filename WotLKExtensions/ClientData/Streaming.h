#pragma once

#include <Macros.h>

#include <cstdint>
#include <vector>

namespace ClientData::Streaming
{
	CLIENT_FUNCTION(InitializeStreaming, 0x00422100, __cdecl, uint32_t, (const char* lpszUrl))

	CLIENT_FUNCTION(IsStreamingMode, 0x00422130, __cdecl, uint32_t, ())

	CLIENT_FUNCTION(OpenArchive, 0x00421950, __stdcall, int, (const char* filename, int priority, int flags, void** outHandle))
	CLIENT_FUNCTION(RebuildHash, 0x00423D70, __cdecl, void, ())

	CLIENT_FUNCTION(OpenFileEx, 0x00424B50, __stdcall, int, (void* hMpq, const char* name, int scope, void** phFile))
	CLIENT_FUNCTION(SFileRead, 0x00422530, __stdcall, int, (void* hFile, void* buffer, uint32_t toRead, uint32_t* read, void* overlapped, uint32_t unused))
	CLIENT_FUNCTION(GetFileSize, 0x004218C0, __stdcall, uint32_t, (void* hFile, uint32_t* highOut))
	CLIENT_FUNCTION(CloseFile, 0x00422910, __stdcall, int, (void* hFile))

	inline bool MountArchive(const char* filename, int priority, void** outHandle)
	{
		return OpenArchive(filename, priority, 0xC00, outHandle) != 0;
	}

	inline bool MountArchive(const char* filename, int priority = 0)
	{
		void* handle = nullptr;
		return MountArchive(filename, priority, &handle);
	}

	inline bool ReadWholeFile(void* hMpq, const char* name, std::vector<uint8_t>& out)
	{
		void* fh = nullptr;
		if (OpenFileEx(hMpq, name, 0, &fh) == 0 || !fh)
			return false;
		uint32_t size = GetFileSize(fh, nullptr);
		bool ok = false;
		if (size != 0 && size != 0xFFFFFFFFu)
		{
			out.resize(size);
			uint32_t read = 0;
			ok = SFileRead(fh, out.data(), size, &read, nullptr, 0) != 0 && read == size;
		}
		CloseFile(fh);
		return ok;
	}

	inline bool IsReady()
	{
		return *reinterpret_cast<uint8_t*>(0x00B38180) != 0;
	}

	inline bool IsTrial()
	{
		return *reinterpret_cast<uint8_t*>(0x00B38181) != 0;
	}

	inline bool Start(const char* manifestUrlOrPath)
	{
		if (IsReady())
			return true;
		return InitializeStreaming(manifestUrlOrPath) != 0;
	}
}
