#include "ManifestSignature.h"

#include "Sha256.h"

#include <Logger.h>

#include <windows.h>
#include <bcrypt.h>

#include <vector>

namespace Streaming
{
	const char* const kSignatureSuffix = ".sig";

	namespace
	{
		const size_t kKeyBytes = 32;       // P-256 field size
		const size_t kSignatureBytes = 64; // r || s

		// Public half of the manifest signing key
		const unsigned char kPublicKey[kKeyBytes * 2] = {
			0xd1,
			0x42,
			0x82,
			0x2d,
			0x08,
			0x60,
			0xb7,
			0xb6,
			0xc8,
			0xab,
			0xe3,
			0xea,
			0xe8,
			0x32,
			0x0a,
			0x93,
			0x90,
			0x05,
			0xac,
			0x11,
			0xd6,
			0x22,
			0x68,
			0x05,
			0xf8,
			0x4e,
			0x59,
			0x6d,
			0xb0,
			0x60,
			0xde,
			0xdc,
			0xe7,
			0x5c,
			0x04,
			0x83,
			0x82,
			0x0a,
			0x00,
			0xad,
			0x67,
			0x24,
			0xe9,
			0x43,
			0xa3,
			0x3a,
			0xd3,
			0x71,
			0x25,
			0x6c,
			0xdd,
			0xe0,
			0x29,
			0x66,
			0xc9,
			0xaf,
			0xde,
			0x6a,
			0x9d,
			0xbd,
			0x61,
			0x6c,
			0x7f,
			0x64,
		};

		int Base64Value(char c)
		{
			if (c >= 'A' && c <= 'Z')
				return c - 'A';
			if (c >= 'a' && c <= 'z')
				return c - 'a' + 26;
			if (c >= '0' && c <= '9')
				return c - '0' + 52;
			if (c == '+')
				return 62;
			if (c == '/')
				return 63;
			return -1; // padding, whitespace and anything else
		}

		bool Base64Decode(const std::string& text, std::vector<unsigned char>& out)
		{
			out.clear();
			unsigned int acc = 0;
			int bits = 0;

			for (char c : text)
			{
				if (c == '=')
					break;
				if (c == '\r' || c == '\n' || c == ' ' || c == '\t')
					continue;

				int v = Base64Value(c);
				if (v < 0)
					return false;

				acc = (acc << 6) | (unsigned int)v;
				bits += 6;
				if (bits >= 8)
				{
					bits -= 8;
					out.push_back((unsigned char)((acc >> bits) & 0xFF));
				}
			}
			return true;
		}
	}

	bool VerifyManifestSignature(const std::string& manifestBytes, const std::string& signatureText)
	{
		std::vector<unsigned char> signature;
		if (!Base64Decode(signatureText, signature) || signature.size() != kSignatureBytes)
			return false;

		unsigned char digest[32];
		if (!Sha256Raw(manifestBytes.data(), manifestBytes.size(), digest))
			return false;

		BCRYPT_ALG_HANDLE alg = nullptr;
		if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0)
		{
			sLog.Write("WARN", "stream", "ECDSA P-256 provider unavailable, cannot verify manifest signature");
			return false;
		}

		// BCRYPT_ECCKEY_BLOB: magic, key size, then the X and Y coordinates back to back.
		std::vector<unsigned char> blob(sizeof(BCRYPT_ECCKEY_BLOB) + sizeof(kPublicKey));
		BCRYPT_ECCKEY_BLOB* header = reinterpret_cast<BCRYPT_ECCKEY_BLOB*>(blob.data());
		header->dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
		header->cbKey = kKeyBytes;
		memcpy(blob.data() + sizeof(BCRYPT_ECCKEY_BLOB), kPublicKey, sizeof(kPublicKey));

		BCRYPT_KEY_HANDLE key = nullptr;
		NTSTATUS st = BCryptImportKeyPair(alg, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key,
		    blob.data(), (ULONG)blob.size(), 0);
		if (st != 0)
		{
			BCryptCloseAlgorithmProvider(alg, 0);
			return false;
		}

		st = BCryptVerifySignature(key, nullptr, digest, sizeof(digest),
		    signature.data(), (ULONG)signature.size(), 0);

		BCryptDestroyKey(key);
		BCryptCloseAlgorithmProvider(alg, 0);
		return st == 0;
	}
}
