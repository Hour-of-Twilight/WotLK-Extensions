// Copyright (C) 2026 WarcraftXL
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
#include "Models/Db2/DB2File.h"

#include "Models/Db2/Db2Decode.h"
#include "Models/Common/ModelLog.h"
#include "Models/Common/Timing.h"

#include "ClientData/Streaming.h"

#include <algorithm>
#include <fstream>
#include <string>

namespace ModernM2::Db2
{
    DB2File::DB2File(uint32_t rowSize) : m_rowSize(rowSize) {}

    namespace
    {
        // Same storage seam every other client-side asset (including wxl-db2's own WDC5 tables) reads
        // through: a loose file first (dev overrides), then the client's own already-mounted archive set.
        bool ReadWholeFile(const char* fileName, std::vector<uint8_t>& bytes)
        {
            bytes.clear();
            const std::string path = std::string("DBFilesClient\\") + fileName;

            std::ifstream loose(path, std::ios::binary | std::ios::ate);
            if (loose)
            {
                const std::streamoff end = loose.tellg();
                if (end > 0)
                {
                    bytes.resize(static_cast<size_t>(end));
                    loose.seekg(0, std::ios::beg);
                    if (loose.read(reinterpret_cast<char*>(bytes.data()), end)) return true;
                    bytes.clear();
                }
            }

            return ClientData::Streaming::ReadWholeFile(nullptr, path.c_str(), bytes);
        }
    }

    bool DB2File::Load(const char* fileName)
    {
        if (m_loaded) return true;
        const long long t0 = TickNow();
        std::vector<uint8_t> bytes;
        if (!ReadWholeFile(fileName, bytes))
        {
            WLOG_INFO("DB2: %s not found", fileName);
            return false;
        }
        m_readMs = TickMs(t0, TickNow());
        return LoadBytes(bytes.data(), static_cast<uint32_t>(bytes.size()), fileName);
    }

    bool DB2File::LoadBytes(const uint8_t* data, uint32_t size, const char* nameForLog)
    {
        if (m_loaded) return true;
        if (!data || size < 4)
        {
            WLOG_WARN("DB2: empty/short data for %s", nameForLog);
            return false;
        }

        uint32_t strColCount = 0;
        const uint32_t* strCols = StringColumns(&strColCount);

        const long long tDecode = TickNow();
        DB2Decoded dec;
        if (!DecodeDB2(data, size, dec, strCols, strColCount))
        {
            WLOG_ERROR("DB2: %s is not a supported DB2 (WDC1/2/3) / malformed", nameForLog);
            return false;
        }

        if (dec.rowSize != m_rowSize)
        {
            WLOG_ERROR("DB2: %s decoded record size %u != definition %u%s", nameForLog, dec.rowSize, m_rowSize,
                       dec.hasRelationship ? " (note: a trailing relationship column is appended)" : "");
            return false;
        }

        m_numRows = static_cast<uint32_t>(dec.ids.size());
        m_records = std::move(dec.records);
        m_ids     = std::move(dec.ids);
        m_strings = std::move(dec.strings);

        const long long tIndex = TickNow();
        BuildIndex();
        const long long tEnd = TickNow();
        m_loaded = true;
        WLOG_INFO("DB2: loaded %s (%u rows, id %d..%d, %s lookup) read=%.0fms decode=%.0fms index=%.0fms",
                  nameForLog, m_numRows, m_minId, m_maxId, m_idsAscending ? "sorted" : "hashed",
                  m_readMs, TickMs(tDecode, tIndex), TickMs(tIndex, tEnd));
        return true;
    }

    void DB2File::BuildIndex()
    {
        m_idIndex.clear();
        m_idsAscending = true;
        for (size_t i = 1; i < m_ids.size(); ++i)
        {
            if (m_ids[i] <= m_ids[i - 1]) { m_idsAscending = false; break; }
        }

        if (m_ids.empty())
        {
            m_minId = 0;
            m_maxId = -1;
            return;
        }

        if (m_idsAscending)
        {
            m_minId = m_ids.front();
            m_maxId = m_ids.back();
            return;
        }

        m_minId = 0x7FFFFFFF;
        m_maxId = -1;
        m_idIndex.reserve(m_ids.size());
        for (uint32_t i = 0; i < m_ids.size(); ++i)
        {
            const int32_t id = m_ids[i];
            m_idIndex[id] = i;
            if (id < m_minId) m_minId = id;
            if (id > m_maxId) m_maxId = id;
        }
    }

    void DB2File::Unload()
    {
        m_loaded = false;
        m_numRows = 0;
        m_minId = 0x7FFFFFFF;
        m_maxId = -1;
        m_records.clear();
        m_ids.clear();
        m_strings.clear();
        m_idIndex.clear();
        m_idsAscending = false;
        m_readMs = 0.0;
    }

    const void* DB2File::RowById(int32_t id) const
    {
        size_t row;
        if (m_idsAscending)
        {
            if (m_ids.empty() || id < m_minId || id > m_maxId) return nullptr;
            const auto it = std::lower_bound(m_ids.begin(), m_ids.end(), id);
            if (it == m_ids.end() || *it != id) return nullptr;
            row = static_cast<size_t>(it - m_ids.begin());
        }
        else
        {
            const auto it = m_idIndex.find(id);
            if (it == m_idIndex.end()) return nullptr;
            row = it->second;
        }
        return m_records.data() + row * m_rowSize;
    }

    const void* DB2File::RowByIndex(uint32_t index) const
    {
        if (index >= m_numRows) return nullptr;
        return m_records.data() + static_cast<size_t>(index) * m_rowSize;
    }

    int32_t DB2File::IdAt(uint32_t index) const
    {
        return (index < m_ids.size()) ? m_ids[index] : -1;
    }

    const char* DB2File::Str(uint32_t offset) const
    {
        if (offset >= m_strings.size()) return "";
        return m_strings.data() + offset;
    }
}
