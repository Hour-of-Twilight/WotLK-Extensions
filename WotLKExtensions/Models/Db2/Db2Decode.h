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
#pragma once

#include <cstdint>
#include <vector>

// DB2 (WDC1 / WDC2 / WDC3) decoder. Reads a .db2 file and normalises every record into a
// flat array of 4-byte columns. Implements the field storage model:
//   - storage type 0 (none):                   field bytes are inline in the record.
//   - storage type 1 (bitpacked):              field is a bitfield in the record.
//   - storage type 2 (common data):            field is a default value, overridden per-id in common_data.
//   - storage type 3 (bitpacked indexed):      field is a bitpacked index into pallet_data.
//   - storage type 4 (bitpacked indexed array):field is one index that expands to an array in pallet_data.
// It also resolves the id list / inline id, copy table (duplicated rows), relationship map (appended as a
// trailing column) and multiple sections.
//
// Each field keeps its native width (a uint16 stays 2 bytes, a uint8 stays 1 byte, an array stays
// consecutive elements), laid out with natural C alignment so a Definition row struct matches the decoded
// record. Strings stay as offsets into the reconstructed string table (see DB2File::Str).
//
// wxl-db2's second DB2 reader: Wdc5.hpp/cpp handles WDC5 declaratively (named fields via Definition);
// this one handles the older WDC1/2/3 family the retail path-table DB2s (TextureFilePath.db2,
// ModelFilePath.db2, TextureFileData.db2) ship as, including inline string columns Wdc5 has no concept
// of. Named Decode (not Db2) to avoid colliding with Db2.hpp on a case-insensitive filesystem.
namespace ModernM2::Db2
{
    struct DB2Decoded
    {
        uint32_t             rowSize = 0;  // decoded bytes per record (column count * 4, plus relationship)
        std::vector<char>    records;      // rowCount * rowSize
        std::vector<int32_t> ids;          // rowCount
        std::vector<char>    strings;      // reconstructed string table
        bool                 hasRelationship = false; // a trailing relationship column was appended
    };

    // Decode the file bytes into out. stringColumns/stringColumnCount may be null/0 (see DB2File).
    // Returns false (and leaves out cleared) if the data is not a supported DB2 or is malformed.
    bool DecodeDB2(const uint8_t* data, uint32_t size, DB2Decoded& out, const uint32_t* stringColumns,
                   uint32_t stringColumnCount);
}
