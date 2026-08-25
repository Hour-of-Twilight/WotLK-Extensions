#include <Editor/Map/Placements.h>

#include <ClientData/SharedDefines.h>
#include <ClientData/VectorMath.h>
#include <Editor/Map/Adt/AdtDocument.h>
#include <Editor/Map/MapClient.h>
#include <Editor/Map/MapCoords.h>
#include <Editor/Map/TileSession.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_set>

namespace MapEditor::Placements
{
	namespace
	{
		constexpr float kPi = 3.14159265f;
		constexpr float kDegToRad = kPi / 180.0f;

		// SMDoodadDef::scale is fixed point, 1024 meaning 1.0. flt_A1C8A0 is the 1/1024 the loader
		// multiplies by.
		constexpr float kScaleUnit = 1.0f / 1024.0f;

		// Where a deleted instance gets parked until the tile reloads. Far enough down that nothing
		// draws or collides, close enough that the float stays exact.
		constexpr float kBuriedZ = -30000.0f;

		CAaBox Normalized(CAaBox const& box)
		{
			CAaBox out;
			out.b.x = box.b.x < box.t.x ? box.b.x : box.t.x;
			out.b.y = box.b.y < box.t.y ? box.b.y : box.t.y;
			out.b.z = box.b.z < box.t.z ? box.b.z : box.t.z;
			out.t.x = box.b.x < box.t.x ? box.t.x : box.b.x;
			out.t.y = box.b.y < box.t.y ? box.t.y : box.b.y;
			out.t.z = box.b.z < box.t.z ? box.t.z : box.b.z;
			return out;
		}

		bool BoxIsEmpty(CAaBox const& box)
		{
			return box.t.x - box.b.x <= 0.0f && box.t.y - box.b.y <= 0.0f && box.t.z - box.b.z <= 0.0f;
		}

		CAaBox BoxAround(C3Vector const& point, float pad)
		{
			return {{point.x - pad, point.y - pad, point.z - pad},
			    {point.x + pad, point.y + pad, point.z + pad}};
		}

		bool BoxIsFinite(CAaBox const& box)
		{
			return std::isfinite(box.b.x) && std::isfinite(box.b.y) && std::isfinite(box.b.z)
			    && std::isfinite(box.t.x) && std::isfinite(box.t.y) && std::isfinite(box.t.z);
		}

		float BoxDiagonal(CAaBox const& box)
		{
			float dx = box.t.x - box.b.x;
			float dy = box.t.y - box.b.y;
			float dz = box.t.z - box.b.z;
			return std::sqrt(dx * dx + dy * dy + dz * dz);
		}

		float ClampToRange(float value, float lo, float hi)
		{
			return value < lo ? lo : (value > hi ? hi : value);
		}

		// Zero when the point is inside, otherwise the straight line gap to the nearest face. The
		// z axis is left out because everything that asks is really asking about ground distance.
		float BoxDistanceSqXY(CAaBox const& box, C3Vector const& point)
		{
			float dx = point.x - ClampToRange(point.x, box.b.x, box.t.x);
			float dy = point.y - ClampToRange(point.y, box.b.y, box.t.y);
			return dx * dx + dy * dy;
		}

		float DistanceSqXY(C3Vector const& a, C3Vector const& b)
		{
			float dx = a.x - b.x;
			float dy = a.y - b.y;
			return dx * dx + dy * dy;
		}

		// Nothing in the game data comes close to this, and a doodad on the far side of the zone
		// is not what a click near the camera meant either.
		constexpr float kMaxSaneBoxSpan = 2000.0f;
		constexpr float kMaxBoxDriftSq = 500.0f * 500.0f;

		// Bounds are only worth trusting when they are finite, not absurd, and actually sit near
		// the placement they belong to. A def whose bounds were never filled in holds whatever the
		// allocator left, and one bogus box swallowing the camera wins every pick at zero distance
		// and shuts out everything else, WMOs included.
		bool BoxUsable(CAaBox const& box, C3Vector const& origin)
		{
			if (!BoxIsFinite(box) || BoxIsEmpty(box))
				return false;

			if (box.t.x - box.b.x > kMaxSaneBoxSpan || box.t.y - box.b.y > kMaxSaneBoxSpan
			    || box.t.z - box.b.z > kMaxSaneBoxSpan)
				return false;

			return BoxDistanceSqXY(box, origin) <= kMaxBoxDriftSq;
		}

		CAaBox FallbackBounds(C3Vector const& position, float sphereRadius)
		{
			float pad = sphereRadius;
			if (!std::isfinite(pad) || pad <= 0.0f || pad > kMaxSaneBoxSpan)
				pad = 1.0f;

			return BoxAround(position, pad);
		}

		CAaBox Merge(CAaBox const& a, CAaBox const& b)
		{
			CAaBox out;
			out.b.x = a.b.x < b.b.x ? a.b.x : b.b.x;
			out.b.y = a.b.y < b.b.y ? a.b.y : b.b.y;
			out.b.z = a.b.z < b.b.z ? a.b.z : b.b.z;
			out.t.x = a.t.x > b.t.x ? a.t.x : b.t.x;
			out.t.y = a.t.y > b.t.y ? a.t.y : b.t.y;
			out.t.z = a.t.z > b.t.z ? a.t.z : b.t.z;
			return out;
		}

		// Slab test against an axis aligned box, with the ray parameter running 0 at start to 1 at
		// end so the fraction can be compared straight across candidates.
		bool RayHitsBox(C3Vector const& start, C3Vector const& dir, CAaBox const& box, float& tHit)
		{
			float const s[3] = {start.x, start.y, start.z};
			float const d[3] = {dir.x, dir.y, dir.z};
			float const lo[3] = {box.b.x, box.b.y, box.b.z};
			float const hi[3] = {box.t.x, box.t.y, box.t.z};

			float entry = 0.0f;
			float exit = 1.0f;

			for (int32_t axis = 0; axis < 3; ++axis)
			{
				if (std::fabs(d[axis]) < 1e-6f)
				{
					if (s[axis] < lo[axis] || s[axis] > hi[axis])
						return false;

					continue;
				}

				float t0 = (lo[axis] - s[axis]) / d[axis];
				float t1 = (hi[axis] - s[axis]) / d[axis];
				if (t0 > t1)
				{
					float swap = t0;
					t0 = t1;
					t1 = swap;
				}

				if (t0 > entry)
					entry = t0;
				if (t1 < exit)
					exit = t1;
				if (entry > exit)
					return false;
			}

			tHit = entry;
			return true;
		}

		// The transform matrix CMap::CreateDoodadDef builds, in exactly its order: translation
		// written straight into the identity's bottom row, then yaw with a half turn added, then
		// pitch, then roll, then a uniform scale.
		C44Matrix BuildDoodadMatrix(C3Vector const& world, C3Vector const& rotation, float scale)
		{
			C44Matrix mat;
			mat.d0 = world.x;
			mat.d1 = world.y;
			mat.d2 = world.z;

			Access::C44Matrix_RotateAroundZ(&mat, rotation.y * kDegToRad + kPi);
			Access::C44Matrix_RotateAroundY(&mat, rotation.x * kDegToRad);
			Access::C44Matrix_RotateAroundX(&mat, rotation.z * kDegToRad);
			Access::C44Matrix_Scale(&mat, scale);
			return mat;
		}

