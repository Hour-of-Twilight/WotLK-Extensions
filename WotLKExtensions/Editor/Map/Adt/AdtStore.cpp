#include <Editor/Map/Adt/AdtStore.h>

#include <Editor/Map/Adt/AdtDocument.h>

#include <cstdio>
#include <fstream>

namespace MapEditor::Adt
{
	namespace
	{
		char const* Name(char const* mapName)
		{
			return mapName && *mapName ? mapName : "";
		}

		bool WriteWholeFile(fs::path const& path, uint8_t const* data, size_t size,
		    std::string& error)
		{
			std::error_code ec;
			fs::create_directories(path.parent_path(), ec);

			std::ofstream out(path, std::ios::binary | std::ios::trunc);
			if (!out)
			{
				error = "cannot open " + path.string() + " for writing";
				return false;
			}

			if (size)
				out.write(reinterpret_cast<char const*>(data), static_cast<std::streamsize>(size));

			out.close();
			if (!out)
			{
				error = "write failed on " + path.string();
				return false;
			}

			return true;
		}

		bool ReadWholeFile(fs::path const& path, std::vector<uint8_t>& out, std::string& error)
		{
			std::ifstream in(path, std::ios::binary | std::ios::ate);
			if (!in)
			{
				error = "cannot open " + path.string();
				return false;
			}

			std::streamoff size = in.tellg();
			if (size < 0)
			{
				error = "cannot size " + path.string();
				return false;
			}

			in.seekg(0);
			out.assign(static_cast<size_t>(size), 0);
			if (size)
				in.read(reinterpret_cast<char*>(out.data()), size);

			if (!in)
			{
				error = "short read on " + path.string();
				return false;
			}

			return true;
		}
	}

	fs::path DiskTilePath(char const* mapName, int32_t tileX, int32_t tileY)
	{
		char leaf[128];
		std::snprintf(leaf, sizeof(leaf), "%s_%d_%d.adt", Name(mapName), tileX, tileY);

		return Util::GetExeDir() / "World" / "Maps" / Name(mapName) / leaf;
	}

	fs::path BackupTilePath(char const* mapName, int32_t tileX, int32_t tileY)
	{
		fs::path path = DiskTilePath(mapName, tileX, tileY);
		path += ".bak";
		return path;
	}

	bool SaveTile(char const* mapName, int32_t tileX, int32_t tileY,
	    std::vector<uint8_t> const& bytes, std::string& error)
	{
		if (bytes.empty())
		{
			error = "refusing to save an empty tile";
			return false;
		}

		fs::path backup = BackupTilePath(mapName, tileX, tileY);

		std::error_code ec;
		if (!fs::exists(backup, ec))
		{
			// Read through SFile rather than off disk so the backup captures the tile as the
			// client currently sees it, whether that is a loose override or the MPQ copy.
			std::string archivePath = TilePath(mapName, tileX, tileY);
			std::vector<uint8_t> pristine;
			if (!ReadFileBytes(archivePath.c_str(), pristine, error))
			{
				error = "no backup taken, refusing to save (" + error + ")";
				return false;
			}

			if (!WriteWholeFile(backup, pristine.data(), pristine.size(), error))
				return false;
		}

		return WriteWholeFile(DiskTilePath(mapName, tileX, tileY), bytes.data(), bytes.size(),
		    error);
	}

	bool RestoreTile(char const* mapName, int32_t tileX, int32_t tileY, std::string& error)
	{
		fs::path backup = BackupTilePath(mapName, tileX, tileY);

		std::error_code ec;
		if (!fs::exists(backup, ec))
		{
			error = "no backup at " + backup.string();
			return false;
		}

		std::vector<uint8_t> pristine;
		if (!ReadWholeFile(backup, pristine, error))
			return false;

		return WriteWholeFile(DiskTilePath(mapName, tileX, tileY), pristine.data(),
		    pristine.size(), error);
	}
}
