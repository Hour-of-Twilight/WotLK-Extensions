#include "Sha256.h"

#include <windows.h>
#include <bcrypt.h>

#include <fstream>
#include <vector>

namespace Streaming
{
	namespace
	{
		std::string ToHex(const unsigned char* d, size_t n)
		{
			static const char* hx = "0123456789abcdef";
			std::string s(n * 2, '0');
			for (size_t i = 0; i < n; ++i)
			{
				s[i * 2] = hx[d[i] >> 4];
				s[i * 2 + 1] = hx[d[i] & 0xF];
			}
			return s;
		}
	}

	std::string Sha256File(const std::wstring& path)
	{
		std::ifstream f(path, std::ios::binary);
		if (!f)
			return "";

		BCRYPT_ALG_HANDLE alg = nullptr;
		if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
			return "";

		DWORD objLen = 0, cb = 0;
		BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen), sizeof(objLen), &cb, 0);
		std::vector<unsigned char> obj(objLen);

		BCRYPT_HASH_HANDLE hash = nullptr;
		if (BCryptCreateHash(alg, &hash, obj.data(), objLen, nullptr, 0, 0) != 0)
		{
			BCryptCloseAlgorithmProvider(alg, 0);
			return "";
		}

		std::vector<char> buf(1 << 20);
		while (f)
		{
			f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
			std::streamsize got = f.gcount();
			if (got > 0)
				BCryptHashData(hash, reinterpret_cast<PUCHAR>(buf.data()), static_cast<ULONG>(got), 0);
		}

		unsigned char digest[32];
		NTSTATUS st = BCryptFinishHash(hash, digest, sizeof(digest), 0);
		BCryptDestroyHash(hash);
		BCryptCloseAlgorithmProvider(alg, 0);
		if (st != 0)
			return "";

		return ToHex(digest, sizeof(digest));
	}
}
