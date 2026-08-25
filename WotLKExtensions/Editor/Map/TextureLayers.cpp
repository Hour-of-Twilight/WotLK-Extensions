#include <Editor/Map/TextureLayers.h>

#include <ClientData/Streaming.h>
#include <Editor/Map/Adt/AlphaMap.h>
#include <Editor/Map/MapClient.h>
#include <Editor/Map/MapCoords.h>
#include <Editor/Map/TextureBrush.h>
#include <Editor/Map/TileSession.h>

#include <cstring>
#include <deque>
#include <map>

namespace MapEditor::TextureLayers
{
	namespace
	{
		constexpr int32_t kSide = Access::kChunksPerTileSide;

		// MTXF bit 0, which stops CMap::LoadTerrainTexture reaching for the "_s.blp" companion.
		constexpr int32_t kTextureNoSpecular = 0x1;

		// Texture names the editor has handed the client. A live texture entry only borrows its
		// name and nothing ever frees them, so these have to outlive every tile that might point at
		// one and a deque keeps the pointers good as more are added.
		std::deque<std::string>& OwnedNames()
		{
			static std::deque<std::string> names;
			return names;
		}

		// MTXF the editor owns, one per tile that has had a texture added. MTXF is one flags word
		// per texture and CMap::LoadTerrainTexture indexes it by texture id, so a tile that has
		// one has to keep it as long as its texture list.
		std::map<std::pair<int32_t, int32_t>, std::vector<int32_t>>& OwnedTextureFlags()
		{
			static std::map<std::pair<int32_t, int32_t>, std::vector<int32_t>> flags;
			return flags;
		}

		char Normalize(char c)
		{
			if (c == '/')
				return '\\';

			return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
		}

		// Forward slashes to backslashes, since that is what MTEX and the client's own name
		// comparisons expect. Typing the path with forward slashes is the easy way to dodge Lua
		// turning "\t" in a path into a tab before it ever reaches here.
		std::string ToGamePath(char const* text)
		{
			std::string out = text;
			for (char& c : out)
			{
				if (c == '/')
					c = '\\';
			}

			return out;
		}

		// A path that came through a Lua literal with single backslashes arrives mangled: "\I"
		// loses its backslash and "\t" is already a tab. Catching the control character is what
		// makes that a message instead of a layer pointing at a file that cannot exist.
		bool LooksMangled(std::string const& path)
		{
			for (char c : path)
			{
				if (static_cast<unsigned char>(c) < 0x20)
					return true;
			}

			return false;
		}

		// Texture paths are compared the way the game treats them, case blind and with either
		// slash, so the same file named two ways does not become two layers.
		bool SamePath(std::string const& a, std::string const& b)
		{
			if (a.size() != b.size())
				return false;

			for (size_t i = 0; i < a.size(); ++i)
			{
				if (Normalize(a[i]) != Normalize(b[i]))
					return false;
			}

			return true;
		}

		// MTEX split the way CMapArea::LoadTextures (0x007D6D20) splits it. Worth matching
		// exactly: the client makes an entry out of every terminator it passes, empty strings
		// included, so any other reading would put the document's texture ids out of step with
		// the live list that SMLayer::textureId actually indexes.
		std::vector<std::string> SplitMtex(std::vector<uint8_t> const& blob)
		{
			std::vector<std::string> out;

			size_t at = 0;
			while (at < blob.size())
			{
				size_t length = 0;
				while (at + length < blob.size() && blob[at + length])
					++length;

				out.emplace_back(reinterpret_cast<char const*>(blob.data()) + at, length);
				at += length + 1;
			}

			return out;
		}

		void JoinMtex(std::vector<std::string> const& names, std::vector<uint8_t>& out)
		{
			out.clear();
			for (std::string const& name : names)
			{
				out.insert(out.end(), name.begin(), name.end());
				out.push_back(0);
			}
		}

		Adt::TopChunk* FindTop(Adt::AdtDocument& doc, uint32_t id)
		{
			for (Adt::TopChunk& top : doc.order)
			{
				if (top.id == id)
					return &top;
			}

			return nullptr;
		}

		// Inserts a top level chunk the tile does not have. Where it lands hardly matters, the
		// client finds everything through the MHDR offsets the writer recomputes, but the usual
		// order keeps other tools reading the tile.
		Adt::TopChunk* EnsureTop(Adt::AdtDocument& doc, uint32_t id, uint32_t after)
		{
			if (Adt::TopChunk* existing = FindTop(doc, id))
				return existing;

			size_t at = doc.order.size();
			for (size_t i = 0; i < doc.order.size(); ++i)
			{
				if (doc.order[i].id == after)
				{
					at = i + 1;
					break;
				}

				if (doc.order[i].mcnkIndex >= 0)
				{
					at = i;
					break;
				}
			}

			Adt::TopChunk chunk;
			chunk.id = id;
			return &*doc.order.insert(doc.order.begin() + static_cast<ptrdiff_t>(at), chunk);
		}

