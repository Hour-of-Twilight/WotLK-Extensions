#include <Defines.h>
#include "BlobContainer.h"

#include "ArchiveManifest.h"

#include <ClientData/Streaming.h>

#include <cstdint>
#include <cstring>

namespace Streaming::BlobContainer
{
	namespace
	{
		constexpr size_t kHeaderSize = 12;
		constexpr uint8 kVersion = 1;
		constexpr uint8 kEncodingRaw = 0;
		constexpr uint8 kEncodingZlib = 1;

		bool Fail(std::string* error, const char* what)
		{
			if (error)
				*error = what;
			return false;
		}
	}

	bool Unpack(const std::string& container, std::string& raw, std::string* error)
	{
		raw.clear();
		if (container.size() < kHeaderSize || std::memcmp(container.data(), "HOTB", 4) != 0)
			return Fail(error, "not a HOTB blob");
		const uint8* h = reinterpret_cast<const uint8*>(container.data());
		if (h[4] != kVersion)
			return Fail(error, "unsupported blob version");

		uint32 rawSize = (uint32)h[8] | ((uint32)h[9] << 8) | ((uint32)h[10] << 16) | ((uint32)h[11] << 24);
		if (rawSize > 0x7FFFFFFFu)
			return Fail(error, "blob is too large");

		const size_t payloadLen = container.size() - kHeaderSize;
		const uint8* payload = h + kHeaderSize;

		switch (h[5])
		{
		case kEncodingRaw:
			if (payloadLen != rawSize)
				return Fail(error, "raw blob size does not match its header");
			raw.assign(reinterpret_cast<const char*>(payload), payloadLen);
			return true;

		case kEncodingZlib:
		{
			if (rawSize == 0)
				return true;
			raw.resize(rawSize);
			uint32 got = rawSize;
			if (!ClientData::Streaming::ZlibDecompress(raw.data(), &got, payload, (uint32)payloadLen) || got != rawSize)
			{
				raw.clear();
				return Fail(error, "blob inflates to the wrong size");
			}
			return true;
		}

		default:
			return Fail(error, "unknown blob encoding");
		}
	}

	std::string RelativeBlobPath(const std::string& digest)
	{
		return "blob/" + digest.substr(0, 2) + "/" + digest;
	}

	std::string RelativeIndexPath(const std::string& contentId)
	{
		return "index/" + contentId + ArchiveManifest::kIndexExtension;
	}
}
