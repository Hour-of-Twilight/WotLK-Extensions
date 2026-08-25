#include <Editor/Map/Sculpt.h>

#include <Editor/Map/Adt/AdtDocument.h>
#include <Editor/Map/MapClient.h>
#include <Editor/Map/MapCoords.h>
#include <Editor/Map/TileSession.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace MapEditor::Sculpt
{
	namespace
	{
		constexpr int32_t kVerts = Access::kVertsPerChunk;
		constexpr int32_t kSide = Access::kChunksPerTileSide;

		float Weight(float distance, float radius, Falloff falloff)
		{
			if (radius <= 0.0f || distance >= radius)
				return 0.0f;

			float t = 1.0f - distance / radius;
			switch (falloff)
			{
				case Falloff::Flat: return 1.0f;
				case Falloff::Linear: return t;
				case Falloff::Smooth:
				default: return t * t * (3.0f - 2.0f * t);
			}
		}

		// Which chunk a piece of work belongs to. Kept as coordinates rather than a pointer
		// because the normals pass has to name chunks the stroke never touched.
		struct ChunkRef
		{
			int32_t tileX = 0;
			int32_t tileY = 0;
			int32_t chunkX = 0;
			int32_t chunkY = 0;

			bool operator==(ChunkRef const& other) const
			{
				return tileX == other.tileX && tileY == other.tileY && chunkX == other.chunkX
				    && chunkY == other.chunkY;
			}
		};

		CMapChunk* Resolve(ChunkRef const& ref)
		{
			CMapArea* area = Access::GetArea(ref.tileX, ref.tileY);
			if (!Access::IsAreaReady(area))
				return nullptr;

			return Access::GetChunk(area, ref.chunkX, ref.chunkY);
		}

		// Steps a chunk index by one, rolling over into the neighbouring tile. Chunk and tile
		// indices count the same way on each axis, so stepping off one end lands on index 0 of
		// the next tile.
		ChunkRef Step(ChunkRef ref, int32_t dChunkX, int32_t dChunkY)
		{
			ref.chunkX += dChunkX;
			ref.chunkY += dChunkY;

			if (ref.chunkX < 0)
			{
				ref.chunkX += kSide;
				--ref.tileX;
			}
			else if (ref.chunkX >= kSide)
			{
				ref.chunkX -= kSide;
				++ref.tileX;
			}

			if (ref.chunkY < 0)
			{
				ref.chunkY += kSide;
				--ref.tileY;
			}
			else if (ref.chunkY >= kSide)
			{
				ref.chunkY -= kSide;
				++ref.tileY;
			}

			return ref;
		}

		// The 3x3 block of chunks centred on one chunk, indexed [dChunkY + 1][dChunkX + 1]. A
		// border vertex belongs to up to four chunks that each keep their own copy, so building its
		// normal from one chunk's vertices alone lights the shared edge differently from either
		// face, which is the lattice of bright lines along the chunk grid.
		struct Neighborhood
		{
			CMapChunk* chunk[3][3] = {};
		};

		Neighborhood Gather(ChunkRef const& center)
		{
			Neighborhood out;

			for (int32_t dy = -1; dy <= 1; ++dy)
			{
				for (int32_t dx = -1; dx <= 1; ++dx)
					out.chunk[dy + 1][dx + 1] = Resolve(Step(center, dx, dy));
			}

			return out;
		}

		// Vertex rows run along the same axis as chunkY and columns along chunkX, so a row or
		// column outside 0..8 resolves into the neighbour on that axis.
		bool OuterAt(Neighborhood const& hood, int32_t row, int32_t col, C3Vector& out)
		{
			int32_t dy = 0;
			int32_t dx = 0;

			if (row < 0)
			{
				dy = -1;
				row += 8;
			}
			else if (row > 8)
			{
				dy = 1;
				row -= 8;
			}

			if (col < 0)
			{
				dx = -1;
				col += 8;
			}
			else if (col > 8)
			{
				dx = 1;
				col -= 8;
			}

			CMapChunk* chunk = hood.chunk[dy + 1][dx + 1];
			if (!chunk || !chunk->vertices)
				return false;

			out = Coords::VertexWorldPos(chunk, Coords::EncodeOuterVertex(row, col));
			return true;
		}

		bool InnerAt(Neighborhood const& hood, int32_t row, int32_t col, C3Vector& out)
		{
			int32_t dy = 0;
			int32_t dx = 0;

			if (row < 0)
			{
				dy = -1;
				row += 8;
			}
			else if (row > 7)
			{
				dy = 1;
				row -= 8;
			}

			if (col < 0)
			{
				dx = -1;
				col += 8;
			}
			else if (col > 7)
			{
				dx = 1;
				col -= 8;
			}

			CMapChunk* chunk = hood.chunk[dy + 1][dx + 1];
			if (!chunk || !chunk->vertices)
				return false;

			out = Coords::VertexWorldPos(chunk, Coords::EncodeInnerVertex(row, col));
			return true;
		}

		// Index of a vertex within the centre chunk, or -1 when it belongs to a neighbour.
		int32_t OuterIndex(int32_t row, int32_t col)
		{
			if (row < 0 || row > 8 || col < 0 || col > 8)
				return -1;

			return Coords::EncodeOuterVertex(row, col);
		}

		int32_t InnerIndex(int32_t row, int32_t col)
		{
			if (row < 0 || row > 7 || col < 0 || col > 7)
				return -1;

			return Coords::EncodeInnerVertex(row, col);
		}

		// Rebuilds MCNR for the centre chunk of a neighbourhood. Each cell is four triangles
		// fanning out from its inner vertex, so a vertex normal is the normalized sum of the face
		// normals of every triangle touching it, and a face pointing down just means that
		// triangle's winding came out backwards.
		void RecomputeNormals(Neighborhood const& hood, int8_t* out)
		{
			C3Vector sum[kVerts] = {};

			auto addFace = [&](C3Vector const& a, C3Vector const& b, C3Vector const& c, int32_t ia,
			                   int32_t ib, int32_t ic)
			{
				C3Vector u{b.x - a.x, b.y - a.y, b.z - a.z};
				C3Vector v{c.x - a.x, c.y - a.y, c.z - a.z};
				C3Vector face{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};

				if (face.z < 0.0f)
				{
					face.x = -face.x;
					face.y = -face.y;
					face.z = -face.z;
				}

				for (int32_t index : {ia, ib, ic})
				{
					if (index < 0)
						continue;

					sum[index].x += face.x;
					sum[index].y += face.y;
					sum[index].z += face.z;
				}
			};

			// Cells from -1 to 8 on each axis, so every vertex of the centre chunk, border rows
			// included, sees all of the cells that actually surround it. The ones outside 0..7
			// contribute to this chunk's border vertices without owning any of their own.
			for (int32_t row = -1; row <= 8; ++row)
			{
				for (int32_t col = -1; col <= 8; ++col)
				{
					C3Vector inner{};
					C3Vector tl{};
					C3Vector tr{};
					C3Vector br{};
					C3Vector bl{};

					// Off the edge of what is loaded. The centre chunk's own cells always resolve,
					// so this only ever drops a neighbour's contribution, which is the old
					// per-chunk behaviour and the best available at the edge of the loaded set.
					if (!InnerAt(hood, row, col, inner) || !OuterAt(hood, row, col, tl)
					    || !OuterAt(hood, row, col + 1, tr) || !OuterAt(hood, row + 1, col + 1, br)
					    || !OuterAt(hood, row + 1, col, bl))
						continue;

					int32_t ii = InnerIndex(row, col);
					int32_t itl = OuterIndex(row, col);
					int32_t itr = OuterIndex(row, col + 1);
					int32_t ibr = OuterIndex(row + 1, col + 1);
					int32_t ibl = OuterIndex(row + 1, col);

					addFace(inner, tl, tr, ii, itl, itr);
					addFace(inner, tr, br, ii, itr, ibr);
					addFace(inner, br, bl, ii, ibr, ibl);
					addFace(inner, bl, tl, ii, ibl, itl);
				}
			}

			// CMapChunk::CreateVerticesWorld (0x007C3F30) reads the three bytes at [normals + i*3]
			// straight into the vertex normal's x, y and z, each scaled by flt_A40360 = 1/127, on
			// the same axes it just wrote the position on. So this is plain signed 8 bit, no axis
			// swap and no reordering.
			for (int32_t i = 0; i < kVerts; ++i)
			{
				C3Vector n = sum[i];
				float length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
				if (length <= 0.0f)
				{
					n = C3Vector{0.0f, 0.0f, 1.0f};
					length = 1.0f;
				}

				auto encode = [](float value)
				{
					float scaled = std::floor(value * 127.0f + 0.5f);
					return static_cast<int8_t>(std::min(std::max(scaled, -127.0f), 127.0f));
				};

				out[i * 3 + 0] = encode(n.x / length);
				out[i * 3 + 1] = encode(n.y / length);
				out[i * 3 + 2] = encode(n.z / length);
			}
		}

		// World height averaged over the vertices immediately around one vertex. Outer and inner
		// vertices interleave, so each one's nearest neighbours are all of the other kind.
		bool NeighborAverage(Neighborhood const& hood, int32_t index, float& out)
		{
			Coords::VertexCoord coord = Coords::DecodeVertex(index);

			int32_t rows[4];
			int32_t cols[4];

			if (coord.inner)
			{
				// An inner vertex sits at the centre of its cell, so its neighbours are that
				// cell's four corners.
				rows[0] = coord.row;
				cols[0] = coord.col;
				rows[1] = coord.row;
				cols[1] = coord.col + 1;
				rows[2] = coord.row + 1;
				cols[2] = coord.col;
				rows[3] = coord.row + 1;
				cols[3] = coord.col + 1;
			}
			else
			{
				// An outer vertex is a corner of up to four cells, so its neighbours are those
				// cells' inner vertices.
				rows[0] = coord.row - 1;
				cols[0] = coord.col - 1;
				rows[1] = coord.row - 1;
				cols[1] = coord.col;
				rows[2] = coord.row;
				cols[2] = coord.col - 1;
				rows[3] = coord.row;
				cols[3] = coord.col;
			}

			float total = 0.0f;
			int32_t count = 0;

			for (int32_t i = 0; i < 4; ++i)
			{
				C3Vector at{};
				bool ok = coord.inner ? OuterAt(hood, rows[i], cols[i], at)
				                      : InnerAt(hood, rows[i], cols[i], at);
				if (!ok)
					continue;

				total += at.z;
				++count;
			}

			if (!count)
				return false;

			out = total / static_cast<float>(count);
			return true;
		}

		// Keyed on the vertex's place in the map-wide lattice, doubled so inner vertices get their
		// own keys. Two chunks sharing a border vertex therefore jitter it identically and the
		// seam stays closed, which a per-call random number generator could not manage.
		float Jitter(ChunkRef const& ref, int32_t index)
		{
			Coords::VertexCoord coord = Coords::DecodeVertex(index);

			int32_t row = ((ref.tileY * kSide + ref.chunkY) * 8 + coord.row) * 2 + (coord.inner ? 1 : 0);
			int32_t col = ((ref.tileX * kSide + ref.chunkX) * 8 + coord.col) * 2 + (coord.inner ? 1 : 0);

			uint32_t h = static_cast<uint32_t>(row) * 73856093u ^ static_cast<uint32_t>(col) * 19349663u;
			h ^= h >> 13;
			h *= 0x5bd1e995u;
			h ^= h >> 15;

			return static_cast<float>(h & 0xFFFFFF) / static_cast<float>(0xFFFFFF) * 2.0f - 1.0f;
		}

		// One chunk the brush reaches, with the heights it should end up with. Heights are worked
		// out for every chunk before any are written, because Smooth reads its neighbours: editing
		// in place would make the result depend on which chunk happened to be visited first, and
		// the two copies of a shared vertex would stop agreeing.
		struct Target
		{
			ChunkRef ref;
			CMapChunk* chunk = nullptr;
			float heights[kVerts] = {};
			bool moved = false;
		};

		void MirrorHeights(Session::OpenTile& tile, ChunkRef const& ref, float const* heights)
		{
			Adt::Mcnk* mcnk = tile.doc.ChunkAt(ref.chunkX, ref.chunkY);
			if (!mcnk)
				return;

			Adt::SubChunk* mcvt = mcnk->Find(Adt::kMCVT);
			if (!mcvt || mcvt->data.size() < kVerts * sizeof(float))
				return;

			std::memcpy(mcvt->data.data(), heights, kVerts * sizeof(float));
			tile.dirty = true;
		}

		void MirrorNormals(Session::OpenTile& tile, ChunkRef const& ref, int8_t const* normals)
		{
			Adt::Mcnk* mcnk = tile.doc.ChunkAt(ref.chunkX, ref.chunkY);
			if (!mcnk)
				return;

			Adt::SubChunk* mcnr = mcnk->Find(Adt::kMCNR);
			if (!mcnr || mcnr->data.size() < kVerts * 3)
				return;

			std::memcpy(mcnr->data.data(), normals, kVerts * 3);
			tile.dirty = true;
		}

		// Recomputes one chunk's normals and pushes them to the client and the document, but only
		// when they actually changed. The check matters because the normals pass deliberately
		// covers chunks the brush never reached, and without it brushing anywhere near a tile
		// border would mark the neighbouring tile dirty every single frame.
		void UpdateNormals(ChunkRef const& ref)
		{
			CMapChunk* chunk = Resolve(ref);
			if (!chunk || !chunk->vertices || !chunk->normals)
				return;

			int8_t normals[kVerts * 3];
			RecomputeNormals(Gather(ref), normals);

			if (std::memcmp(chunk->normals, normals, sizeof(normals)) == 0)
				return;

			std::memcpy(chunk->normals, normals, sizeof(normals));

			std::string error;
			if (Session::OpenTile* tile = Session::Open(ref.tileX, ref.tileY, error))
				MirrorNormals(*tile, ref, normals);

			// Drops the GPU copy so RenderPrepBufs rebuilds it from the new MCVT and MCNR next
			// frame.
			if (chunk->renderChunk)
				Access::CMapRenderChunk_FreeBuf(chunk->renderChunk);
		}
	}

	int32_t Apply(Stroke const& stroke)
	{
		if (stroke.radius <= 0.0f)
			return 0;

		bool converging = stroke.mode == Mode::Flatten || stroke.mode == Mode::Smooth;
		if (converging ? stroke.blend <= 0.0f : stroke.amount == 0.0f)
			return 0;

		C3Vector lo{stroke.center.x - stroke.radius, stroke.center.y - stroke.radius, 0.0f};
		C3Vector hi{stroke.center.x + stroke.radius, stroke.center.y + stroke.radius, 0.0f};

		int32_t tileX0 = std::min(Coords::TileX(lo), Coords::TileX(hi));
		int32_t tileX1 = std::max(Coords::TileX(lo), Coords::TileX(hi));
		int32_t tileY0 = std::min(Coords::TileY(lo), Coords::TileY(hi));
		int32_t tileY1 = std::max(Coords::TileY(lo), Coords::TileY(hi));

		std::vector<Target> targets;

		for (int32_t tileY = tileY0; tileY <= tileY1; ++tileY)
		{
			for (int32_t tileX = tileX0; tileX <= tileX1; ++tileX)
			{
				if (!Access::IsAreaReady(Access::GetArea(tileX, tileY)))
					continue;

				for (int32_t chunkY = 0; chunkY < kSide; ++chunkY)
				{
					for (int32_t chunkX = 0; chunkX < kSide; ++chunkX)
					{
						ChunkRef ref{tileX, tileY, chunkX, chunkY};
						CMapChunk* chunk = Resolve(ref);
						if (!chunk || !chunk->vertices || !chunk->normals)
							continue;

						if (!Coords::CircleTouchesChunk(chunk, stroke.center, stroke.radius))
							continue;

						Target target;
						target.ref = ref;
						target.chunk = chunk;
						std::memcpy(target.heights, chunk->vertices, sizeof(target.heights));
						targets.push_back(target);
					}
				}
			}
		}

		int32_t moved = 0;

		for (Target& target : targets)
		{
			Neighborhood hood;
			if (stroke.mode == Mode::Smooth)
				hood = Gather(target.ref);

			for (int32_t i = 0; i < kVerts; ++i)
			{
				C3Vector world = Coords::VertexWorldPos(target.chunk, i);
				float weight = Weight(Coords::Distance2D(world, stroke.center), stroke.radius,
				    stroke.falloff);

				if (weight <= 0.0f)
					continue;

				// MCVT heights are relative to the chunk's own origin, so anything expressed in
				// world z has to come back down by it.
				float height = target.chunk->vertices[i];

				switch (stroke.mode)
				{
					case Mode::Raise:
						height += stroke.amount * weight;
						break;

					case Mode::Noise:
						height += stroke.amount * weight * Jitter(target.ref, i);
						break;

					case Mode::Flatten:
					{
						float goal = stroke.targetHeight - target.chunk->topLeftCoords.z;
						height += (goal - height) * weight * stroke.blend;
						break;
					}

					case Mode::Smooth:
					{
						float average = 0.0f;
						if (!NeighborAverage(hood, i, average))
							continue;

						float goal = average - target.chunk->topLeftCoords.z;
						height += (goal - height) * weight * stroke.blend;
						break;
					}
				}

				if (height == target.heights[i])
					continue;

				target.heights[i] = height;
				target.moved = true;
				++moved;
			}
		}

		if (!moved)
			return 0;

		std::vector<ChunkRef> needNormals;

		for (Target& target : targets)
		{
			if (!target.moved)
				continue;

			std::memcpy(target.chunk->vertices, target.heights, sizeof(target.heights));

			std::string error;
			if (Session::OpenTile* tile = Session::Open(target.ref.tileX, target.ref.tileY, error))
				MirrorHeights(*tile, target.ref, target.heights);

			// Without this the chunk keeps its old bbox, so culling and the collision broadphase
			// both go stale. Collision itself needs nothing, since CMap::GetTriSubchunk reads
			// chunk->vertices directly.
			Access::CMapChunk_CreateBounds(target.chunk);

			// A moved chunk changes the normals of its neighbours' border vertices too, so they
			// have to be rebuilt even though none of their own heights moved.
			for (int32_t dy = -1; dy <= 1; ++dy)
			{
				for (int32_t dx = -1; dx <= 1; ++dx)
				{
					ChunkRef ref = Step(target.ref, dx, dy);
					if (std::find(needNormals.begin(), needNormals.end(), ref) == needNormals.end())
						needNormals.push_back(ref);
				}
			}
		}

		for (ChunkRef const& ref : needNormals)
			UpdateNormals(ref);

		return moved;
	}
}
