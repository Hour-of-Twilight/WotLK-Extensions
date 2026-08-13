#include "DbcFromMpq.h"

#include <DBCPatch.h>
#include <ClientData/Streaming.h>
#include <CDBCMgr.h>
#include <Helpers/Util.h>
#include <Logger.h>

#include <cstdint>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>

namespace DbcFromMpq
{
	namespace
	{
		struct DbcCopy
		{
			uint16_t structOff;
			uint16_t fileOff;
			uint16_t bytes;
		};
		struct DbcStr
		{
			uint16_t structOff;
			uint16_t fileOff;
		};
		struct DbcTransform
		{
			const char* name;
			uint16_t structSize;
			uint16_t fileRecordSize;
			const DbcCopy* copies;
			uint8_t copyCount;
			const DbcStr* strings;
			uint8_t strCount;
			uint16_t idOff;     // struct offset the client keys m_recordsById off
			uint8_t idGenerated; // 1: no ID column, the key is the 0-based row index
		};

#include "DbcTransforms.inc"

		bool iequal_n(const char* a, const char* b, size_t n)
		{
			for (size_t i = 0; i < n; ++i)
				if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
					return false;
			return true;
		}

		bool icontains(const char* hay, size_t hayLen, const char* needle, size_t needleLen)
		{
			if (needleLen == 0 || hayLen < needleLen)
				return false;
			for (size_t i = 0; i + needleLen <= hayLen; ++i)
				if (iequal_n(hay + i, needle, needleLen))
					return true;
			return false;
		}

		const DbcTransform* FindTransform(const char* stem, size_t len)
		{
			for (const DbcTransform& t : kTransforms)
				if (std::strlen(t.name) == len && iequal_n(t.name, stem, len))
					return &t;
			return nullptr;
		}

		bool IsCustomDbc(const char* stem, size_t len)
		{
			for (const auto& kv : GlobalCDBCMap.allCDBCs)
				if (kv.first.size() == len && iequal_n(kv.first.c_str(), stem, len))
					return true;
			return false;
		}

#pragma pack(push, 1)
		struct WdbcHeader
		{
			char magic[4]; // 'WDBC'
			uint32_t recordCount;
			uint32_t fieldCount;
			uint32_t recordSize;
			uint32_t stringSize;
		};
#pragma pack(pop)

		void ApplyDbc(void* hMpq, const char* mpqPath, const char* stem, size_t stemLen)
		{
			std::vector<uint8_t> data;
			if (!ClientData::Streaming::ReadWholeFile(hMpq, mpqPath, data))
				return;
			if (data.size() < sizeof(WdbcHeader))
				return;

			WdbcHeader h;
			std::memcpy(&h, data.data(), sizeof(h));
			if (std::memcmp(h.magic, "WDBC", 4) != 0 || h.recordSize == 0 || h.recordCount == 0)
				return;
			size_t recBytes = static_cast<size_t>(h.recordCount) * h.recordSize;
			if (sizeof(WdbcHeader) + recBytes + h.stringSize > data.size())
				return; // truncated / malformed

			const uint8_t* records = data.data() + sizeof(WdbcHeader);
			const char* strBlock = reinterpret_cast<const char*>(records + recBytes);
			std::string name(stem, stemLen);

			const DbcTransform* t = FindTransform(stem, stemLen);
			if (!t)
			{
				// No in-memory layout, so the raw file record would corrupt the storage.
				Util::DebugOutput("dbc: %s has no transform, skipped", name.c_str());
				LOG_DEBUG << "DBC skip '" << name.c_str() << "' no transform; " << h.recordCount
				          << " records not applied";
				return;
			}
			if (h.recordSize != t->fileRecordSize)
			{
				Util::DebugOutput("dbc: %s file record %u != expected %u, skipped",
				    name.c_str(), h.recordSize, t->fileRecordSize);
				return;
			}

			// Collapse each file record to the in-memory struct layout.
			std::vector<uint8_t> images(static_cast<size_t>(h.recordCount) * t->structSize, 0);
			std::vector<uint32_t> ids(h.recordCount);
			for (uint32_t i = 0; i < h.recordCount; ++i)
			{
				const uint8_t* fileRec = records + static_cast<size_t>(i) * h.recordSize;
				uint8_t* dst = images.data() + static_cast<size_t>(i) * t->structSize;
				for (uint8_t c = 0; c < t->copyCount; ++c)
					std::memcpy(dst + t->copies[c].structOff, fileRec + t->copies[c].fileOff,
					    t->copies[c].bytes);
				for (uint8_t s = 0; s < t->strCount; ++s)
					std::memcpy(dst + t->strings[s].structOff, fileRec + t->strings[s].fileOff,
					    sizeof(uint32_t)); // enUS offset into strBlock
				// No ID column: the client stamps the row index and keys off that.
				if (t->idGenerated)
					std::memcpy(dst + t->idOff, &i, sizeof(uint32_t));
				std::memcpy(&ids[i], dst + t->idOff, sizeof(uint32_t));
			}
			std::vector<uint16_t> strOffsets(t->strCount);
			for (uint8_t s = 0; s < t->strCount; ++s)
				strOffsets[s] = t->strings[s].structOff;

			// Row index keys are positional, so the streamed file has to be the whole table.
			if (DBCPatch::ApplyRecords(name.c_str(), t->structSize, strOffsets, ids,
			        images.data(), strBlock, h.stringSize, t->idGenerated != 0))
				Util::DebugOutput("dbc: refreshed %s (%u records) from streamed mpq",
				    name.c_str(), h.recordCount);
		}

