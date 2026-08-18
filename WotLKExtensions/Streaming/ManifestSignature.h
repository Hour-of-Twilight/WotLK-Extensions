#pragma once

#include <string>

namespace Streaming
{
	// Suffix appended to the manifest URL to reach its detached signature.
	extern const char* const kSignatureSuffix;

	// Checks a base64 ECDSA P-256 signature against the raw manifest bytes, using the public key
	// pinned in ManifestSignature.cpp. Everything about the manifest is untrusted until this
	// returns true, so it runs before parsing.
	bool VerifyManifestSignature(const std::string& manifestBytes, const std::string& signatureText);
}
