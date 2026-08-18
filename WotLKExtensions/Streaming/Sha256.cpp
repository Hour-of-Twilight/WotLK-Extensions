#include "Sha256.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <fstream>
#include <vector>

namespace Streaming
{
	namespace
	{
		const char kQuickPrefix[] = "q1:";
		const size_t kQuickPrefixLen = 3;

		// Sampled-digest layout, version 1. HoTManifestTool produces these and the launcher
		// checks them, so any change here has to land in HoTLauncher.Shared/Hashing.cs too.
		//
		//   sha256 over:
		//     "hotquick1"   9 bytes ASCII, domain tag
		//     size          8 bytes little-endian
		//     windows       kWindowBytes each, in ascending offset order
		//
		// Files of kWindowBytes * kWindows or less are hashed whole. Larger ones use kWindows
		// evenly spaced windows, the first at offset 0 and the last ending exactly at EOF.
		const long long kWindowBytes = 1 << 20;
		const int kWindows = 8;

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

		struct Sha256Hasher
		{
			BCRYPT_ALG_HANDLE alg = nullptr;
			BCRYPT_HASH_HANDLE hash = nullptr;
			std::vector<unsigned char> obj;

			bool Open()
			{
				if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
				{
					alg = nullptr;
					return false;
				}

				DWORD objLen = 0, cb = 0;
				BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen),
				    sizeof(objLen), &cb, 0);
				obj.resize(objLen);

				if (BCryptCreateHash(alg, &hash, obj.data(), objLen, nullptr, 0, 0) != 0)
				{
					hash = nullptr;
					return false;
				}
				return true;
			}

			void Update(const void* data, size_t n)
			{
				if (n > 0)
					BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<void*>(data)),
					    static_cast<ULONG>(n), 0);
			}

			bool FinishRaw(unsigned char out[32])
			{
				return BCryptFinishHash(hash, out, 32, 0) == 0;
			}

			std::string Finish()
			{
				unsigned char digest[32];
				if (!FinishRaw(digest))
					return "";
				return ToHex(digest, sizeof(digest));
			}

			~Sha256Hasher()
			{
				if (hash)
					BCryptDestroyHash(hash);
				if (alg)
					BCryptCloseAlgorithmProvider(alg, 0);
			}
		};

		// Hashes count bytes from the stream's current position. False on a short read.
		bool HashRange(std::ifstream& f, Sha256Hasher& h, std::vector<char>& buf, long long count)
		{
			while (count > 0)
			{
				std::streamsize want = static_cast<std::streamsize>(
				    std::min<long long>(count, static_cast<long long>(buf.size())));
				f.read(buf.data(), want);
				std::streamsize got = f.gcount();
				if (got <= 0)
					return false;
				h.Update(buf.data(), static_cast<size_t>(got));
				count -= got;
			}
			return true;
		}
	}

	std::string Sha256File(const std::wstring& path)
	{
		std::ifstream f(path, std::ios::binary);
		if (!f)
			return "";

		Sha256Hasher h;
		if (!h.Open())
			return "";

		std::vector<char> buf(1 << 20);
		while (f)
		{
			f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
			std::streamsize got = f.gcount();
			if (got > 0)
				h.Update(buf.data(), static_cast<size_t>(got));
		}

		return h.Finish();
	}

	std::string QuickDigestFile(const std::wstring& path)
	{
		std::ifstream f(path, std::ios::binary | std::ios::ate);
		if (!f)
			return "";

		long long size = static_cast<long long>(f.tellg());
		if (size < 0)
			return "";

		Sha256Hasher h;
		if (!h.Open())
			return "";

		h.Update("hotquick1", 9);

		unsigned char le[8];
		for (int i = 0; i < 8; ++i)
			le[i] = static_cast<unsigned char>(static_cast<unsigned long long>(size) >> (i * 8));
		h.Update(le, sizeof(le));

		const bool whole = size <= kWindowBytes * kWindows;
		const int windows = whole ? 1 : kWindows;
		std::vector<char> buf(static_cast<size_t>(kWindowBytes));

		for (int i = 0; i < windows; ++i)
		{
			long long offset = whole ? 0 : (size - kWindowBytes) * i / (kWindows - 1);
			f.clear(); // the previous window may have ended on EOF, which blocks seekg
			f.seekg(offset, std::ios::beg);
			if (!f)
				return "";
			if (!HashRange(f, h, buf, whole ? size : kWindowBytes))
				return "";
		}

		std::string hex = h.Finish();
		if (hex.empty())
			return "";
		return kQuickPrefix + hex;
	}

	bool IsQuickDigest(const std::string& digest)
	{
		return digest.compare(0, kQuickPrefixLen, kQuickPrefix) == 0;
	}

	bool Sha256Raw(const void* data, size_t size, unsigned char out[32])
	{
		Sha256Hasher h;
		if (!h.Open())
			return false;
		h.Update(data, size);
		return h.FinishRaw(out);
	}
}