		void EnumerateListfile(void* hMpq, RefreshResult& result, bool& customSeen)
		{
			std::vector<uint8_t> lf;
			if (!ClientData::Streaming::ReadWholeFile(hMpq, "(listfile)", lf))
				return; // shipped MPQs always have a listfile

			static const char kPrefix[] = "dbfilesclient\\"; // matched case-insensitively
			static const char kExt[] = ".dbc";
			static const char kInterface[] = "interface";
			static const char* const kArtExts[] = { ".m2", ".mdx", ".skin", ".blp", ".anim" };
			const size_t prefLen = sizeof(kPrefix) - 1;
			const size_t extLen = sizeof(kExt) - 1;
			const size_t ifaceLen = sizeof(kInterface) - 1;

			const char* p = reinterpret_cast<const char*>(lf.data());
			const char* end = p + lf.size();
			while (p < end)
			{
				const char* nl = p;
				while (nl < end && *nl != '\r' && *nl != '\n')
					++nl;
				size_t len = static_cast<size_t>(nl - p);

				if (!result.interfaceFiles && icontains(p, len, kInterface, ifaceLen))
					result.interfaceFiles = true;

				// UI art rides on the reload prompt, so only world art counts here.
				if (!result.artFiles && !(len >= ifaceLen && iequal_n(p, kInterface, ifaceLen)))
					for (const char* ext : kArtExts)
					{
						size_t el = std::strlen(ext);
						if (len > el && iequal_n(p + len - el, ext, el))
						{
							result.artFiles = true;
							break;
						}
					}

				if (len > prefLen + extLen &&
				    iequal_n(p, kPrefix, prefLen) &&
				    iequal_n(p + len - extLen, kExt, extLen))
				{
					size_t stemEnd = len - extLen;
					size_t stemStart = stemEnd;
					while (stemStart > 0 && p[stemStart - 1] != '\\' && p[stemStart - 1] != '/')
						--stemStart;
					const char* stem = p + stemStart;
					size_t stemLen = stemEnd - stemStart;
					if (stemLen >= 5 && iequal_n(stem, "spell", 5))
						result.spellDataChanged = true;
					// achievement, achievement_Category and achievement_Criteria all feed the index.
					if (stemLen >= 11 && iequal_n(stem, "achievement", 11))
						result.achievementDataChanged = true;
					if (IsCustomDbc(stem, stemLen))
					{
						if (GlobalCDBCMap.hasRowWriter(std::string(stem, stemLen)))
						{
							std::string path(p, len);
							ApplyDbc(hMpq, path.c_str(), stem, stemLen);
						}
						else
							customSeen = true;
					}
					else
					{
						std::string path(p, len);
						ApplyDbc(hMpq, path.c_str(), stem, stemLen);
					}
				}

				p = nl;
				while (p < end && (*p == '\r' || *p == '\n'))
					++p;
			}
		}
	}

	RefreshResult RefreshFromArchive(void* hMpq)
	{
		LOG_DEBUG << "DbcFromMpq: refresh pass start";
		RefreshResult result;
		bool customSeen = false;
		EnumerateListfile(hMpq, result, customSeen);
		if (customSeen)
			CDBCMgr::Load();
		LOG_DEBUG << "DbcFromMpq: refresh pass done (customSeen=" << (int)customSeen
		          << " spell=" << (int)result.spellDataChanged
		          << " interface=" << (int)result.interfaceFiles
		          << " art=" << (int)result.artFiles << ")";
		return result;
	}
}
