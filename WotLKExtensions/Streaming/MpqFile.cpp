#include <Defines.h>
#include "MpqFile.h"

#include "ArchiveManifest.h"

#include <ClientData/Streaming.h>

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Streaming::MpqFile
{
	namespace
	{
		constexpr uint32 kMagic = 0x1A51504D;
		constexpr uint32 kHashEmpty = 0xFFFFFFFF;
		constexpr uint32 kHashDeleted = 0xFFFFFFFE;
		constexpr uint32 kMaxHashTableSize = 0x80000;
		constexpr long long kHeaderScanLimit = 1 << 20;

		constexpr uint32 kFlagImplode = 0x00000100;
		constexpr uint32 kFlagCompress = 0x00000200;
		constexpr uint32 kFlagEncrypted = 0x00010000;
		constexpr uint32 kFlagFixKey = 0x00020000;
		constexpr uint32 kFlagPatch = 0x00100000;
		constexpr uint32 kFlagSingleUnit = 0x01000000;
		constexpr uint32 kFlagDeleteMarker = 0x02000000;
		constexpr uint32 kFlagSectorCrc = 0x04000000;
		constexpr uint32 kFlagExists = 0x80000000;

#pragma pack(push, 1)
		struct Header
		{
			uint32 magic;
			uint32 headerSize;
			uint32 archiveSize;
			uint16 formatVersion;
			uint16 sectorShift;
			uint32 hashTablePos;
			uint32 blockTablePos;
			uint32 hashTableSize;
			uint32 blockTableSize;
		};

		struct HeaderV2
		{
			uint64 hiBlockTablePos;
			uint16 hashTableHi;
			uint16 blockTableHi;
		};

		struct HashEntry
		{
			uint32 name1;
			uint32 name2;
			uint16 locale;
			uint16 platform;
			uint32 blockIndex;
		};

		struct BlockEntry
		{
			uint32 filePos;
			uint32 compSize;
			uint32 fileSize;
			uint32 flags;
		};
#pragma pack(pop)

		struct CryptTable
		{
			uint32 v[0x500];

			CryptTable()
			{
				uint32 seed = 0x00100001;
				for (uint32 index1 = 0; index1 < 0x100; ++index1)
				{
					for (uint32 index2 = index1, i = 0; i < 5; ++i, index2 += 0x100)
					{
						seed = (seed * 125 + 3) % 0x2AAAAB;
						uint32 temp1 = (seed & 0xFFFF) << 0x10;
						seed = (seed * 125 + 3) % 0x2AAAAB;
						uint32 temp2 = (seed & 0xFFFF);
						v[index2] = temp1 | temp2;
					}
				}
			}
		};

		const CryptTable& Crypt()
		{
			static CryptTable table;
			return table;
		}

		uint32 HashString(const char* s, uint32 type)
		{
			const CryptTable& t = Crypt();
			uint32 seed1 = 0x7FED7FED;
			uint32 seed2 = 0xEEEEEEEE;
			for (; *s; ++s)
			{
				uint32 ch = (unsigned char)*s;
				if (ch == '/')
					ch = '\\';
				if (ch >= 'a' && ch <= 'z')
					ch -= 32;
				seed1 = t.v[(type << 8) + ch] ^ (seed1 + seed2);
				seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3;
			}
			return seed1;
		}

		void Decrypt(uint32* data, size_t count, uint32 key)
		{
			const CryptTable& t = Crypt();
			uint32 seed = 0xEEEEEEEE;
			for (size_t i = 0; i < count; ++i)
			{
				seed += t.v[0x400 + (key & 0xFF)];
				uint32 ch = data[i] ^ (key + seed);
				key = ((~key << 0x15) + 0x11111111) | (key >> 0x0B);
				seed = ch + seed + (seed << 5) + 3;
				data[i] = ch;
			}
		}

		const char* PlainName(const char* name)
		{
			const char* plain = name;
			for (const char* p = name; *p; ++p)
				if (*p == '\\' || *p == '/')
					plain = p + 1;
			return plain;
		}

		struct Reader
		{
			HANDLE handle = INVALID_HANDLE_VALUE;
			long long size = 0;
			bool* unreadable = nullptr;

			~Reader()
			{
				if (handle != INVALID_HANDLE_VALUE)
					CloseHandle(handle);
			}

			bool Broken()
			{
				if (unreadable)
					*unreadable = true;
				return false;
			}

			bool Open(const std::wstring& path)
			{
				handle = CreateFileW(path.c_str(), GENERIC_READ,
				    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
				    FILE_ATTRIBUTE_NORMAL, nullptr);
				if (handle == INVALID_HANDLE_VALUE)
					return Broken();
				LARGE_INTEGER li{};
				if (!GetFileSizeEx(handle, &li))
					return Broken();
				size = li.QuadPart;
				return true;
			}

			bool ReadAt(long long offset, void* out, size_t n)
			{
				if (offset < 0 || n > 0x7FFFFFFF || offset + (long long)n > size)
					return false;
				uint8* dst = static_cast<uint8*>(out);
				while (n > 0)
				{
					OVERLAPPED ov{};
					ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
					ov.OffsetHigh = (DWORD)(offset >> 32);
					DWORD got = 0;
					if (!::ReadFile(handle, dst, (DWORD)n, &got, &ov) || got == 0)
						return Broken();
					dst += got;
					offset += got;
					n -= got;
				}
				return true;
			}
		};

		bool Fail(std::string* error, const char* what)
		{
			if (error)
				*error = what;
			return false;
		}
	}

	bool ReadFile(const std::wstring& archivePath, const char* name, std::string& out, std::string* error,
	    bool* unreadable)
	{
		out.clear();
		if (unreadable)
			*unreadable = false;
		Reader r;
		r.unreadable = unreadable;
		if (!r.Open(archivePath))
			return Fail(error, "cannot open archive");

		Header hdr{};
		long long headerOff = -1;
		for (long long off = 0; off + (long long)sizeof(Header) <= r.size && off < kHeaderScanLimit; off += 0x200)
		{
			if (!r.ReadAt(off, &hdr, sizeof(hdr)))
				return Fail(error, "cannot read header");
			if (hdr.magic == kMagic)
			{
				headerOff = off;
				break;
			}
		}
		if (headerOff < 0)
			return Fail(error, "no MPQ header");
		if (hdr.formatVersion > 1)
			return Fail(error, "unsupported MPQ format version");
		if (hdr.hashTableSize == 0 || hdr.hashTableSize > kMaxHashTableSize ||
		    (hdr.hashTableSize & (hdr.hashTableSize - 1)) != 0)
			return Fail(error, "bad hash table size");
		if (hdr.sectorShift > 16)
			return Fail(error, "bad sector size");

		long long hashPos = headerOff + hdr.hashTablePos;
		long long blockPos = headerOff + hdr.blockTablePos;
		HeaderV2 v2{};
		if (hdr.formatVersion == 1)
		{
			if (!r.ReadAt(headerOff + sizeof(Header), &v2, sizeof(v2)))
				return Fail(error, "cannot read extended header");
			hashPos += (long long)v2.hashTableHi << 32;
			blockPos += (long long)v2.blockTableHi << 32;
		}

		const size_t hashBytes = (size_t)hdr.hashTableSize * sizeof(HashEntry);
		const size_t blockBytes = (size_t)hdr.blockTableSize * sizeof(BlockEntry);
		if (hashPos + (long long)hashBytes > r.size || blockPos + (long long)blockBytes > r.size)
			return Fail(error, "tables lie outside the file");

		std::vector<HashEntry> hashes(hdr.hashTableSize);
		if (!r.ReadAt(hashPos, hashes.data(), hashBytes))
			return Fail(error, "cannot read hash table");
		Decrypt(reinterpret_cast<uint32*>(hashes.data()), hashBytes / 4, HashString("(hash table)", 3));

		const uint32 mask = hdr.hashTableSize - 1;
		const uint32 start = HashString(name, 0) & mask;
		const uint32 nameA = HashString(name, 1);
		const uint32 nameB = HashString(name, 2);
		uint32 blockIndex = kHashEmpty;
		for (uint32 i = 0; i < hdr.hashTableSize; ++i)
		{
			const HashEntry& e = hashes[(start + i) & mask];
			if (e.blockIndex == kHashEmpty)
				break;
			if (e.blockIndex == kHashDeleted)
				continue;
			if (e.name1 == nameA && e.name2 == nameB)
			{
				blockIndex = e.blockIndex;
				if (e.locale == 0)
					break;
			}
		}
		if (blockIndex == kHashEmpty)
			return Fail(error, "file not in archive");
		if (blockIndex >= hdr.blockTableSize)
			return Fail(error, "block index out of range");

		std::vector<BlockEntry> blocks(hdr.blockTableSize);
		if (!r.ReadAt(blockPos, blocks.data(), blockBytes))
			return Fail(error, "cannot read block table");
		Decrypt(reinterpret_cast<uint32*>(blocks.data()), blockBytes / 4, HashString("(block table)", 3));

		const BlockEntry b = blocks[blockIndex];
		if (!(b.flags & kFlagExists) || (b.flags & kFlagDeleteMarker))
			return Fail(error, "file not in archive");
		if (b.flags & (kFlagPatch | kFlagImplode))
			return Fail(error, "unsupported file storage");

		long long filePos = headerOff + b.filePos;
		if (hdr.formatVersion == 1 && v2.hiBlockTablePos != 0)
		{
			uint16 hi = 0;
			if (!r.ReadAt(headerOff + (long long)v2.hiBlockTablePos + (long long)blockIndex * 2, &hi, sizeof(hi)))
				return Fail(error, "cannot read hi-block table");
			filePos += (long long)hi << 32;
		}
		if (filePos + (long long)b.compSize > r.size)
			return Fail(error, "file data lies outside the archive");
		if (b.fileSize == 0)
			return true;

		std::vector<uint8> comp(b.compSize);
		if (!r.ReadAt(filePos, comp.data(), b.compSize))
			return Fail(error, "cannot read file data");

		const bool encrypted = (b.flags & kFlagEncrypted) != 0;
		const bool compressed = (b.flags & kFlagCompress) != 0;
		uint32 key = 0;
		if (encrypted)
		{
			key = HashString(PlainName(name), 3);
			if (b.flags & kFlagFixKey)
				key = (key + b.filePos) ^ b.fileSize;
		}

		out.resize(b.fileSize);
		uint8* dst = reinterpret_cast<uint8*>(out.data());

		if (b.flags & kFlagSingleUnit)
		{
			if (encrypted)
				Decrypt(reinterpret_cast<uint32*>(comp.data()), comp.size() / 4, key);
			if (compressed)
			{
				uint32 outSize = b.fileSize;
				if (!ClientData::Streaming::DecompressSector(dst, &outSize, comp.data(), b.compSize) || outSize != b.fileSize)
					return Fail(error, "cannot decompress file");
			}
			else
			{
				if (b.compSize < b.fileSize)
					return Fail(error, "file data is truncated");
				std::memcpy(dst, comp.data(), b.fileSize);
			}
			return true;
		}

		const uint32 sectorSize = 0x200u << hdr.sectorShift;
		const uint32 sectors = (uint32)(((uint64)b.fileSize + sectorSize - 1) / sectorSize);

		if (!compressed)
		{
			if (b.compSize < b.fileSize)
				return Fail(error, "file data is truncated");
			std::memcpy(dst, comp.data(), b.fileSize);
			if (encrypted)
				for (uint32 s = 0; s < sectors; ++s)
				{
					uint32 off = s * sectorSize;
					uint32 n = std::min(sectorSize, b.fileSize - off);
					Decrypt(reinterpret_cast<uint32*>(dst + off), n / 4, key + s);
				}
			return true;
		}

		const uint32 offCount = sectors + 1 + ((b.flags & kFlagSectorCrc) ? 1 : 0);
		if ((uint64)offCount * 4 > b.compSize)
			return Fail(error, "sector table is truncated");
		std::vector<uint32> offs(offCount);
		std::memcpy(offs.data(), comp.data(), (size_t)offCount * 4);
		if (encrypted)
			Decrypt(offs.data(), offCount, key - 1);

		for (uint32 s = 0; s < sectors; ++s)
		{
			uint32 begin = offs[s];
			uint32 end = offs[s + 1];
			if (begin > end || end > b.compSize)
				return Fail(error, "sector table is corrupt");
			uint32 rawOff = s * sectorSize;
			uint32 rawSize = std::min(sectorSize, b.fileSize - rawOff);
			uint32 compLen = end - begin;
			uint8* in = comp.data() + begin;
			if (encrypted)
				Decrypt(reinterpret_cast<uint32*>(in), compLen / 4, key + s);
			uint32 outSize = rawSize;
			if (!ClientData::Streaming::DecompressSector(dst + rawOff, &outSize, in, compLen) || outSize != rawSize)
				return Fail(error, "cannot decompress sector");
		}
		return true;
	}

	bool ContentId(const std::wstring& archivePath, std::string& out)
	{
		out.clear();
		std::string bytes;
		bool unreadable = false;
		if (!ReadFile(archivePath, ArchiveManifest::kFileName, bytes, nullptr, &unreadable))
			return !unreadable;
		if (!bytes.empty())
			out = ArchiveManifest::ContentIdOf(bytes);
		return true;
	}
}
