#include <Editor/Map/TileSession.h>

#include <Editor/Map/Adt/AdtStore.h>
#include <Editor/Map/MapClient.h>

#include <cstring>
#include <memory>

namespace MapEditor::Session
{
	namespace
	{
		// unique_ptr because callers hold an OpenTile* across frames, and a vector of values would
		// invalidate every one of them the moment a second tile is opened.
		std::vector<std::unique_ptr<OpenTile>>& Tiles()
		{
			static std::vector<std::unique_ptr<OpenTile>> tiles;
			return tiles;
		}

		std::string& CurrentMap()
		{
			static std::string name;
			return name;
		}

		void DropIfMapChanged()
		{
			char const* now = Access::sMapName;
			if (CurrentMap() == now)
				return;

			Tiles().clear();
			CurrentMap() = now;
		}
	}

	char const* MapName()
	{
		return CurrentMap().c_str();
	}

	void OnMapChanged()
	{
		Tiles().clear();
		CurrentMap().clear();
	}

	OpenTile* Find(int32_t tileX, int32_t tileY)
	{
		DropIfMapChanged();

		for (std::unique_ptr<OpenTile> const& tile : Tiles())
		{
			if (tile->tileX == tileX && tile->tileY == tileY)
				return tile.get();
		}

		return nullptr;
	}

	OpenTile* Open(int32_t tileX, int32_t tileY, std::string& error)
	{
		if (OpenTile* existing = Find(tileX, tileY))
			return existing;

		std::string path = Adt::TilePath(CurrentMap().c_str(), tileX, tileY);

		std::vector<uint8_t> bytes;
		if (!Adt::ReadFileBytes(path.c_str(), bytes, error))
			return nullptr;

		auto tile = std::make_unique<OpenTile>();
		tile->tileX = tileX;
		tile->tileY = tileY;

		if (!Adt::Read(bytes.data(), static_cast<uint32_t>(bytes.size()), tile->doc, error))
			return nullptr;

		Tiles().push_back(std::move(tile));
		return Tiles().back().get();
	}

	std::vector<OpenTile*> AllOpen()
	{
		DropIfMapChanged();

		std::vector<OpenTile*> out;
		out.reserve(Tiles().size());
		for (std::unique_ptr<OpenTile> const& tile : Tiles())
			out.push_back(tile.get());

		return out;
	}

	int32_t DirtyCount()
	{
		int32_t count = 0;
		for (std::unique_ptr<OpenTile> const& tile : Tiles())
			count += tile->dirty ? 1 : 0;

		return count;
	}

	bool SaveDirty(int32_t& saved, std::string& error)
	{
		saved = 0;

		for (std::unique_ptr<OpenTile>& tile : Tiles())
		{
			if (!tile->dirty)
				continue;

			std::vector<uint8_t> bytes;
			if (!Adt::Write(tile->doc, bytes, error))
				return false;

			if (!Adt::SaveTile(CurrentMap().c_str(), tile->tileX, tile->tileY, bytes, error))
				return false;

			tile->dirty = false;
			++saved;
		}

		return true;
	}

	bool SaveOne(int32_t tileX, int32_t tileY, std::string& error)
	{
		OpenTile* tile = Find(tileX, tileY);
		if (!tile)
		{
			error = "tile " + std::to_string(tileX) + "_" + std::to_string(tileY) + " is not open";
			return false;
		}

		std::vector<uint8_t> bytes;
		if (!Adt::Write(tile->doc, bytes, error))
			return false;

		if (!Adt::SaveTile(CurrentMap().c_str(), tileX, tileY, bytes, error))
			return false;

		tile->dirty = false;
		return true;
	}

	int32_t RevertAll()
	{
		int32_t reverted = 0;
		std::string error;

		for (std::unique_ptr<OpenTile> const& tile : Tiles())
		{
			if (Adt::RestoreTile(CurrentMap().c_str(), tile->tileX, tile->tileY, error))
				++reverted;

			// Purge either way. If there was no backup the tile was never saved, and reloading it
			// is still the right way to throw away whatever was painted into the live buffer.
			Access::ReloadTile(tile->tileX, tile->tileY);
		}

		Tiles().clear();
		return reverted;
	}

	void CloseAll()
	{
		Tiles().clear();
	}
}