		uint16_t PackScale(float scale)
		{
			float raw = scale * 1024.0f;
			if (raw < 1.0f)
				raw = 1.0f;
			if (raw > 65535.0f)
				raw = 65535.0f;

			return static_cast<uint16_t>(raw + 0.5f);
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

		SMDoodadDef* DocDoodads(Session::OpenTile& tile, size_t& count)
		{
			count = 0;

			Adt::TopChunk* top = FindTop(tile.doc, Adt::kMDDF);
			if (!top)
				return nullptr;

			count = top->data.size() / sizeof(SMDoodadDef);
			return count ? reinterpret_cast<SMDoodadDef*>(top->data.data()) : nullptr;
		}

		SMMapObjDef* DocMapObjs(Session::OpenTile& tile, size_t& count)
		{
			count = 0;

			Adt::TopChunk* top = FindTop(tile.doc, Adt::kMODF);
			if (!top)
				return nullptr;

			count = top->data.size() / sizeof(SMMapObjDef);
			return count ? reinterpret_cast<SMMapObjDef*>(top->data.data()) : nullptr;
		}

		// The client mints ids for the defs it builds itself, counting down from 0xFFFFFFFE
		// (dword_CE04A4, seeded at 0x0079E88B), and Blizzard's own file ids are all small, so the
		// middle of the range belongs to nobody. New placements start there and the ceiling keeps
		// a runtime id that wandered into view from dragging the counter up with it.
		constexpr uint32_t kUniqueIdBase = 0x40000000;
		constexpr uint32_t kUniqueIdCeiling = 0x80000000;

		// A WMO the game files do not have, or one we could not read, gets a box this big. Too
		// large only means it is considered for drawing sooner than it needs to be, too small means
		// it pops out of view, so err upwards.
		constexpr float kUnknownMapObjRadius = 200.0f;

		// Forward slashes to backslashes, the way MMDX and MWMO store them. Typing the path with
		// forward slashes is also the easy way to dodge Lua turning "\t" into a tab on the way in.
		std::string GamePath(char const* text)
		{
			std::string out = text ? text : "";
			for (char& c : out)
			{
				if (c == '/')
					c = '\\';
			}

			while (!out.empty() && out.back() == ' ')
				out.pop_back();

			return out;
		}

		// A path that came through a Lua literal with single backslashes arrives mangled: "\I" lost
		// its backslash and "\t" is already a tab. Catching the control character turns that into a
		// message instead of an entry pointing at a file that cannot exist.
		bool LooksMangled(std::string const& path)
		{
			for (char c : path)
			{
				if (static_cast<unsigned char>(c) < 0x20)
					return true;
			}

			return false;
		}

		// MMDX paths are written .mdx or .mdl and CM2Cache::CreateShared (0x0081C390) swaps either
		// for .m2 before it opens anything, so the file on disk is under a name the entry never
		// mentions. Both spellings have to be tried or every stock doodad looks missing.
		bool ModelExists(std::string const& path)
		{
			if (SFile::FileExistsEx(path.c_str(), 0))
				return true;

			size_t dot = path.find_last_of('.');
			if (dot == std::string::npos)
				return false;

			std::string swapped = path.substr(0, dot) + ".m2";
			return SFile::FileExistsEx(swapped.c_str(), 0) != 0;
		}

		// MODF extents are not decoration. CMap::CreateMapObjDef (0x007BF460) copies them into the
		// live bbox at +0x48 and derives the culling sphere at +0x6C from them and nothing ever
		// recomputes them, so a new WMO gets a cube sized from the root MOHD bounds, which holds it
		// at any rotation.
		float MapObjRadius(std::string const& path)
		{
			std::vector<uint8_t> bytes;
			std::string ignored;
			if (!Adt::ReadFileBytes(path.c_str(), bytes, ignored))
				return kUnknownMapObjRadius;

			size_t at = 0;
			while (at + 8 <= bytes.size())
			{
				uint32_t id = 0;
				uint32_t size = 0;
				std::memcpy(&id, bytes.data() + at, sizeof(id));
				std::memcpy(&size, bytes.data() + at + 4, sizeof(size));
				at += 8;

				if (size > bytes.size() - at)
					break;

				// MOHD counts and colours first, then the bounds at 0x24 and 0x30.
				if (id == Adt::ChunkId("MOHD") && size >= 0x3C)
				{
					float corners[6] = {};
					std::memcpy(corners, bytes.data() + at + 0x24, sizeof(corners));

					float radius = 0.0f;
					for (float value : corners)
					{
						if (std::isfinite(value) && std::fabs(value) > radius)
							radius = std::fabs(value);
					}

					// Three axes at that reach, so the corner is sqrt(3) further out.
					radius *= 1.7320508f;
					return radius > 1.0f && radius < 4000.0f ? radius : kUnknownMapObjRadius;
				}

				at += size;
			}

			return kUnknownMapObjRadius;
		}

		// Inserts a top level chunk the tile does not have. Where it lands in the file barely
		// matters, since the client finds everything through MHDR offsets the writer recomputes,
		// but keeping the usual order means other tools still read the tile.
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

				// Past the terrain there is nowhere sensible left, so settle for just before it.
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

		// MMDX and MWMO are a run of NUL terminated paths, MMID and MWID a parallel array of byte
		// offsets into that run. An MDDF or MODF nameId indexes the offset array, which makes it a
		// number that only means anything inside its own tile.
		char const* DocName(Session::OpenTile& tile, bool doodad, uint32_t nameId)
		{
			Adt::TopChunk* ids = FindTop(tile.doc, doodad ? Adt::kMMID : Adt::kMWID);
			Adt::TopChunk* blob = FindTop(tile.doc, doodad ? Adt::kMMDX : Adt::kMWMO);
			if (!ids || !blob)
				return nullptr;

			if ((static_cast<size_t>(nameId) + 1) * sizeof(uint32_t) > ids->data.size())
				return nullptr;

			uint32_t offset = 0;
			std::memcpy(&offset, ids->data.data() + nameId * sizeof(uint32_t), sizeof(offset));
			if (offset >= blob->data.size())
				return nullptr;

			char const* text = reinterpret_cast<char const*>(blob->data.data()) + offset;
			if (!std::memchr(text, 0, blob->data.size() - offset))
				return nullptr;

			return text;
		}

		// ADT paths are stored with backslashes and whatever case the artist typed, so a byte for
		// byte compare would put the same model in the list twice.
		bool SameName(char const* a, char const* b)
		{
			for (; *a && *b; ++a, ++b)
			{
				char ca = *a == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(*a)));
				char cb = *b == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(*b)));
				if (ca != cb)
					return false;
			}

			return *a == *b;
		}

		// The tile's nameId for a model, adding it to the list when the tile has never referenced
		// that model before. Appending is safe at any point because every offset already stored
		// points at a byte before the new string, so nothing existing has to move.
		bool ResolveName(Session::OpenTile& tile, bool doodad, char const* name, uint32_t& nameId)
		{
			uint32_t blobId = doodad ? Adt::kMMDX : Adt::kMWMO;
			uint32_t idsId = doodad ? Adt::kMMID : Adt::kMWID;

			if (Adt::TopChunk* ids = FindTop(tile.doc, idsId))
			{
				size_t count = ids->data.size() / sizeof(uint32_t);
				for (size_t i = 0; i < count; ++i)
				{
					char const* text = DocName(tile, doodad, static_cast<uint32_t>(i));
					if (text && SameName(text, name))
					{
						nameId = static_cast<uint32_t>(i);
						return true;
					}
				}
			}

			// Both inserts first, since either can move the order vector out from under a pointer
			// taken before it.
			EnsureTop(tile.doc, blobId, doodad ? Adt::kMTEX : Adt::kMMID);
			EnsureTop(tile.doc, idsId, blobId);

			Adt::TopChunk* blob = FindTop(tile.doc, blobId);
			Adt::TopChunk* ids = FindTop(tile.doc, idsId);
			if (!blob || !ids)
				return false;

			uint32_t offset = static_cast<uint32_t>(blob->data.size());
			size_t length = std::strlen(name);
			blob->data.insert(blob->data.end(), name, name + length + 1);

			nameId = static_cast<uint32_t>(ids->data.size() / sizeof(uint32_t));

			uint8_t const* raw = reinterpret_cast<uint8_t const*>(&offset);
			ids->data.insert(ids->data.end(), raw, raw + sizeof(offset));
			return true;
		}

		// The world footprint of one chunk, derived from its indices rather than from the live
		// chunk, so this still answers for a tile the client has not prepared yet. Both axes count
		// down from the map edge, which is why the plus corner is the low index one.
		bool ChunkOverlaps(int32_t tileX, int32_t tileY, int32_t chunkX, int32_t chunkY, CAaBox const& bounds)
		{
			float maxY = Coords::kMapHalf - static_cast<float>(tileX * 16 + chunkX) * Coords::kChunkSize;
			float maxX = Coords::kMapHalf - static_cast<float>(tileY * 16 + chunkY) * Coords::kChunkSize;

			return bounds.t.x >= maxX - Coords::kChunkSize && bounds.b.x <= maxX
			    && bounds.t.y >= maxY - Coords::kChunkSize && bounds.b.y <= maxY;
		}

		// Rewrites one chunk's MCRF. `want` says whether the chunk should list the index at all,
		// `shift` whether indices above it slide down, which is only true when the entry itself is
		// leaving MDDF or MODF.
		void SetRef(Adt::Mcnk& mcnk, bool doodad, uint32_t index, bool want, bool shift)
		{
			Adt::SubChunk* mcrf = mcnk.Find(Adt::kMCRF);
			if (!mcrf)
				return;

			size_t doodadRefs = mcnk.header.nDoodadRefs;
			size_t mapObjRefs = mcnk.header.nMapObjRefs;
			size_t total = doodadRefs + mapObjRefs;
			if (mcrf->data.size() < total * sizeof(uint32_t))
				return;

			std::vector<uint32_t> refs(total);
			if (total)
				std::memcpy(refs.data(), mcrf->data.data(), total * sizeof(uint32_t));

			std::vector<uint32_t> doodadOut;
			std::vector<uint32_t> mapObjOut;
			doodadOut.reserve(doodadRefs + 1);
			mapObjOut.reserve(mapObjRefs + 1);
			bool present = false;

			for (size_t i = 0; i < total; ++i)
			{
				bool isDoodad = i < doodadRefs;
				std::vector<uint32_t>& target = isDoodad ? doodadOut : mapObjOut;
				uint32_t value = refs[i];

				if (isDoodad != doodad)
				{
					target.push_back(value);
					continue;
				}

				if (value == index)
				{
					if (!want)
						continue;

					present = true;
					target.push_back(value);
					continue;
				}

				target.push_back(shift && value > index ? value - 1 : value);
			}

			if (want && !present)
				(doodad ? doodadOut : mapObjOut).push_back(index);

			mcnk.header.nDoodadRefs = static_cast<uint32_t>(doodadOut.size());
			mcnk.header.nMapObjRefs = static_cast<uint32_t>(mapObjOut.size());

			mcrf->data.resize((doodadOut.size() + mapObjOut.size()) * sizeof(uint32_t));
			if (!doodadOut.empty())
				std::memcpy(mcrf->data.data(), doodadOut.data(), doodadOut.size() * sizeof(uint32_t));
			if (!mapObjOut.empty())
			{
				std::memcpy(mcrf->data.data() + doodadOut.size() * sizeof(uint32_t), mapObjOut.data(),
				    mapObjOut.size() * sizeof(uint32_t));
			}
		}

		// Puts the index into exactly the chunks the new footprint covers and takes it out of the
		// rest. MCRF is what decides whether a chunk draws an instance at all, so without this a
		// doodad dragged one chunk over simply vanishes the next time the tile loads.
		void RetargetRefs(Session::OpenTile& tile, bool doodad, uint32_t index, CAaBox const& bounds)
		{
			for (int32_t chunkY = 0; chunkY < 16; ++chunkY)
			{
				for (int32_t chunkX = 0; chunkX < 16; ++chunkX)
				{
					Adt::Mcnk* mcnk = tile.doc.ChunkAt(chunkX, chunkY);
					if (!mcnk)
						continue;

					bool want = ChunkOverlaps(tile.tileX, tile.tileY, chunkX, chunkY, bounds);
					SetRef(*mcnk, doodad, index, want, false);
				}
			}
		}

		void DropRefs(Session::OpenTile& tile, bool doodad, uint32_t index)
		{
			for (int32_t chunkY = 0; chunkY < 16; ++chunkY)
			{
				for (int32_t chunkX = 0; chunkX < 16; ++chunkX)
				{
					if (Adt::Mcnk* mcnk = tile.doc.ChunkAt(chunkX, chunkY))
						SetRef(*mcnk, doodad, index, false, true);
				}
			}
		}

		// Every live placement the client holds, found by walking each loaded chunk's link list. A
		// def straddling chunks is linked into all of them, so the same pointer comes back several
		// times and has to be deduped.
		template <typename Fn>
		void ForEachLiveDef(bool doodads, Fn&& fn)
		{
			std::unordered_set<void*> seen;

			for (int32_t tileY = 0; tileY < Access::kTilesPerSide; ++tileY)
			{
				for (int32_t tileX = 0; tileX < Access::kTilesPerSide; ++tileX)
				{
					CMapArea* area = Access::GetArea(tileX, tileY);
					if (!Access::IsAreaReady(area))
						continue;

					for (int32_t i = 0; i < Access::kChunksPerTile; ++i)
					{
						CMapChunk* chunk = area->mapChunks[i];
						if (!chunk)
							continue;

						TSExplicitList const& list = doodads ? chunk->doodadDefLink : chunk->mapObjDefLink;
						Access::ForEachLink(list,
						    [&](uint8_t* node)
						    {
							    void* owner = reinterpret_cast<CMapDefChunkLink*>(node)->owner;
							    if (owner && seen.insert(owner).second)
								    fn(owner);
						    });
					}
				}
			}
		}

		// bboxStaticEntity is the one CMapDoodadDef::CreateBounds (0x007B5740) rewrites every time
		// the doodad moves, on both the model-loaded and the still-streaming path. bboxDoodadDef at
		// 0xC0 is only written by the loaded path (0x007BDB10), so on a doodad whose m2 has not
		// arrived yet it is uninitialised heap, which is how a box a whole zone wide used to end up
		// wrapped around the camera.
		CAaBox DoodadBounds(CMapDoodadDef const* def)
		{
			CAaBox box = Normalized(def->bboxStaticEntity);
			if (BoxUsable(box, def->position))
				return box;

			return FallbackBounds(def->position, def->sphereRadius);
		}

		CAaBox MapObjBounds(CMapObjDef const* def)
		{
			CAaBox box = Normalized(def->bbox);
			if (BoxUsable(box, def->position))
				return box;

			return FallbackBounds(def->position, def->sphereRadius);
		}

		// The tile whose MDDF or MODF actually lists a placement is the one its position falls in.
		// A model overlapping the border is listed again in the neighbour with the same uniqueId,
		// but the client only ever builds one instance, so the containing tile is the one to edit.
		Session::OpenTile* OwningTile(C3Vector const& world, int32_t& tileX, int32_t& tileY)
		{
			tileX = Coords::TileX(world);
			tileY = Coords::TileY(world);
			if (!Access::IsValidTile(tileX, tileY))
				return nullptr;

			std::string ignored;
			return Session::Open(tileX, tileY, ignored);
		}

		bool RefForDoodad(CMapDoodadDef const* def, Ref& out)
		{
			int32_t tileX = 0;
			int32_t tileY = 0;
			Session::OpenTile* tile = OwningTile(def->position, tileX, tileY);
			if (!tile)
				return false;

			size_t count = 0;
			SMDoodadDef const* entries = DocDoodads(*tile, count);
			for (size_t i = 0; i < count; ++i)
			{
				if (entries[i].uniqueId != def->uniqueId)
					continue;

				out.kind = Kind::Doodad;
				out.tileX = tileX;
				out.tileY = tileY;
				out.index = static_cast<int32_t>(i);
				out.uniqueId = def->uniqueId;
				return true;
			}

			return false;
		}

		bool RefForMapObj(CMapObjDef const* def, Ref& out)
		{
			int32_t tileX = 0;
			int32_t tileY = 0;
			Session::OpenTile* tile = OwningTile(def->position, tileX, tileY);
			if (!tile)
				return false;

			size_t count = 0;
			SMMapObjDef const* entries = DocMapObjs(*tile, count);
			for (size_t i = 0; i < count; ++i)
			{
				if (entries[i].uniqueId != def->uniqueId)
					continue;

				out.kind = Kind::MapObj;
				out.tileX = tileX;
				out.tileY = tileY;
				out.index = static_cast<int32_t>(i);
				out.uniqueId = entries[i].uniqueId;
				return true;
			}

			return false;
		}

		// The live instance a file entry built, straight out of the client's own hash. A def is
		// taken back out of it by ~CMapObjDef, so a hit is by definition still alive.
		CMapObjDef* LiveMapObj(SMMapObjDef const& entry)
		{
			return Access::FindMapObjDef(entry.uniqueId);
		}

		std::string TileName(int32_t tileX, int32_t tileY)
		{
			return std::to_string(tileX) + "_" + std::to_string(tileY);
		}

		// Appends bytes to a top level chunk and hands back the index the entry landed at.
		uint32_t AppendEntry(Adt::TopChunk& top, void const* entry, size_t stride)
		{
			uint32_t index = static_cast<uint32_t>(top.data.size() / stride);
			uint8_t const* raw = static_cast<uint8_t const*>(entry);
			top.data.insert(top.data.end(), raw, raw + stride);
			return index;
		}

		void RaiseHighest(uint32_t& highest, uint32_t candidate)
		{
			if (candidate >= kUniqueIdBase && candidate < kUniqueIdCeiling && candidate >= highest)
				highest = candidate + 1;
		}

		// An id nothing in sight is using, meaning every open document plus every tile the client
		// currently has loaded. An id minted here could in principle collide with one saved in a
		// session where that tile was not loaded, but closing that would mean reading all 700
		// tiles.
		uint32_t AllocateUniqueId()
		{
			static uint32_t next = kUniqueIdBase;
			uint32_t highest = next;

			for (Session::OpenTile* tile : Session::AllOpen())
			{
				size_t count = 0;
				if (SMDoodadDef const* entries = DocDoodads(*tile, count))
				{
					for (size_t i = 0; i < count; ++i)
						RaiseHighest(highest, entries[i].uniqueId);
				}

				count = 0;
				if (SMMapObjDef const* entries = DocMapObjs(*tile, count))
				{
					for (size_t i = 0; i < count; ++i)
						RaiseHighest(highest, entries[i].uniqueId);
				}
			}

			for (int32_t tileY = 0; tileY < Access::kTilesPerSide; ++tileY)
			{
				for (int32_t tileX = 0; tileX < Access::kTilesPerSide; ++tileX)
				{
					CMapArea* area = Access::GetArea(tileX, tileY);
					if (!Access::IsAreaReady(area))
						continue;

					for (int32_t i = 0; i < area->doodadDefCount; ++i)
						RaiseHighest(highest, area->doodadDef[i].uniqueId);

					for (int32_t i = 0; i < area->mapObjDefCount; ++i)
						RaiseHighest(highest, area->mapObjDef[i].uniqueId);
				}
			}

			next = highest + 1;
			return highest;
		}

		bool InsertDoodad(Session::OpenTile& tile, std::string const& model, Transform const& transform,
		    uint16_t flags, Ref& out, std::string& error)
		{
			uint32_t nameId = 0;
			if (!ResolveName(tile, true, model.c_str(), nameId))
			{
				error = "could not add " + model + " to tile " + TileName(tile.tileX, tile.tileY);
				return false;
			}

			SMDoodadDef entry{};
			entry.nameId = nameId;
			entry.uniqueId = AllocateUniqueId();
			entry.position = Coords::WorldToAdt(transform.position);
			entry.rotation = transform.rotation;
			entry.scale = PackScale(transform.scale);
			entry.flags = flags;

			Adt::TopChunk* top = EnsureTop(tile.doc, Adt::kMDDF, Adt::kMWID);
			uint32_t index = AppendEntry(*top, &entry, sizeof(entry));

			// No live instance to measure, so the footprint is a guess. Whatever the model turns
			// out to be, the chunk it stands on has to list it or it will not draw at all, and the
			// tile reload that follows rebuilds the rest from the real bounds anyway.
			RetargetRefs(tile, true, index, BoxAround(transform.position, 8.0f));
			tile.dirty = true;

			out.kind = Kind::Doodad;
			out.tileX = tile.tileX;
			out.tileY = tile.tileY;
			out.index = static_cast<int32_t>(index);
			out.uniqueId = entry.uniqueId;
			return true;
		}

		bool InsertMapObj(Session::OpenTile& tile, std::string const& model, Transform const& transform,
		    CAaBox const& extents, uint16_t flags, uint16_t doodadSet, uint16_t nameSet, Ref& out,
		    std::string& error)
		{
			uint32_t nameId = 0;
			if (!ResolveName(tile, false, model.c_str(), nameId))
			{
				error = "could not add " + model + " to tile " + TileName(tile.tileX, tile.tileY);
				return false;
			}

			SMMapObjDef entry{};
			entry.nameId = nameId;
			entry.uniqueId = AllocateUniqueId();
			entry.position = Coords::WorldToAdt(transform.position);
			entry.rotation = transform.rotation;
			entry.extents = extents;
			entry.flags = flags;
			entry.doodadSet = doodadSet;
			entry.nameSet = nameSet;

			// MODF has a scale field the 3.3.5 loader never reads. It is 1024 in every stock tile,
			// so write what the files write.
			entry.scale = 1024;

			Adt::TopChunk* top = EnsureTop(tile.doc, Adt::kMODF, Adt::kMDDF);
			uint32_t index = AppendEntry(*top, &entry, sizeof(entry));

			CAaBox world = Merge(BoxAround(transform.position, 1.0f),
			    {Coords::AdtToWorld(extents.b), Coords::AdtToWorld(extents.t)});

			RetargetRefs(tile, false, index, Normalized(world));
			tile.dirty = true;

			out.kind = Kind::MapObj;
			out.tileX = tile.tileX;
			out.tileY = tile.tileY;
			out.index = static_cast<int32_t>(index);
			out.uniqueId = entry.uniqueId;
			return true;
		}

		bool MigrateDoodad(Ref& ref, Session::OpenTile& from, Session::OpenTile& to,
		    Transform const& transform, std::string& error)
		{
			size_t count = 0;
			SMDoodadDef* entries = DocDoodads(from, count);
			if (!entries || static_cast<size_t>(ref.index) >= count
			    || entries[ref.index].uniqueId != ref.uniqueId)
			{
				error = "that placement is no longer at index " + std::to_string(ref.index);
				return false;
			}

			// By value, the source list is about to lose the row it sits in.
			SMDoodadDef entry = entries[ref.index];

			char const* found = DocName(from, true, entry.nameId);
			if (!found)
			{
				error = "tile " + TileName(from.tileX, from.tileY) + " has no model path for that entry";
				return false;
			}

			// DocName points into the source blob, which ResolveName is about to grow.
			std::string model = found;

			uint32_t nameId = 0;
			if (!ResolveName(to, true, model.c_str(), nameId))
			{
				error = "could not add " + model + " to tile " + TileName(to.tileX, to.tileY);
				return false;
			}

			entry.nameId = nameId;
			entry.position = Coords::WorldToAdt(transform.position);
			entry.rotation = transform.rotation;
			entry.scale = PackScale(transform.scale);

			// Everything that could fail is behind us, so the old row can go before the new one
			// lands. Doing it in this order means a half done move never leaves two live copies.
			if (!Remove(ref, error))
				return false;

			Adt::TopChunk* top = EnsureTop(to.doc, Adt::kMDDF, Adt::kMWID);
			uint32_t index = AppendEntry(*top, &entry, sizeof(entry));

			ref.tileX = to.tileX;
			ref.tileY = to.tileY;
			ref.index = static_cast<int32_t>(index);

			CAaBox bounds = BoxAround(transform.position, 1.0f);
			if (CMapDoodadDef* def = Access::FindDoodadDef(ref.uniqueId))
			{
				C44Matrix mat = BuildDoodadMatrix(transform.position, transform.rotation, entry.scale * kScaleUnit);
				Access::CMapDoodadDef_UpdateMatrix(def, &mat);
				bounds = Merge(bounds, DoodadBounds(def));
			}

			RetargetRefs(to, true, index, bounds);
			to.dirty = true;
			return true;
		}

		bool MigrateMapObj(Ref& ref, Session::OpenTile& from, Session::OpenTile& to,
		    Transform const& transform, std::string& error)
		{
			size_t count = 0;
			SMMapObjDef* entries = DocMapObjs(from, count);
			if (!entries || static_cast<size_t>(ref.index) >= count
			    || entries[ref.index].uniqueId != ref.uniqueId)
			{
				error = "that placement is no longer at index " + std::to_string(ref.index);
				return false;
			}

			SMMapObjDef entry = entries[ref.index];

			char const* found = DocName(from, false, entry.nameId);
			if (!found)
			{
				error = "tile " + TileName(from.tileX, from.tileY) + " has no model path for that entry";
				return false;
			}

			std::string model = found;

			uint32_t nameId = 0;
			if (!ResolveName(to, false, model.c_str(), nameId))
			{
				error = "could not add " + model + " to tile " + TileName(to.tileX, to.tileY);
				return false;
			}

			C3Vector adt = Coords::WorldToAdt(transform.position);
			C3Vector shift = VectorMath::Subtract(adt, entry.position);

			entry.nameId = nameId;
			entry.position = adt;
			entry.rotation = transform.rotation;

			// Same prebaked box SetTransform has to drag along, nothing recomputes it. In ADT space
			// like the position beside it, so the shift has to be worked out there too.
			entry.extents.b = VectorMath::Add(entry.extents.b, shift);
			entry.extents.t = VectorMath::Add(entry.extents.t, shift);

			if (!Remove(ref, error))
				return false;

			Adt::TopChunk* top = EnsureTop(to.doc, Adt::kMODF, Adt::kMDDF);
			uint32_t index = AppendEntry(*top, &entry, sizeof(entry));

			ref.tileX = to.tileX;
			ref.tileY = to.tileY;
			ref.index = static_cast<int32_t>(index);

			CAaBox bounds = BoxAround(transform.position, 1.0f);
			if (CMapObjDef* def = Access::FindMapObjDef(entry.uniqueId))
			{
				C3Vector world = transform.position;
				Access::CMapObjDef_UpdatePos(def, &world, transform.rotation.y * kDegToRad + kPi,
				    transform.rotation.x * kDegToRad, transform.rotation.z * kDegToRad);
				bounds = Merge(bounds, MapObjBounds(def));
			}

			RetargetRefs(to, false, index, bounds);
			to.dirty = true;
			return true;
		}

		// Dragged past a border, so the entry has to change tiles: a different index, a different
		// nameId and MCRF work on both sides, with the uniqueId riding across untouched so the
		// client still treats it as the same instance.
		//
		// The client's parsed copy cannot follow, area->doodadDef and mapObjDef are fixed size
		// arrays allocated at load. The visible model is moved directly instead and the arrays
		// catch up when the tiles next load from disk.
		bool Migrate(Ref& ref, int32_t destX, int32_t destY, Transform const& transform, std::string& error)
		{
			if (!Access::IsValidTile(destX, destY))
			{
				error = "that lands off the edge of the map";
				return false;
			}

			Session::OpenTile* from = Session::Open(ref.tileX, ref.tileY, error);
			if (!from)
				return false;

			Session::OpenTile* to = Session::Open(destX, destY, error);
			if (!to)
			{
				error = "tile " + TileName(destX, destY) + " could not be opened: " + error;
				return false;
			}

			return ref.kind == Kind::Doodad ? MigrateDoodad(ref, *from, *to, transform, error)
			                                : MigrateMapObj(ref, *from, *to, transform, error);
		}
	}

	bool Pick(C3Vector const& start, C3Vector const& end, Ref& out)
	{
		if (!Access::IsActive())
			return false;

		C3Vector dir = VectorMath::Subtract(end, start);
		float rayLength = VectorMath::Length(dir);

		Ref best;
		float bestDistance = 0.0f;
		float bestSize = 0.0f;

		// Standing inside a building puts the ray origin inside its bounds, and every candidate in
		// there reports the same zero distance. Anything within half a yard of the current best is
		// settled on box size instead, which is what makes a lamp post inside a WMO selectable.
		auto beats = [&](float distance, float size)
		{
			if (!best.Valid())
				return true;

			if (distance > bestDistance + 0.5f)
				return false;

			if (distance > bestDistance - 0.5f)
				return size < bestSize;

			return true;
		};

		ForEachLiveDef(true,
		    [&](void* owner)
		    {
			    auto* def = static_cast<CMapDoodadDef*>(owner);
			    CAaBox box = DoodadBounds(def);

			    float t = 0.0f;
			    if (!RayHitsBox(start, dir, box, t))
				    return;

			    float distance = t * rayLength;
			    float size = BoxDiagonal(box);
			    if (!beats(distance, size))
				    return;

			    Ref ref;
			    if (!RefForDoodad(def, ref))
				    return;

			    bestDistance = distance;
			    bestSize = size;
			    best = ref;
		    });

		ForEachLiveDef(false,
		    [&](void* owner)
		    {
			    auto* def = static_cast<CMapObjDef*>(owner);
			    CAaBox box = MapObjBounds(def);

			    float t = 0.0f;
			    if (!RayHitsBox(start, dir, box, t))
				    return;

			    float distance = t * rayLength;
			    float size = BoxDiagonal(box);
			    if (!beats(distance, size))
				    return;

			    Ref ref;
			    if (!RefForMapObj(def, ref))
				    return;

			    bestDistance = distance;
			    bestSize = size;
			    best = ref;
		    });

		if (!best.Valid())
			return false;

		out = best;
		return true;
	}

	bool Resolve(Ref& ref)
	{
		if (!ref.Valid())
			return false;

		Session::OpenTile* tile = Session::Find(ref.tileX, ref.tileY);
		if (!tile)
		{
			std::string ignored;
			tile = Session::Open(ref.tileX, ref.tileY, ignored);
		}

		if (!tile)
			return false;

		size_t count = 0;
		if (ref.kind == Kind::Doodad)
		{
			SMDoodadDef const* entries = DocDoodads(*tile, count);
			if (static_cast<size_t>(ref.index) < count && entries[ref.index].uniqueId == ref.uniqueId)
				return true;

			for (size_t i = 0; i < count; ++i)
			{
				if (entries[i].uniqueId != ref.uniqueId)
					continue;

				ref.index = static_cast<int32_t>(i);
				return true;
			}

			return false;
		}

		SMMapObjDef const* entries = DocMapObjs(*tile, count);
		if (static_cast<size_t>(ref.index) < count && entries[ref.index].uniqueId == ref.uniqueId)
			return true;

		for (size_t i = 0; i < count; ++i)
		{
			if (entries[i].uniqueId != ref.uniqueId)
				continue;

			ref.index = static_cast<int32_t>(i);
			return true;
		}

		return false;
	}

	bool Describe(Ref const& ref, Info& out)
	{
		if (!ref.Valid())
			return false;

		Session::OpenTile* tile = Session::Find(ref.tileX, ref.tileY);
		if (!tile)
		{
			std::string ignored;
			tile = Session::Open(ref.tileX, ref.tileY, ignored);
		}

		if (!tile)
			return false;

		CMapArea* area = Access::GetArea(ref.tileX, ref.tileY);

		out = Info{};
		out.ref = ref;

		if (ref.kind == Kind::Doodad)
		{
			size_t count = 0;
			SMDoodadDef const* entries = DocDoodads(*tile, count);
			if (static_cast<size_t>(ref.index) >= count)
				return false;

			SMDoodadDef const& entry = entries[ref.index];
			out.transform.position = Coords::AdtToWorld(entry.position);
			out.transform.rotation = entry.rotation;
			out.transform.scale = entry.scale * kScaleUnit;

			// The document first. A migrated entry can carry a nameId the client's own parsed list
			// does not reach yet, and neither accessor bounds checks, so asking the client about
			// one would read off the end of MMID.
			if (char const* name = DocName(*tile, true, entry.nameId))
				out.name = name;
			else if (char const* name = Access::IsAreaReady(area) ? Access::DoodadFileName(area, entry.nameId) : nullptr)
				out.name = name;

			if (CMapDoodadDef* def = Access::FindDoodadDef(entry.uniqueId))
			{
				out.bounds = DoodadBounds(def);
				out.live = true;
			}

			return true;
		}

		size_t count = 0;
		SMMapObjDef const* entries = DocMapObjs(*tile, count);
		if (static_cast<size_t>(ref.index) >= count)
			return false;

		SMMapObjDef const& entry = entries[ref.index];
		out.transform.position = Coords::AdtToWorld(entry.position);
		out.transform.rotation = entry.rotation;
		out.transform.scale = 1.0f;

		if (char const* name = DocName(*tile, false, entry.nameId))
			out.name = name;
		else if (char const* name = Access::IsAreaReady(area) ? Access::MapObjFileName(area, entry.nameId) : nullptr)
			out.name = name;

		if (CMapObjDef* def = LiveMapObj(entry))
		{
			out.bounds = MapObjBounds(def);
			out.live = true;
		}

		return true;
	}

	bool Preview(Ref const& ref, Transform const& transform)
	{
		if (!ref.Valid())
			return false;

		if (ref.kind == Kind::Doodad)
		{
			CMapDoodadDef* def = Access::FindDoodadDef(ref.uniqueId);
			if (!def)
				return false;

			C44Matrix mat = BuildDoodadMatrix(transform.position, transform.rotation, transform.scale);
			Access::CMapDoodadDef_UpdateMatrix(def, &mat);
			return true;
		}

		Session::OpenTile* tile = Session::Find(ref.tileX, ref.tileY);
		if (!tile)
			return false;

		size_t count = 0;
		SMMapObjDef const* entries = DocMapObjs(*tile, count);
		if (static_cast<size_t>(ref.index) >= count)
			return false;

		CMapObjDef* def = LiveMapObj(entries[ref.index]);
		if (!def)
			return false;

		C3Vector world = transform.position;
		Access::CMapObjDef_UpdatePos(def, &world, transform.rotation.y * kDegToRad + kPi,
		    transform.rotation.x * kDegToRad, transform.rotation.z * kDegToRad);
		return true;
	}

	bool SetTransform(Ref& ref, Transform const& transform, std::string& error)
	{
		if (!ref.Valid())
		{
			error = "nothing selected";
			return false;
		}

		Session::OpenTile* tile = Session::Open(ref.tileX, ref.tileY, error);
		if (!tile)
			return false;

		int32_t destX = Coords::TileX(transform.position);
		int32_t destY = Coords::TileY(transform.position);
		if (destX != ref.tileX || destY != ref.tileY)
			return Migrate(ref, destX, destY, transform, error);

		CMapArea* area = Access::GetArea(ref.tileX, ref.tileY);
		C3Vector adt = Coords::WorldToAdt(transform.position);

		if (ref.kind == Kind::Doodad)
		{
			size_t count = 0;
			SMDoodadDef* entries = DocDoodads(*tile, count);
			if (static_cast<size_t>(ref.index) >= count || entries[ref.index].uniqueId != ref.uniqueId)
			{
				error = "that placement is no longer at index " + std::to_string(ref.index);
				return false;
			}

			uint16_t scale = PackScale(transform.scale);

			entries[ref.index].position = adt;
			entries[ref.index].rotation = transform.rotation;
			entries[ref.index].scale = scale;

			// The client parsed its own copy of the same entry out of filePtr. Keeping that in step
			// is what makes a reload before saving show the last edit rather than the one before it.
			if (Access::IsAreaReady(area) && ref.index < area->doodadDefCount)
			{
				area->doodadDef[ref.index].position = adt;
				area->doodadDef[ref.index].rotation = transform.rotation;
				area->doodadDef[ref.index].scale = scale;
			}

			CAaBox bounds = BoxAround(transform.position, 1.0f);
			if (CMapDoodadDef* def = Access::FindDoodadDef(ref.uniqueId))
			{
				C44Matrix mat = BuildDoodadMatrix(transform.position, transform.rotation, scale * kScaleUnit);
				Access::CMapDoodadDef_UpdateMatrix(def, &mat);
				bounds = Merge(bounds, DoodadBounds(def));
			}

			RetargetRefs(*tile, true, static_cast<uint32_t>(ref.index), bounds);
			tile->dirty = true;
			return true;
		}

		size_t count = 0;
		SMMapObjDef* entries = DocMapObjs(*tile, count);
		if (static_cast<size_t>(ref.index) >= count || entries[ref.index].uniqueId != ref.uniqueId)
		{
			error = "that placement is no longer at index " + std::to_string(ref.index);
			return false;
		}

		CMapObjDef* def = LiveMapObj(entries[ref.index]);

		C3Vector shift = VectorMath::Subtract(adt, entries[ref.index].position);
		entries[ref.index].position = adt;
		entries[ref.index].rotation = transform.rotation;

		// MODF carries a prebaked bounding box, in ADT space like the position above it.
		// CMap::CreateMapObjDef (0x007BF460) copies it into the live bbox at +0x48 and derives the
		// culling sphere from it, and nothing ever recomputes it off the model, so it has to be
		// dragged along by hand or culling keeps using the old footprint.
		entries[ref.index].extents.b = VectorMath::Add(entries[ref.index].extents.b, shift);
		entries[ref.index].extents.t = VectorMath::Add(entries[ref.index].extents.t, shift);

		if (Access::IsAreaReady(area) && ref.index < area->mapObjDefCount)
			area->mapObjDef[ref.index] = entries[ref.index];

		CAaBox bounds = BoxAround(transform.position, 1.0f);
		if (def)
		{
			C3Vector world = transform.position;
			Access::CMapObjDef_UpdatePos(def, &world, transform.rotation.y * kDegToRad + kPi,
			    transform.rotation.x * kDegToRad, transform.rotation.z * kDegToRad);
			bounds = Merge(bounds, Normalized(def->bbox));
		}

		RetargetRefs(*tile, false, static_cast<uint32_t>(ref.index), bounds);
		tile->dirty = true;
		return true;
	}

	bool Remove(Ref const& ref, std::string& error)
	{
		if (!ref.Valid())
		{
			error = "nothing selected";
			return false;
		}

		Session::OpenTile* tile = Session::Open(ref.tileX, ref.tileY, error);
		if (!tile)
			return false;

		bool doodad = ref.kind == Kind::Doodad;
		Adt::TopChunk* top = FindTop(tile->doc, doodad ? Adt::kMDDF : Adt::kMODF);
		if (!top)
		{
			error = "tile has no placement list";
			return false;
		}

		size_t stride = doodad ? sizeof(SMDoodadDef) : sizeof(SMMapObjDef);
		size_t count = top->data.size() / stride;
		if (static_cast<size_t>(ref.index) >= count)
		{
			error = "index " + std::to_string(ref.index) + " is past the end of the list";
			return false;
		}

		// Bury the live instance before the entry goes, since finding it again needs what the entry
		// holds. Nothing else can hide one: the client has no unlink that survives to the next
		// frame, and purging the def outright takes the whole tile with it.
		if (doodad)
		{
			auto const& entry = reinterpret_cast<SMDoodadDef const*>(top->data.data())[ref.index];
			if (entry.uniqueId != ref.uniqueId)
			{
				error = "that placement is no longer at index " + std::to_string(ref.index);
				return false;
			}

			if (CMapDoodadDef* def = Access::FindDoodadDef(entry.uniqueId))
			{
				C3Vector buried = Coords::AdtToWorld(entry.position);
				buried.z = kBuriedZ;

				C44Matrix mat = BuildDoodadMatrix(buried, entry.rotation, entry.scale * kScaleUnit);
				Access::CMapDoodadDef_UpdateMatrix(def, &mat);
			}
		}
		else
		{
			auto const& entry = reinterpret_cast<SMMapObjDef const*>(top->data.data())[ref.index];
			if (entry.uniqueId != ref.uniqueId)
			{
				error = "that placement is no longer at index " + std::to_string(ref.index);
				return false;
			}

			if (CMapObjDef* def = LiveMapObj(entry))
			{
				C3Vector buried = Coords::AdtToWorld(entry.position);
				buried.z = kBuriedZ;

				Access::CMapObjDef_UpdatePos(def, &buried, entry.rotation.y * kDegToRad + kPi,
				    entry.rotation.x * kDegToRad, entry.rotation.z * kDegToRad);
			}
		}

		DropRefs(*tile, doodad, static_cast<uint32_t>(ref.index));

		auto begin = top->data.begin() + static_cast<ptrdiff_t>(ref.index * stride);
		top->data.erase(begin, begin + static_cast<ptrdiff_t>(stride));

		tile->dirty = true;
		return true;
	}

	int32_t ListNear(C3Vector const& center, float radius, std::vector<Info>& out)
	{
		out.clear();
		if (!Access::IsActive())
			return 0;

		float limit = radius * radius;

		auto collect = [&](Ref const& ref, float distanceSq)
		{
			Info info;
			if (!Describe(ref, info))
				return;

			info.distance = std::sqrt(distanceSq);
			info.originDistance = std::sqrt(DistanceSqXY(info.transform.position, center));
			out.push_back(info);
		};

		// Measured to the bounds, not to the origin. A WMO's MODF position is the model's own
		// origin, which on a building the size of a keep can sit hundreds of yards from the wall
		// you are standing at, so an origin test drops exactly the big WMOs you went looking for.
		ForEachLiveDef(true,
		    [&](void* owner)
		    {
			    auto* def = static_cast<CMapDoodadDef*>(owner);
			    float distanceSq = BoxDistanceSqXY(DoodadBounds(def), center);
			    if (distanceSq > limit)
				    return;

			    Ref ref;
			    if (RefForDoodad(def, ref))
				    collect(ref, distanceSq);
		    });

		ForEachLiveDef(false,
		    [&](void* owner)
		    {
			    auto* def = static_cast<CMapObjDef*>(owner);
			    float distanceSq = BoxDistanceSqXY(MapObjBounds(def), center);
			    if (distanceSq > limit)
				    return;

			    Ref ref;
			    if (RefForMapObj(def, ref))
				    collect(ref, distanceSq);
		    });

		// Anything whose footprint you are standing in measures zero, so a city block of WMOs all
		// tie at the top with nothing to order them. The origin breaks that tie, which puts the
		// hut you are inside above the keep whose corner you happen to be clipping.
		std::stable_sort(out.begin(), out.end(),
		    [](Info const& a, Info const& b)
		    {
			    if (a.distance != b.distance)
				    return a.distance < b.distance;
			    return a.originDistance < b.originDistance;
		    });

		return static_cast<int32_t>(out.size());
	}

	bool KindForPath(char const* path, Kind& out)
	{
		std::string text = GamePath(path);
		size_t dot = text.find_last_of('.');
		if (dot == std::string::npos)
			return false;

		std::string ext = text.substr(dot);
		for (char& c : ext)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

		if (ext == ".wmo")
		{
			out = Kind::MapObj;
			return true;
		}

		// .mdx and .mdl both end up opening a .m2, so all three name a doodad.
		if (ext == ".m2" || ext == ".mdx" || ext == ".mdl")
		{
			out = Kind::Doodad;
			return true;
		}

		return false;
	}

	bool Add(Kind kind, char const* path, Transform const& transform, Ref& out, std::string& error)
	{
		std::string model = GamePath(path);
		if (model.empty())
		{
			error = "no model path given";
			return false;
		}

		if (LooksMangled(model))
		{
			error = "that path has a control character in it, so a Lua escape ate a backslash. "
			        "Use forward slashes.";
			return false;
		}

		if (!ModelExists(model))
		{
			error = "the game files have no " + model;
			return false;
		}

		int32_t tileX = Coords::TileX(transform.position);
		int32_t tileY = Coords::TileY(transform.position);
		if (!Access::IsValidTile(tileX, tileY))
		{
			error = "that spot is off the edge of the map";
			return false;
		}

		Session::OpenTile* tile = Session::Open(tileX, tileY, error);
		if (!tile)
			return false;

		if (kind == Kind::Doodad)
			return InsertDoodad(*tile, model, transform, 0, out, error);

		// A cube big enough to hold the model whichever way it is turned, since the extents are
		// what culling runs on and nothing will ever correct a guess that was too small.
		C3Vector adt = Coords::WorldToAdt(transform.position);
		float radius = MapObjRadius(model);
		CAaBox extents = {{adt.x - radius, adt.y - radius, adt.z - radius},
		    {adt.x + radius, adt.y + radius, adt.z + radius}};

		return InsertMapObj(*tile, model, transform, extents, 0, 0, 0, out, error);
	}

	bool Clone(Ref const& ref, Transform const& transform, Ref& out, std::string& error)
	{
		if (!ref.Valid())
		{
			error = "nothing selected";
			return false;
		}

		Session::OpenTile* source = Session::Open(ref.tileX, ref.tileY, error);
		if (!source)
			return false;

		int32_t tileX = Coords::TileX(transform.position);
		int32_t tileY = Coords::TileY(transform.position);
		if (!Access::IsValidTile(tileX, tileY))
		{
			error = "that spot is off the edge of the map";
			return false;
		}

		bool doodad = ref.kind == Kind::Doodad;

		// The row by value and the path by copy, both before anything opens a second tile or grows
		// a name list, either of which moves the bytes these point at.
		SMDoodadDef doodadEntry{};
		SMMapObjDef mapObjEntry{};
		uint32_t nameId = 0;

		if (doodad)
		{
			size_t count = 0;
			SMDoodadDef* entries = DocDoodads(*source, count);
			if (!entries || static_cast<size_t>(ref.index) >= count
			    || entries[ref.index].uniqueId != ref.uniqueId)
			{
				error = "that placement is no longer at index " + std::to_string(ref.index);
				return false;
			}

			doodadEntry = entries[ref.index];
			nameId = doodadEntry.nameId;
		}
		else
		{
			size_t count = 0;
			SMMapObjDef* entries = DocMapObjs(*source, count);
			if (!entries || static_cast<size_t>(ref.index) >= count
			    || entries[ref.index].uniqueId != ref.uniqueId)
			{
				error = "that placement is no longer at index " + std::to_string(ref.index);
				return false;
			}

			mapObjEntry = entries[ref.index];
			nameId = mapObjEntry.nameId;
		}

		char const* found = DocName(*source, doodad, nameId);
		if (!found)
		{
			error = "tile " + TileName(ref.tileX, ref.tileY) + " has no model path for that entry";
			return false;
		}

		std::string model = found;

		Session::OpenTile* tile = Session::Open(tileX, tileY, error);
		if (!tile)
			return false;

		if (doodad)
			return InsertDoodad(*tile, model, transform, doodadEntry.flags, out, error);

		// The original's own box, moved to the new spot. Better than a guessed cube, since the
		// source's came out of whatever built the tile in the first place.
		C3Vector adt = Coords::WorldToAdt(transform.position);
		C3Vector shift = VectorMath::Subtract(adt, mapObjEntry.position);
		CAaBox extents = {VectorMath::Add(mapObjEntry.extents.b, shift),
		    VectorMath::Add(mapObjEntry.extents.t, shift)};

		return InsertMapObj(*tile, model, transform, extents, mapObjEntry.flags, mapObjEntry.doodadSet,
		    mapObjEntry.nameSet, out, error);
	}

	int32_t ListModels(C3Vector const& center, float radius, std::vector<std::string>& out)
	{
		out.clear();

		std::vector<Info> nearby;
		ListNear(center, radius, nearby);

		// ListNear walks the chunk lists in whatever order they happen to be in, so sort first.
		// Distance to the bounds rather than the origin, same reason ListNear measures that way.
		std::sort(nearby.begin(), nearby.end(),
		    [&](Info const& a, Info const& b)
		    { return BoxDistanceSqXY(a.bounds, center) < BoxDistanceSqXY(b.bounds, center); });

		// Keeping the first sighting of each path leaves the list nearest first as well.
		for (Info const& info : nearby)
		{
			if (info.name.empty())
				continue;

			bool seen = false;
			for (std::string const& have : out)
			{
				if (SameName(have.c_str(), info.name.c_str()))
				{
					seen = true;
					break;
				}
			}

			if (!seen)
				out.push_back(info.name);
		}

		return static_cast<int32_t>(out.size());
	}

	void DebugRay(C3Vector const& start, C3Vector const& end, std::vector<std::string>& lines)
	{
		lines.clear();

		char line[256];
		if (!Access::IsActive())
		{
			lines.push_back("placements: CMap is not active");
			return;
		}

		C3Vector dir = VectorMath::Subtract(end, start);
		float rayLength = VectorMath::Length(dir);

		struct Candidate
		{
			bool doodad = false;
			float distance = 0.0f;
			float size = 0.0f;
			bool resolved = false;
			Ref ref;
		};

		std::vector<Candidate> hits;

		auto sweep = [&](bool doodads, int32_t& live, int32_t& resolved)
		{
			live = 0;
			resolved = 0;

			ForEachLiveDef(doodads,
			    [&](void* owner)
			    {
				    ++live;

				    CAaBox box = doodads ? DoodadBounds(static_cast<CMapDoodadDef*>(owner))
				                         : MapObjBounds(static_cast<CMapObjDef*>(owner));

				    Ref ref;
				    bool ok = doodads ? RefForDoodad(static_cast<CMapDoodadDef*>(owner), ref)
				                      : RefForMapObj(static_cast<CMapObjDef*>(owner), ref);
				    if (ok)
					    ++resolved;

				    float t = 0.0f;
				    if (!RayHitsBox(start, dir, box, t))
					    return;

				    Candidate candidate;
				    candidate.doodad = doodads;
				    candidate.distance = t * rayLength;
				    candidate.size = BoxDiagonal(box);
				    candidate.resolved = ok;
				    candidate.ref = ref;
				    hits.push_back(candidate);
			    });
		};

		int32_t liveDoodads = 0;
		int32_t okDoodads = 0;
		int32_t liveMapObjs = 0;
		int32_t okMapObjs = 0;
		sweep(true, liveDoodads, okDoodads);
		sweep(false, liveMapObjs, okMapObjs);

		std::snprintf(line, sizeof(line), "placements: %d live doodads (%d matched), %d live WMOs (%d matched)",
		    liveDoodads, okDoodads, liveMapObjs, okMapObjs);
		lines.push_back(line);

		std::snprintf(line, sizeof(line), "  ray crosses %d of them", static_cast<int32_t>(hits.size()));
		lines.push_back(line);

		std::sort(hits.begin(), hits.end(),
		    [](Candidate const& a, Candidate const& b) { return a.distance < b.distance; });

		size_t show = hits.size() < 8 ? hits.size() : 8;
		for (size_t i = 0; i < show; ++i)
		{
			Candidate const& hit = hits[i];

			Info info;
			std::string name = hit.resolved && Describe(hit.ref, info) ? info.name : std::string("?");

			std::snprintf(line, sizeof(line), "  %s dist %.1f  box %.1f  %s  %s",
			    hit.doodad ? "m2 " : "wmo", hit.distance, hit.size,
			    hit.resolved ? "matched" : "NO FILE ENTRY", name.c_str());
			lines.push_back(line);
		}
	}
}