		// The specular companion the client looks for, ".blp" swapped for "_s.blp".
		std::string SpecularPath(std::string const& path)
		{
			size_t dot = path.find_last_of('.');
			return (dot == std::string::npos ? path : path.substr(0, dot)) + "_s.blp";
		}

		bool FileExists(std::string const& path)
		{
			void* handle = nullptr;
			if (!ClientData::Streaming::OpenFileEx(nullptr, path.c_str(), 0, &handle) || !handle)
				return false;

			ClientData::Streaming::CloseFile(handle);
			return true;
		}

		// Finds a texture in the tile's list, adding it if it is not there yet. The document and
		// the client's live list are grown together, because SMLayer::textureId indexes both and
		// the two have to agree the moment a layer starts using it.
		bool EnsureTexture(Session::OpenTile& tile, CMapArea* area, char const* texture,
		    uint32_t& textureId, std::string& error)
		{
			Adt::TopChunk* mtex = FindTop(tile.doc, Adt::kMTEX);
			if (!mtex)
			{
				error = "tile has no MTEX";
				return false;
			}

			std::vector<std::string> names = SplitMtex(mtex->data);

			// The client counts MTEX itself on load, so a disagreement here means one of them has
			// been read wrong and a texture id would point somewhere else entirely.
			if (names.size() != area->textureCount)
			{
				error = "tile texture list does not match MTEX";
				return false;
			}

			std::string wanted = ToGamePath(texture);
			for (size_t i = 0; i < names.size(); ++i)
			{
				if (SamePath(names[i], wanted))
				{
					textureId = static_cast<uint32_t>(i);
					return true;
				}
			}

			OwnedNames().push_back(wanted);
			char const* stable = OwnedNames().back().c_str();

			uint32_t added = 0;
			if (!Access::AppendTerrainTexture(area, stable, added))
			{
				OwnedNames().pop_back();
				error = "could not grow the tile texture list";
				return false;
			}

			names.push_back(wanted);
			JoinMtex(names, mtex->data);

			// MTXF runs one flags word per texture and LoadTerrainTexture reads it by texture id, so
			// a tile that has one needs an entry for the new texture or the client reads past the
			// end of it. Bit 0 also has to be right: with it clear the client loads the "_s.blp"
			// companion in place of the texture itself and never falls back, so a texture that has
			// no specular map renders as nothing at all.
			bool noSpecular = !FileExists(SpecularPath(wanted));

			Adt::TopChunk* mtxf = FindTop(tile.doc, Adt::kMTXF);
			if (!mtxf && noSpecular)
				mtxf = EnsureTop(tile.doc, Adt::kMTXF, Adt::kMH2O);

			if (mtxf)
			{
				mtxf->data.resize(names.size() * sizeof(int32_t), 0);

				if (noSpecular)
				{
					int32_t flags = kTextureNoSpecular;
					std::memcpy(mtxf->data.data() + added * sizeof(int32_t), &flags, sizeof(flags));
				}

				std::vector<int32_t>& owned = OwnedTextureFlags()[{tile.tileX, tile.tileY}];
				owned.assign(names.size(), 0);
				std::memcpy(owned.data(), mtxf->data.data(), mtxf->data.size());
				area->textureFlags = owned.data();
			}

			tile.dirty = true;
			textureId = added;
			return true;
		}

		// Reads a chunk's layers, changes them, and puts the result back everywhere it has to go.
		// Returns false when the chunk was left alone, which is not an error.
		template <typename Edit>
		bool EditChunk(Session::OpenTile& tile, CMapChunk* chunk, int32_t chunkX, int32_t chunkY,
		    Edit&& edit)
		{
			Adt::Mcnk* mcnk = tile.doc.ChunkAt(chunkX, chunkY);
			if (!mcnk)
				return false;

			Adt::AlphaFormat format = Adt::FormatFor(*mcnk, *Access::sMapFlags);

			Adt::ChunkAlpha alpha;
			std::string error;
			if (!Adt::ReadChunkAlpha(*mcnk, format, alpha, error))
				return false;

			if (!edit(alpha))
				return false;

			if (!Adt::WriteChunkAlpha(alpha, format, *mcnk, error))
				return false;

			tile.dirty = true;
			TextureBrush::PushChunk(tile.tileX, tile.tileY, chunkX, chunkY, chunk, *mcnk);
			return true;
		}

		// Whether the circle reaches any chunk of a tile, so a tile that is only clipped by the
		// bounding square is never opened off disk.
		bool CircleTouchesTile(CMapArea* area, C3Vector const& center, float radius)
		{
			for (int32_t i = 0; i < Access::kChunksPerTile; ++i)
			{
				if (Coords::CircleTouchesChunk(area->mapChunks[i], center, radius))
					return true;
			}

			return false;
		}

