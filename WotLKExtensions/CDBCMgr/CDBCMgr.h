#pragma once

#include <functional>
#include <unordered_map>
#include <string>
#include <any>
#include <limits>

class CDBCMgr
{
	using CDBC = std::unordered_map<int, std::any>;

public:
	std::unordered_map<std::string, CDBC> allCDBCs;
	std::unordered_map<std::string, std::pair<int, int>> cdbcIndexRanges;
	// Type-erased row writers so network DBC patches (raw bytes + id) can land in a CDBC
	// without the concrete row type at the call site. Registered per CDBC in its LoadDB.
	std::unordered_map<std::string, std::function<bool(int, const void*, uint32_t)>> rowWriters;
	static void Load();
	void addCDBC(std::string cdbcName);
	// these stay in .h because haha template
	template <typename T>
	void addRow(std::string cdbcName, int rowIndex, T row)
	{
		allCDBCs[cdbcName][rowIndex] = row;
	}

	// Remember how to turn a raw record image into a stored row of type T.
	template <typename T>
	void registerRowType(std::string cdbcName)
	{
		rowWriters[cdbcName] = [this, cdbcName](int rowIndex, const void* bytes, uint32_t size) -> bool
		{
			if (size != sizeof(T))
				return false;
			addRow<T>(cdbcName, rowIndex, *static_cast<const T*>(bytes));
			return true;
		};
	}

	bool hasCDBC(const std::string& cdbcName) const
	{
		return allCDBCs.count(cdbcName) != 0;
	}
	bool hasRowWriter(const std::string& cdbcName) const
	{
		return rowWriters.count(cdbcName) != 0;
	}

	bool patchRow(const std::string& cdbcName, int rowIndex, const void* bytes, uint32_t size)
	{
		auto it = rowWriters.find(cdbcName);
		if (it == rowWriters.end() || !it->second(rowIndex, bytes, size))
			return false;
		auto& r = cdbcIndexRanges[cdbcName];
		if (r.first == 0 && r.second == 0)
			r = { rowIndex, rowIndex };
		else
		{
			if (rowIndex < r.first)
				r.first = rowIndex;
			if (rowIndex > r.second)
				r.second = rowIndex;
		}
		return true;
	}
	template <typename T>
	T* getRow(std::string cdbcName, int rowIndex)
	{
		auto it = allCDBCs.find(cdbcName);
		if (it != allCDBCs.end())
		{
			auto objIt = it->second.find(rowIndex);
			if (objIt != it->second.end())
				return std::any_cast<T>(&objIt->second);
		}
		return nullptr;
	}

	std::pair<int, int> getIndexRange(std::string cdbcName)
	{
		auto it = cdbcIndexRanges.find(cdbcName);

		if (it != cdbcIndexRanges.end())
			return it->second;

		return { 0, 0 };
	}

	void setIndexRange(std::string cdbcName, uint32_t minIndex, uint32_t maxIndex)
	{
		auto it = cdbcIndexRanges.find(cdbcName);
		if (it != cdbcIndexRanges.end())
			it->second = { minIndex, maxIndex };
		else
			it->second = { 0, 0 };
	}
};

extern CDBCMgr GlobalCDBCMap;
