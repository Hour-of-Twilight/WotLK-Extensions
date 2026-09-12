#pragma once

#include <string>

namespace Streaming::BlobContainer
{
	// One archived file as served from <baseUrl>/blob/<aa>/<sha256>: "HOTB", version, encoding
	// (0 raw, 1 zlib), two reserved bytes, raw size as little-endian uint32, then the payload.
	bool Unpack(const std::string& container, std::string& raw, std::string* error = nullptr);

	std::string RelativeBlobPath(const std::string& digest);
	std::string RelativeIndexPath(const std::string& contentId);
}