		// Walks the chunks a brush circle reaches, opening each tile once.
		template <typename Visit>
		void ForEachChunk(C3Vector const& center, float radius, Visit&& visit)
		{
			// Tile coordinates run backwards along both world axes, so the corner that gives the
			// low index is the one at plus radius.
			C3Vector lo{center.x + radius, center.y + radius, 0.0f};
			C3Vector hi{center.x - radius, center.y - radius, 0.0f};

			for (int32_t tileY = Coords::TileY(lo); tileY <= Coords::TileY(hi); ++tileY)
			{
				for (int32_t tileX = Coords::TileX(lo); tileX <= Coords::TileX(hi); ++tileX)
				{
					CMapArea* area = Access::GetArea(tileX, tileY);
					if (!Access::IsAreaReady(area) || !CircleTouchesTile(area, center, radius))
						continue;

					std::string error;
					Session::OpenTile* tile = Session::Open(tileX, tileY, error);
					if (!tile)
						continue;

					for (int32_t chunkY = 0; chunkY < kSide; ++chunkY)
					{
						for (int32_t chunkX = 0; chunkX < kSide; ++chunkX)
						{
							CMapChunk* chunk = Access::GetChunk(area, chunkX, chunkY);
							if (!Coords::CircleTouchesChunk(chunk, center, radius))
								continue;

							visit(area, *tile, chunk, chunkX, chunkY);
						}
					}
				}
			}
		}
	}

	void Reset()
	{
		OwnedNames().clear();
		OwnedTextureFlags().clear();
	}

	bool ListTileTextures(int32_t tileX, int32_t tileY, std::vector<std::string>& out,
	    std::string& error)
	{
		out.clear();

		Session::OpenTile* tile = Session::Open(tileX, tileY, error);
		if (!tile)
			return false;

		Adt::TopChunk* mtex = FindTop(tile->doc, Adt::kMTEX);
		if (!mtex)
		{
			error = "tile has no MTEX";
			return false;
		}

		out = SplitMtex(mtex->data);
		return true;
	}

	int32_t Add(C3Vector const& center, float radius, char const* texture, int32_t& skipped,
	    std::string& error)
	{
		skipped = 0;

		if (!texture || !*texture || radius <= 0.0f)
		{
			error = "no texture given";
			return 0;
		}

		if (LooksMangled(texture))
		{
			error = "that path has a control character in it, so Lua ate the backslashes - "
			        "double them or use forward slashes";
			return 0;
		}

		int32_t added = 0;

		ForEachChunk(center, radius,
		    [&](CMapArea* area, Session::OpenTile& tile, CMapChunk* chunk, int32_t chunkX,
		        int32_t chunkY)
		    {
			    uint32_t textureId = 0;
			    if (!EnsureTexture(tile, area, texture, textureId, error))
			    {
				    ++skipped;
				    return;
			    }

			    bool changed = EditChunk(tile, chunk, chunkX, chunkY,
			        [&](Adt::ChunkAlpha& alpha)
			        {
				        if (alpha.layers.size() >= kMaxLayers)
					        return false;

				        for (SMLayer const& layer : alpha.layers)
				        {
					        if (layer.textureId == textureId)
						        return false;
				        }

				        SMLayer fresh{};
				        fresh.textureId = textureId;
				        fresh.flags = Adt::kLayerUseAlpha;

				        // Ground effect doodads are keyed off the layer, and a texture the tile
				        // never had cannot inherit one. Zero is none, which is the honest answer
				        // until there is a way to pick.
				        fresh.effectId = 0;

				        alpha.layers.push_back(fresh);

				        // Covering nothing to start with. WriteChunkAlpha fills in the flags and
				        // the MCAL offset, so the mask is all this has to supply.
				        alpha.maps.emplace_back();
				        return true;
			        });

			    if (changed)
				    ++added;
			    else
				    ++skipped;
		    });

		if (!added && error.empty())
			error = "every chunk already had it, or was already at four layers";

		return added;
	}

	int32_t Remove(C3Vector const& center, float radius, int32_t layer, int32_t& skipped,
	    std::string& error)
	{
		skipped = 0;

		if (layer <= 0 || layer >= kMaxLayers)
		{
			error = "layer 0 is the base and cannot be removed";
			return 0;
		}

		int32_t removed = 0;

		ForEachChunk(center, radius,
		    [&](CMapArea*, Session::OpenTile& tile, CMapChunk* chunk, int32_t chunkX,
		        int32_t chunkY)
		    {
			    bool changed = EditChunk(tile, chunk, chunkX, chunkY,
			        [&](Adt::ChunkAlpha& alpha)
			        {
				        if (layer >= static_cast<int32_t>(alpha.layers.size()))
					        return false;

				        // Nothing has to be redistributed. Coverage of the base is whatever the
				        // masks leave over, so dropping a mask hands exactly what it held back to
				        // the layers underneath, which is what removing a layer should mean.
				        alpha.layers.erase(alpha.layers.begin() + layer);
				        alpha.maps.erase(alpha.maps.begin() + layer);
				        return true;
			        });

			    if (changed)
				    ++removed;
			    else
				    ++skipped;
		    });

		if (!removed && error.empty())
			error = "no chunk under the brush had that layer";

		return removed;
	}
}
