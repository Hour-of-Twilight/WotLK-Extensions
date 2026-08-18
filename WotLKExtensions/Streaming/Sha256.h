#pragma once

#include <string>

namespace Streaming
{
	std::string Sha256File(const std::wstring& path);

	// Cheap stand-in for Sha256File when all we need to know is "is this still the file the
	// manifest describes". Reads a few sampled windows instead of the whole file, so a 2 GB MPQ
	// costs megabytes rather than gigabytes. Returns "q1:<hex>", or "" if the file can't be read.
	std::string QuickDigestFile(const std::wstring& path);

	// Digest strings carry their own kind, so a manifest that switches a file between quick and
	// full hashing never compares one against the other.
	bool IsQuickDigest(const std::string& digest);

	// Raw 32-byte digest of a memory buffer, for callers that need the bytes rather than hex.
	bool Sha256Raw(const void* data, size_t size, unsigned char out[32]);
}
