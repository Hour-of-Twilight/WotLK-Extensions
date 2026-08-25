#include <Editor/Map/MapEditorRuntime.h>

#include <ClientData/Camera.h>
#include <ClientData/ClientFunctions.h>
#include <ClientData/Draw.h>
#include <ClientData/Event.h>
#include <ClientData/GameClient.h>
#include <ClientData/GxDevice.h>
#include <ClientData/ObjectManager.h>
#include <ClientData/VectorMath.h>
#include <CustomLua.h>
#include <Editor/GizmoDraw.h>
#include <Editor/GizmoPick.h>
#include <Editor/Map/Adt/AdtDocument.h>
#include <Editor/Map/Adt/AdtStore.h>
#include <Editor/Map/Adt/AlphaMap.h>
#include <Editor/Map/BrushDecal.h>
#include <Editor/Map/Liquids.h>
#include <Editor/Map/MapClient.h>
#include <Editor/Map/MapCoords.h>
#include <Editor/Map/Placements.h>
#include <Editor/Map/Sculpt.h>
#include <Editor/Map/TextureBrush.h>
#include <Editor/Map/TextureLayers.h>
#include <Editor/Map/TileSession.h>
#include <SharedDefines.h>

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace MapEditor::Runtime
{
	using namespace ClientData;

	namespace
	{
		constexpr float kRadToDeg = 57.2957795f;

		// The rings sit outside the arrows so both are reachable, the same ratio the gameobject
		// editor uses.
		constexpr float kRingScale = 1.3f;

		struct CursorInfo
		{
			bool valid = false;
			C3Vector position{};
			int32_t tileX = -1;
			int32_t tileY = -1;
			int32_t chunkX = -1;
			int32_t chunkY = -1;
			int32_t areaId = 0;
		};

		// What a drag with the left button does. Both tools share the brush footprint, the
		// falloff and the strength, so switching is one command rather than a separate mode.
		enum class Tool
		{
			Sculpt,
			Paint,
			Place,
			Liquid,
		};

		// A gizmo drag in progress. It holds the transform the drag started from and where the ray
		// first met the gizmo, so the model keeps its offset from the cursor instead of snapping to
		// it.
		struct GizmoDrag
		{
			bool active = false;
			bool rotating = false;
			GPick::Axis axis = GPick::Axis::None;
			C3Vector startPosition{};
			C3Vector anchorOnAxis{};

			C3Vector startRotation{};
			float anchorAngle = 0.0f;
			C3Vector ringAxis{};
			C3Vector ringRefDir{};
			C3Vector ringRefPerp{};
		};

		struct RuntimeState
		{
			bool enabled = false;

			// Placement work with the rest of the editor off. Sculpting has to own the mouse
			// because a stroke is a drag, but moving a doodad only needs the clicks that land on
			// one, so here a click that hits nothing goes through to the game.
			bool placeOnly = false;

			bool showGrid = false;
			float brushRadius = 10.0f;

			// Yards per second at the centre of the brush, so a stroke feels the same whatever the
			// frame rate.
			float brushStrength = 8.0f;
			Sculpt::Falloff falloff = Sculpt::Falloff::Smooth;
			Sculpt::Mode mode = Sculpt::Mode::Raise;

			Tool tool = Tool::Sculpt;

			// Which of the hovered chunk's layers the paint tool lays down. Layer 0 is the base,
			// so painting it means clearing everything above it back off.
			int32_t paintLayer = 1;

			// LiquidType.dbc row the liquid tools lay down. 1 is plain water.
			uint32_t liquidType = 1;

			// How far above the ground a new surface goes when nobody names a height. Laying it
			// exactly at the cursor buries it in the terrain it is meant to cover.
			float liquidOffset = 1.0f;

			// Tiles the liquid brush has edited since the drag started. It saves and reloads once
			// on mouse up rather than per frame, because each publish is a visible blink.
			std::vector<Liquids::TileRef> liquidPending;
			uint32_t lastLiquidMs = 0;

			// "lower" is raise pointed the other way rather than its own mode, so brush strength
			// stays a plain positive number and shift still inverts whichever way is current.
			bool sink = false;

			bool painting = false;
			uint32_t lastStrokeMs = 0;

			// Sampled when the stroke starts so Flatten levels to the ground it began on rather
			// than chasing whatever is under the cursor now.
			float strokeTarget = 0.0f;

			float mouseX = 0.0f;
			float mouseY = 0.0f;

			C3Vector rayStart{};
			C3Vector rayEnd{};
			float rayFraction = 0.0f;
			CursorInfo cursor{};

			// The placement tool's current subject. Transform is the working copy the gizmo drags,
			// which only gets written back to the file once the drag ends.
			Placements::Ref selection{};
			Placements::Transform selected{};
			std::string selectedName;
			GPick::Axis hoverAxis = GPick::Axis::None;
			GPick::Axis hoverRing = GPick::Axis::None;
			GizmoDrag drag{};

			// A just-added entry, waiting for the reload to build its model so its MCRF footprint
			// can be redone off the real bounds instead of the guess it went in with.
			Placements::Ref pending{};
			Placements::Transform pendingTransform{};
			uint32_t pendingUntilMs = 0;

			// Why the last UpdateCursor gave up, for MapEditor_Debug.
			const char* lastMiss = "not run yet";
		};

		RuntimeState& State()
		{
			static RuntimeState state;
			return state;
		}

		bool& EventsRegistered()
		{
			static bool registered = false;
			return registered;
		}

		void Print(const char* format, ...)
		{
			char message[512];
			va_list args;
			va_start(args, format);
			std::vsnprintf(message, sizeof(message), format, args);
			va_end(args);

			CGChat::AddChatMessage(message, 0, 0, 0, nullptr, 0, nullptr, 0, 0, 0, 0, 0, nullptr);
		}

		// Placement mode with the editor itself off. The two flags are independent so turning the
		// editor on does not silently change what a click does.
		bool PlaceOnly()
		{
			RuntimeState const& state = State();
			return state.placeOnly && !state.enabled;
		}

		// Whether the placement tool should be running its ray, gizmo and picking this frame.
		bool PlacementLive()
		{
			RuntimeState const& state = State();
			return PlaceOnly() || (state.enabled && state.tool == Tool::Place);
		}

		// Anything at all to do this frame.
		bool AnyModeLive()
		{
			return State().enabled || PlaceOnly();
		}

		// Fills in the tile, chunk and area for a spot on the ground.
		CursorInfo At(C3Vector const& position)
		{
			CursorInfo info;
			info.valid = true;
			info.position = position;
			info.tileX = Coords::TileX(position);
			info.tileY = Coords::TileY(position);
			info.chunkX = Coords::ChunkX(position);
			info.chunkY = Coords::ChunkY(position);

			CMapArea* area = Access::GetArea(info.tileX, info.tileY);
			if (CMapChunk* chunk = Access::GetChunk(area, info.chunkX, info.chunkY))
				info.areaId = chunk->areaId;

			return info;
		}

		// Rebuilds the picking ray and re-runs the terrain query. Only safe after the client's own
		// OnWorldRender has finished, since MouseToWorld reprograms the device projection and view
		// before restoring them.
		void UpdateCursor(CGWorldFrameFull* worldFrame)
		{
			RuntimeState& state = State();
			state.cursor = {};

			if (!worldFrame || !worldFrame->m_camera)
			{
				state.lastMiss = "no world frame or camera";
				return;
			}

			// Polled rather than cached off EVENT_ID_MOUSEMOVE so the brush keeps tracking while
			// the camera turns under a stationary cursor. Already in device coords.
			EventInputGetMousePosition(&state.mouseX, &state.mouseY);
			worldFrame->MouseToWorld(state.mouseX, state.mouseY, &state.rayStart, &state.rayEnd);

			C3Vector hit{};
			if (!Access::RaycastTerrain(state.rayStart, state.rayEnd, hit, &state.rayFraction))
			{
				state.lastMiss = Access::IsActive() ? "raycast missed terrain" : "CMap is not active";
				return;
			}

			state.lastMiss = "";
			state.cursor = At(hit);
		}

		// Where a command from Lua should act: the cursor when there is one, otherwise the
		// character, otherwise the camera. The addon's buttons are only clickable with the editor
		// off, and with it off nothing tracks a cursor, so without the fallback every button would
		// report no terrain.
		CursorInfo Focus()
		{
			RuntimeState& state = State();
			if (state.cursor.valid)
				return state.cursor;

			C3Vector at{};
			if (CGObject_C* player = ObjectManager::GetActivePlayerObject())
			{
				player->GetPosition(at);
				return At(at);
			}

			CGWorldFrameFull* worldFrame = CGWorldFrameFull::Current();
			if (!worldFrame || !worldFrame->m_camera)
				return {};

			return At(worldFrame->m_camera->m_position);
		}

		// Where a list of what is standing around you should measure from. Focus follows the mouse,
		// which is right for a brush and wrong here: aim the camera across the valley and every
		// distance in the list belongs to the valley rather than to you.
		CursorInfo StandingAt()
		{
			C3Vector at{};
			if (CGObject_C* player = ObjectManager::GetActivePlayerObject())
			{
				player->GetPosition(at);
				return At(at);
			}

			return Focus();
		}

		// Plots the hovered chunk's 145 MCVT vertices as a wire grid. It doubles as the check on
		// Coords::VertexWorldPos, since transposed row and column axes would show up as a grid
		// sitting rotated against the terrain.
		void DrawChunkGrid(CGWorldFrameFull* worldFrame)
		{
			RuntimeState& state = State();
			CursorInfo const& cursor = state.cursor;
			if (!cursor.valid)
				return;

			CMapArea* area = Access::GetArea(cursor.tileX, cursor.tileY);
			CMapChunk* chunk = Access::GetChunk(area, cursor.chunkX, cursor.chunkY);
			if (!chunk || !chunk->vertices)
				return;

			CGxDevice* device = CGxDevice::Get();
			if (!device)
				return;

			C3Vector const& camera = worldFrame->m_camera->m_position;
			CImVector outer{};
			outer.value = 0xFF30FF30;
			CImVector inner{};
			inner.value = 0xFF208020;

			device->Push();
			GxRsSet(GxRs_VertexShader, nullptr);
			GxRsSet(GxRs_PixelShader, nullptr);
			GxRsSet_int32_t(GxRs_Culling, 0);
			GxRsSet_int32_t(GxRs_Fog, 0);
			GxRsSet_int32_t(GxRs_Lighting, 0);
			GxRsSet_int32_t(GxRs_DepthTest, 1);

			// Lifted off the surface, otherwise the lines z-fight with the terrain they trace and
			// mostly disappear.
			auto vertexAt = [&](int32_t index)
			{
				C3Vector world = Coords::VertexWorldPos(chunk, index);
				world.z += 0.15f;
				return VectorMath::Subtract(world, camera);
			};

			for (int32_t row = 0; row < 9; ++row)
			{
				for (int32_t col = 0; col < 9; ++col)
				{
					int32_t here = Coords::EncodeOuterVertex(row, col);
					if (col < 8)
						GizmoDraw::DrawLine(vertexAt(here), vertexAt(Coords::EncodeOuterVertex(row, col + 1)), outer);
					if (row < 8)
						GizmoDraw::DrawLine(vertexAt(here), vertexAt(Coords::EncodeOuterVertex(row + 1, col)), outer);
				}
			}

			// One diagonal per cell so the interleaved 8x8 vertices are visible too.
			for (int32_t row = 0; row < 8; ++row)
			{
				for (int32_t col = 0; col < 8; ++col)
				{
					C3Vector center = vertexAt(Coords::EncodeInnerVertex(row, col));
					GizmoDraw::DrawLine(vertexAt(Coords::EncodeOuterVertex(row, col)), center, inner);
					GizmoDraw::DrawLine(center, vertexAt(Coords::EncodeOuterVertex(row + 1, col + 1)), inner);
				}
			}

			device->Pop();
		}

		// Grows the gizmo with distance so it stays grabbable whether the model is underfoot or
		// across the valley. Picking and drawing both read this and have to agree, which is why it
		// is a function rather than two constants.
		float GizmoScale(C3Vector const& position, C3Vector const& camera)
		{
			float distance = VectorMath::Length(VectorMath::Subtract(position, camera));
			float scale = distance * 0.12f;
			if (scale < 1.0f)
				scale = 1.0f;
			if (scale > 25.0f)
				scale = 25.0f;

			return scale;
		}

		void ClearSelection()
		{
			RuntimeState& state = State();
			state.selection = {};
			state.selected = {};
			state.selectedName.clear();
			state.hoverAxis = GPick::Axis::None;
			state.hoverRing = GPick::Axis::None;
			state.drag = {};
		}

		// Re-reads the selection from the document, which is what keeps the working transform
		// honest after an edit is rejected or the entry moves under us.
		bool RefreshSelection()
		{
			RuntimeState& state = State();
			if (!state.selection.Valid())
				return false;

			Placements::Info info;
			if (!Placements::Describe(state.selection, info))
			{
				ClearSelection();
				return false;
			}

			state.selected = info.transform;
			state.selectedName = info.name;
			return true;
		}

		void PrintSelection()
		{
			RuntimeState& state = State();
			Placements::Ref const& ref = state.selection;
			if (!ref.Valid())
			{
				Print("MapEditor: nothing selected");
				return;
			}

			Print("MapEditor: %s %d in tile %d_%d  %s", ref.kind == Placements::Kind::Doodad ? "doodad" : "wmo",
			    ref.index, ref.tileX, ref.tileY,
			    state.selectedName.empty() ? "(name unknown)" : state.selectedName.c_str());
			Print("  pos %.2f %.2f %.2f  rot %.1f %.1f %.1f  scale %.3f  uid %u", state.selected.position.x,
			    state.selected.position.y, state.selected.position.z, state.selected.rotation.x,
			    state.selected.rotation.y, state.selected.rotation.z, state.selected.scale, ref.uniqueId);
		}

		// Writes the working transform back to the document. Only called when a drag ends or a
		// command runs, never per frame, because it rewrites all 256 MCRFs.
		void CommitSelection()
		{
			RuntimeState& state = State();
			if (!state.selection.Valid())
				return;

			int32_t wasX = state.selection.tileX;
			int32_t wasY = state.selection.tileY;

			std::string error;
			if (Placements::SetTransform(state.selection, state.selected, error))
			{
				// Worth saying out loud, since it dirties a tile the player never touched and both
				// of them now need saving.
				if (state.selection.tileX != wasX || state.selection.tileY != wasY)
				{
					Print("MapEditor: moved from tile %d_%d into %d_%d, both need saving", wasX, wasY,
					    state.selection.tileX, state.selection.tileY);
				}

				return;
			}

			Print("MapEditor: %s", error.c_str());
			RefreshSelection();
			Placements::Preview(state.selection, state.selected);
		}

		// Which Euler field one ring turns. MDDF and MODF store the three in the order the loader
		// feeds them to the matrix, rotation.x to RotateAroundY and so on, so the mapping from
		// world axis to field is not the identity.
		float* RotationFieldForRing(C3Vector& rotation, GPick::Axis axis)
		{
			switch (axis)
			{
			case GPick::Axis::X:
				return &rotation.z;
			case GPick::Axis::Y:
				return &rotation.x;
			case GPick::Axis::Z:
				return &rotation.y;
			default:
				return nullptr;
			}
		}

		// Only exact while the other two angles are zero, which is how nearly every placement in
		// the game data sits. Past that a ring still turns the model the way you expect, it just
		// stops being a pure world axis turn.
		void ApplyRingDrag(RuntimeState& state)
		{
			float angle = 0.0f;
			if (!GPick::RayAngleOnRing(state.rayStart, state.rayEnd, state.drag.startPosition,
			        state.drag.ringAxis, state.drag.ringRefDir, state.drag.ringRefPerp, angle))
				return;

			float delta = GPick::NormalizeSignedAngleDelta(angle - state.drag.anchorAngle) * kRadToDeg;

			C3Vector rotation = state.drag.startRotation;
			float* field = RotationFieldForRing(rotation, state.drag.axis);
			if (!field)
				return;

			*field = std::fmod(*field + delta, 360.0f);
			if (*field < 0.0f)
				*field += 360.0f;

			state.selected.rotation = rotation;
			Placements::Preview(state.selection, state.selected);
		}

		void ApplyArrowDrag(RuntimeState& state)
		{
			C3Vector axis = GPick::AxisDirection(state.drag.axis);
			float along = GPick::ClosestRayAxisParameter(state.rayStart, state.rayEnd,
			    state.drag.startPosition, axis);
			C3Vector onAxis = VectorMath::Add(state.drag.startPosition, VectorMath::Scale(axis, along));
			C3Vector delta = VectorMath::Subtract(onAxis, state.drag.anchorOnAxis);

			state.selected.position = VectorMath::Add(state.drag.startPosition, delta);
			Placements::Preview(state.selection, state.selected);
		}

		// Hover, drag and draw for the placement gizmo, all off the ray UpdateCursor just rebuilt.
		void UpdatePlacement(CGWorldFrameFull* worldFrame)
		{
			RuntimeState& state = State();
			if (!PlacementLive() || !state.selection.Valid() || !worldFrame || !worldFrame->m_camera)
				return;

			C3Vector const& camera = worldFrame->m_camera->m_position;
			float scale = GizmoScale(state.selected.position, camera);
			float ringScale = scale * kRingScale;

			if (state.drag.active && state.drag.rotating)
			{
				ApplyRingDrag(state);
			}
			else if (state.drag.active)
			{
				ApplyArrowDrag(state);
			}
			else
			{
				state.hoverAxis = GPick::PickTranslationGizmo(state.rayStart, state.rayEnd,
				    state.selected.position, scale);
				state.hoverRing = GPick::PickRotationGizmo(state.rayStart, state.rayEnd,
				    state.selected.position, ringScale);
			}

			CGxDevice* device = CGxDevice::Get();
			if (!device)
				return;

			device->Push();
			GxRsSet(GxRs_VertexShader, nullptr);
			GxRsSet(GxRs_PixelShader, nullptr);
			GxRsSet_int32_t(GxRs_Culling, 0);
			GxRsSet_int32_t(GxRs_Fog, 0);
			GxRsSet_int32_t(GxRs_Lighting, 0);

			// Drawn through whatever it is attached to. A gizmo buried inside the model it moves is
			// no use.
			GxRsSet_int32_t(GxRs_DepthTest, 0);

			bool dragging = state.drag.active;
			GPick::Axis arrow = dragging ? (state.drag.rotating ? GPick::Axis::None : state.drag.axis)
			                             : state.hoverAxis;
			GPick::Axis ring = dragging ? (state.drag.rotating ? state.drag.axis : GPick::Axis::None)
			                            : state.hoverRing;

			C3Vector origin = VectorMath::Subtract(state.selected.position, camera);
			GizmoDraw::DrawTranslationGizmo(origin, scale, arrow);
			GizmoDraw::DrawRotationGizmo(origin, ringScale, ring);

			device->Pop();
		}

		// A new doodad went in with a guessed MCRF footprint because there was no live instance to
		// measure. The reload has built the model by now, so running the same transform back
		// through SetTransform rewrites the refs from the real bounds.
		void SettleNewPlacement()
		{
			RuntimeState& state = State();
			if (!state.pending.Valid())
				return;

			// The tile may never prepare a chunk near it, or the model may simply not load, so this
			// gives up rather than watching forever.
			if (GetTickCount() > state.pendingUntilMs)
			{
				state.pending = {};
				return;
			}

			if (!Access::FindDoodadDef(state.pending.uniqueId))
				return;

			Placements::Ref ref = state.pending;
			state.pending = {};

			std::string error;
			if (!Placements::SetTransform(ref, state.pendingTransform, error))
				Print("MapEditor: could not settle the new placement - %s", error.c_str());
		}

		bool PublishTile(int32_t tileX, int32_t tileY);

		// One tick of the liquid brush. It either fills the cells under the brush or it does not,
		// so it runs on a timer rather than on elapsed time, and a full MH2O rebuild per tile is
		// too much to do every frame.
		constexpr uint32_t kLiquidTickMs = 100;

		void StrokeLiquid(bool erase)
		{
			RuntimeState& state = State();
			uint32_t now = GetTickCount();
			if (now - state.lastLiquidMs < kLiquidTickMs)
				return;

			state.lastLiquidMs = now;

			std::string error;
			if (erase)
			{
				Liquids::Remove(state.cursor.position, state.brushRadius, -1, state.liquidPending,
				    error);
				return;
			}

			// strokeTarget was sampled where the drag began, so a drag lays one flat surface rather
			// than a staircase that follows the ground.
			Liquids::Add(state.cursor.position, state.brushRadius, state.liquidType,
			    state.strokeTarget, state.liquidPending, error);
		}

		// Saves and reloads every tile the drag reached, once each. EditTiles appends a tile per
		// call, so the same one is in here many times over by the end of a stroke.
		void FlushLiquidStroke()
		{
			RuntimeState& state = State();
			if (state.liquidPending.empty())
				return;

			std::vector<Liquids::TileRef> tiles;
			tiles.swap(state.liquidPending);

			int32_t published = 0;
			for (size_t i = 0; i < tiles.size(); ++i)
			{
				bool seen = false;
				for (size_t j = 0; j < i && !seen; ++j)
					seen = tiles[j].x == tiles[i].x && tiles[j].y == tiles[i].y;

				if (!seen && PublishTile(tiles[i].x, tiles[i].y))
					++published;
			}

			Print("MapEditor: liquid stroke over %d tile%s", published, published == 1 ? "" : "s");
		}

		// Runs one frame of the held brush, scaled by real elapsed time so the result does not
		// depend on how fast the machine is drawing.
		void UpdateStroke()
		{
			RuntimeState& state = State();
			uint32_t now = GetTickCount();

			if (!state.painting || !state.cursor.valid)
			{
				state.lastStrokeMs = now;
				return;
			}

			uint32_t elapsed = now - state.lastStrokeMs;
			state.lastStrokeMs = now;

			// A long stall, usually a load screen, should not land as one enormous stroke.
			if (!elapsed || elapsed > 250)
				return;

			// Shift inverts, which is the usual convention and saves a mode switch mid-stroke.
			// Only the modes that move by a signed step have anything to invert.
			bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			float direction = (shift != state.sink) ? -1.0f : 1.0f;
			float seconds = elapsed / 1000.0f;

			if (state.tool == Tool::Liquid)
			{
				StrokeLiquid(shift);
				return;
			}

			// Blend modes cannot use a per-second rate the way raise does, since what they move by
			// depends on how far off the target already is. Read the strength as a rate of closing
			// instead, calibrated so the default of 8 closes the gap in about a second.
			float closing = state.brushStrength * seconds / 8.0f;
			if (closing > 1.0f)
				closing = 1.0f;

			if (state.tool == Tool::Paint)
			{
				TextureBrush::Stroke paint;
				paint.center = state.cursor.position;
				paint.radius = state.brushRadius;
				paint.falloff = state.falloff;
				paint.layer = state.paintLayer;

				// Shift erases the chosen layer rather than laying it down, matching what it does
				// to the sculpt brush.
				paint.blend = shift ? -closing : closing;

				TextureBrush::Apply(paint);
				return;
			}

			Sculpt::Stroke stroke;
			stroke.center = state.cursor.position;
			stroke.radius = state.brushRadius;
			stroke.falloff = state.falloff;
			stroke.mode = state.mode;
			stroke.targetHeight = state.strokeTarget;
			stroke.amount = direction * state.brushStrength * seconds;
			stroke.blend = closing;

			Sculpt::Apply(stroke);
		}

		int32_t OnMouseDown(const void* rawData, void*)
		{
			RuntimeState& state = State();
			if (!AnyModeLive())
				return 1;

			auto const* data = static_cast<EventDataMouse const*>(rawData);
			if (!data || data->button != MOUSE_BUTTON_LEFT)
				return 1;

			// The placement tool works off the ray alone, so unlike the brushes it has something to
			// do even when the cursor is over sky or over a model rather than over terrain.
			if (PlacementLive())
			{
				// An arrow under the cursor wins over picking, or grabbing one that crosses another
				// model would select that model instead. A ring is second, since an arrow crosses
				// its own rings near the origin.
				if (state.selection.Valid() && state.hoverAxis != GPick::Axis::None)
				{
					C3Vector axis = GPick::AxisDirection(state.hoverAxis);
					float along = GPick::ClosestRayAxisParameter(state.rayStart, state.rayEnd,
					    state.selected.position, axis);

					state.drag = {};
					state.drag.active = true;
					state.drag.axis = state.hoverAxis;
					state.drag.startPosition = state.selected.position;
					state.drag.anchorOnAxis
					    = VectorMath::Add(state.selected.position, VectorMath::Scale(axis, along));
					return 0;
				}

				if (state.selection.Valid() && state.hoverRing != GPick::Axis::None)
				{
					C3Vector ringAxis{};
					C3Vector refDir{};
					C3Vector refPerp{};
					GPick::RingBasis(state.hoverRing, ringAxis, refDir, refPerp);

					float angle = 0.0f;
					if (!GPick::RayAngleOnRing(state.rayStart, state.rayEnd, state.selected.position,
					        ringAxis, refDir, refPerp, angle))
						return 0;

					state.drag = {};
					state.drag.active = true;
					state.drag.rotating = true;
					state.drag.axis = state.hoverRing;
					state.drag.startPosition = state.selected.position;
					state.drag.startRotation = state.selected.rotation;
					state.drag.anchorAngle = angle;
					state.drag.ringAxis = ringAxis;
					state.drag.ringRefDir = refDir;
					state.drag.ringRefPerp = refPerp;
					return 0;
				}

				Placements::Ref picked;
				if (!Placements::Pick(state.rayStart, state.rayEnd, picked))
				{
					// With the editor off the click was probably meant for the game, so let it
					// through untouched rather than eating it to say nothing was there.
					if (PlaceOnly())
						return 1;

					ClearSelection();
					Print("MapEditor: no placement under the cursor");
					return 0;
				}

				ClearSelection();
				state.selection = picked;
				if (RefreshSelection())
					PrintSelection();

				return 0;
			}

			if (!state.cursor.valid)
				return 1;

			state.painting = true;
			state.lastStrokeMs = GetTickCount();
			state.strokeTarget = state.cursor.position.z;

			if (state.tool == Tool::Liquid)
			{
				// Above the ground the drag started on, not level with it, or the surface comes out
				// buried in the terrain it is supposed to sit on.
				state.strokeTarget += state.liquidOffset;

				// Far enough back that the first tick lands on this frame instead of a tenth of a
				// second into the drag.
				state.lastLiquidMs = state.lastStrokeMs - kLiquidTickMs;
			}

			// Consumed, otherwise the same drag that paints also turns the camera.
			return 0;
		}

		int32_t OnMouseUp(const void* rawData, void*)
		{
			auto const* data = static_cast<EventDataMouse const*>(rawData);
			if (!data || data->button != MOUSE_BUTTON_LEFT)
				return 1;

			RuntimeState& state = State();
			state.painting = false;
			FlushLiquidStroke();

			if (state.drag.active)
			{
				state.drag = {};
				CommitSelection();
			}

			return 1;
		}

		int SetBrushStrength(lua_State* L)
		{
			if (FrameScript::IsNumber(L, 1))
				State().brushStrength = static_cast<float>(FrameScript::GetNumber(L, 1));

			Print("MapEditor: brush strength %.2f yards/sec", State().brushStrength);
			return 0;
		}

		int SetBrushMode(lua_State* L)
		{
			char const* name = FrameScript::ToLString(L, 1, false);
			RuntimeState& state = State();

			if (name && !std::strcmp(name, "lower"))
			{
				state.mode = Sculpt::Mode::Raise;
				state.sink = true;
			}
			else if (name && !std::strcmp(name, "flatten"))
			{
				state.mode = Sculpt::Mode::Flatten;
				state.sink = false;
			}
			else if (name && !std::strcmp(name, "smooth"))
			{
				state.mode = Sculpt::Mode::Smooth;
				state.sink = false;
			}
			else if (name && !std::strcmp(name, "noise"))
			{
				state.mode = Sculpt::Mode::Noise;
				state.sink = false;
			}
			else
			{
				state.mode = Sculpt::Mode::Raise;
				state.sink = false;
				name = "raise";
			}

			Print("MapEditor: brush mode %s (hold shift to invert)", name);
			return 0;
		}

		int SetTool(lua_State* L)
		{
			char const* name = FrameScript::ToLString(L, 1, false);
			RuntimeState& state = State();

			if (name && !std::strcmp(name, "paint"))
			{
				state.tool = Tool::Paint;
				name = "paint";
			}
			else if (name && (!std::strcmp(name, "place") || !std::strcmp(name, "move")))
			{
				state.tool = Tool::Place;
				name = "place - click a model to select it, drag an arrow to move it";
			}
			else if (name && (!std::strcmp(name, "liquid") || !std::strcmp(name, "water")))
			{
				state.tool = Tool::Liquid;
				name = "liquid - drag to fill, hold shift to clear";
			}
			else
			{
				state.tool = Tool::Sculpt;
				name = "sculpt";
			}

			if (state.tool != Tool::Place)
				ClearSelection();

			Print("MapEditor: tool %s", name);
			return 0;
		}

		int SetPaintLayer(lua_State* L)
		{
			RuntimeState& state = State();

			if (FrameScript::IsNumber(L, 1))
			{
				int32_t layer = static_cast<int32_t>(FrameScript::GetNumber(L, 1));
				state.paintLayer = layer < 0 ? 0 : (layer > 3 ? 3 : layer);
			}

			Print("MapEditor: paint layer %d (hold shift to erase it)", state.paintLayer);
			return 0;
		}

		// What the hovered chunk is actually made of. Layers are per chunk while the texture list
		// is per tile, so a layer number on its own says nothing without this.
		int ListTextures(lua_State*)
		{
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no terrain under the cursor");
				return 0;
			}

			CMapArea* area = Access::GetArea(cursor.tileX, cursor.tileY);
			CMapChunk* chunk = Access::GetChunk(area, cursor.chunkX, cursor.chunkY);
			if (!Access::IsAreaReady(area) || !chunk || !chunk->header || !chunk->layers)
			{
				Print("MapEditor: tile %d,%d is not loaded", cursor.tileX, cursor.tileY);
				return 0;
			}

			Print("MapEditor: chunk %d,%d of tile %d,%d has %u layer%s of %u tile textures",
			    cursor.chunkX, cursor.chunkY, cursor.tileX, cursor.tileY, chunk->header->nLayers,
			    chunk->header->nLayers == 1 ? "" : "s", area->textureCount);

			// Coverage under the cursor as well as the names. It is the quickest way to tell a
			// brush that is not running from one that is running and not showing.
			std::string error;
			Session::OpenTile* tile = Session::Open(cursor.tileX, cursor.tileY, error);
			Adt::Mcnk const* mcnk = tile ? tile->doc.ChunkAt(cursor.chunkX, cursor.chunkY) : nullptr;

			Adt::ChunkAlpha alpha;
			bool haveAlpha = mcnk
			    && Adt::ReadChunkAlpha(*mcnk, Adt::FormatFor(*mcnk, *Access::sMapFlags), alpha,
			        error);

			int32_t texel = 0;
			if (haveAlpha)
			{
				int32_t row = static_cast<int32_t>(
				    (chunk->topLeftCoords.x - cursor.position.x) / Coords::kTexelSize);
				int32_t col = static_cast<int32_t>(
				    (chunk->topLeftCoords.y - cursor.position.y) / Coords::kTexelSize);

				row = row < 0 ? 0 : (row > 63 ? 63 : row);
				col = col < 0 ? 0 : (col > 63 ? 63 : col);
				texel = Coords::EncodeTexel(row, col);
			}

			int32_t base = 255;
			for (uint32_t i = 1; haveAlpha && i < alpha.maps.size(); ++i)
				base -= alpha.maps[i].texel[texel];

			for (uint32_t i = 0; i < chunk->header->nLayers && i < 4; ++i)
			{
				SMLayer const& layer = chunk->layers[i];
				char const* name = Access::TerrainTextureName(area, layer.textureId);

				if (!haveAlpha || i >= alpha.maps.size())
				{
					Print("  layer %u -> tex %u  %s", i, layer.textureId, name ? name : "?");
					continue;
				}

				int32_t here = i ? alpha.maps[i].texel[texel] : (base < 0 ? 0 : base);
				Print("  layer %u -> tex %u  alpha %d  %s", i, layer.textureId, here,
				    name ? name : "?");
			}

			return 0;
		}

		// Everything the hovered tile can offer a layer. A texture has to be in this list before a
		// chunk can use it, and MapEditor_AddLayer is what puts one there.
		int ListTileTextures(lua_State*)
		{
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no terrain under the cursor");
				return 0;
			}

			std::vector<std::string> names;
			std::string error;
			if (!TextureLayers::ListTileTextures(cursor.tileX, cursor.tileY, names, error))
			{
				Print("MapEditor: %s", error.c_str());
				return 0;
			}

			Print("MapEditor: tile %d,%d has %u texture%s", cursor.tileX, cursor.tileY,
			    static_cast<uint32_t>(names.size()), names.size() == 1 ? "" : "s");

			for (size_t i = 0; i < names.size(); ++i)
				Print("  %u  %s", static_cast<uint32_t>(i), names[i].c_str());

			return 0;
		}

		// Lays a texture over every chunk the brush reaches as a new top layer, covering nothing
		// until it is painted. The whole footprint rather than one chunk, because a layer is only
		// useful once every chunk a stroke crosses has it.
		int AddLayer(lua_State* L)
		{
			RuntimeState& state = State();
			char const* texture = FrameScript::ToLString(L, 1, false);

			if (!texture || !*texture)
			{
				Print("MapEditor: MapEditor_AddLayer(\"Tileset\\\\...\\\\Something.blp\")");
				return 0;
			}

			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no cursor and no character to work from");
				return 0;
			}

			int32_t skipped = 0;
			std::string error;
			int32_t added = TextureLayers::Add(cursor.position, state.brushRadius, texture,
			    skipped, error);

			if (!added)
			{
				Print("MapEditor: added nothing - %s", error.c_str());
				return 0;
			}

			Print("MapEditor: added %s to %d chunk%s, skipped %d", texture, added,
			    added == 1 ? "" : "s", skipped);
			Print("  paint it with MapEditor_ListTextures to find its layer number");
			return 0;
		}

		// Drops a layer from every chunk the brush reaches. Whatever it covered goes back to the
		// layers underneath, since the base is simply what the masks leave over.
		int RemoveLayer(lua_State* L)
		{
			RuntimeState& state = State();
			int32_t layer = FrameScript::IsNumber(L, 1)
			    ? static_cast<int32_t>(FrameScript::GetNumber(L, 1))
			    : state.paintLayer;

			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no cursor and no character to work from");
				return 0;
			}

			int32_t skipped = 0;
			std::string error;
			int32_t removed = TextureLayers::Remove(cursor.position, state.brushRadius, layer,
			    skipped, error);

			if (!removed)
			{
				Print("MapEditor: removed nothing - %s", error.c_str());
				return 0;
			}

			// Worth saying out loud: layers above the removed one all shuffle down by one, so a
			// paint layer chosen before this now means a different texture.
			Print("MapEditor: removed layer %d from %d chunk%s, skipped %d", layer, removed,
			    removed == 1 ? "" : "s", skipped);
			Print("  layers above it have shifted down one");
			return 0;
		}

		int SetFalloff(lua_State* L)
		{
			char const* name = FrameScript::ToLString(L, 1, false);
			RuntimeState& state = State();

			if (name && !std::strcmp(name, "linear"))
				state.falloff = Sculpt::Falloff::Linear;
			else if (name && !std::strcmp(name, "flat"))
				state.falloff = Sculpt::Falloff::Flat;
			else
				state.falloff = Sculpt::Falloff::Smooth;

			Print("MapEditor: falloff %s", name ? name : "smooth");
			return 0;
		}

		// Writes every tile the brush has touched. Painting only changes the client's live buffer
		// and the in-memory document, so nothing survives a reload until this runs.
		int SaveEdits(lua_State*)
		{
			int32_t saved = 0;
			std::string error;

			if (!Session::SaveDirty(saved, error))
			{
				Print("MapEditor: save failed after %d tiles - %s", saved, error.c_str());
				return 0;
			}

			if (!saved)
				Print("MapEditor: nothing to save");
			else
				Print("MapEditor: saved %d tile%s", saved, saved == 1 ? "" : "s");

			return 0;
		}

		// Puts every open tile back to its backup and reloads, which is the way out of a stroke
		// that went wrong.
		int RevertEdits(lua_State*)
		{
			// Reverting first, then forgetting. RevertAll purges the tiles, which is what stops
			// anything still pointing at the MCLY, MCAL and texture name buffers the editor owns.
			int32_t reverted = Session::RevertAll();
			TextureBrush::Reset();
			TextureLayers::Reset();
			Print("MapEditor: reverted %d tile%s", reverted, reverted == 1 ? "" : "s");
			return 0;
		}

		int DumpTile(lua_State*)
		{
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no terrain under the cursor");
				return 0;
			}

			CMapArea* area = Access::GetArea(cursor.tileX, cursor.tileY);
			if (!Access::IsAreaReady(area))
			{
				Print("MapEditor: tile %d,%d is not loaded", cursor.tileX, cursor.tileY);
				return 0;
			}

			Print("MapEditor: %s_%d_%d  file %p size %d  index %d,%d", Access::sMapName,
			    cursor.tileX, cursor.tileY, area->filePtr, area->fileSize, area->index.x, area->index.y);
			Print("  header %p flags 0x%X  mcin +0x%X mtex +0x%X mmdx +0x%X mddf +0x%X modf +0x%X mh2o +0x%X",
			    area->header, area->header->flags, area->header->mcin, area->header->mtex,
			    area->header->mmdx, area->header->mddf, area->header->modf, area->header->mh2o);
			Print("  doodads %d  wmos %d", area->doodadDefCount, area->mapObjDefCount);

			for (int32_t i = 0; i < area->doodadDefCount && i < 3; ++i)
			{
				SMDoodadDef const& def = area->doodadDef[i];
				const char* name = Access::DoodadFileName(area, def.nameId);
				Print("  MDDF[%d] id %u uid %u (%.1f %.1f %.1f) %s", i, def.nameId, def.uniqueId,
				    def.position.x, def.position.y, def.position.z, name ? name : "?");
			}

			for (int32_t i = 0; i < area->mapObjDefCount && i < 3; ++i)
			{
				SMMapObjDef const& def = area->mapObjDef[i];
				const char* name = Access::MapObjFileName(area, def.nameId);
				Print("  MODF[%d] id %u uid %u (%.1f %.1f %.1f) %s", i, def.nameId, def.uniqueId,
				    def.position.x, def.position.y, def.position.z, name ? name : "?");
			}

			CMapChunk* chunk = Access::GetChunk(area, cursor.chunkX, cursor.chunkY);
			SMChunkInfo* info = Access::GetChunkInfo(area, cursor.chunkX, cursor.chunkY);
			if (chunk && chunk->header && info)
			{
				SMChunk const* header = chunk->header;
				uint32_t fileOffset =
				    static_cast<uint32_t>(chunk->chunkInfoBeginPtr - static_cast<uint8_t*>(area->filePtr));

				Print("  chunk %d,%d  MCIN off 0x%X size %u flags 0x%X  ptr off 0x%X", cursor.chunkX,
				    cursor.chunkY, info->offset, info->size, info->flags, fileOffset);
				Print("    index %d,%d  areaId %d  layers %u  doodadRefs %u  wmoRefs %u  holes 0x%X",
				    header->index.x, header->index.y, chunk->areaId, header->nLayers,
				    header->nDoodadRefs, header->nMapObjRefs, header->holes);
				Print("    position (%.2f %.2f %.2f)  topLeftCoords (%.2f %.2f %.2f)", header->position.x,
				    header->position.y, header->position.z, chunk->topLeftCoords.x,
				    chunk->topLeftCoords.y, chunk->topLeftCoords.z);
				Print("    ofsAlpha 0x%X sizeAlpha %u  ofsLiquid 0x%X sizeLiquid %u  ofsMCCV 0x%X",
				    header->ofsAlpha, header->sizeAlpha, header->ofsLiquid, header->sizeLiquid,
				    header->ofsMCCV);
				Print("    height[0] %.3f -> world %.3f   height[144] %.3f -> world %.3f",
				    chunk->vertices[0], Coords::VertexHeight(chunk, 0), chunk->vertices[144],
				    Coords::VertexHeight(chunk, 144));

				for (uint32_t layer = 0; layer < header->nLayers && layer < 4; ++layer)
					Print("    MCLY[%u] tex %u flags 0x%X ofsAlpha 0x%X", layer,
					    chunk->layers[layer].textureId, chunk->layers[layer].flags,
					    chunk->layers[layer].offsetInMCAL);
			}

			return 0;
		}

		// What a sweep actually exercised. A clean round-trip over tiles that all look alike
		// proves very little, so the sweep reports which features it really touched.
		struct Coverage
		{
			int32_t tiles = 0;
			int32_t mh2o = 0;
			int32_t mfbo = 0;
			int32_t mtxf = 0;
			int32_t mclq = 0;
			int32_t mcsh = 0;
			int32_t mccv = 0;
			int32_t maxLayers = 0;

			// Alpha codec, counted per chunk rather than per tile since a single tile mixes
			// formats freely.
			int32_t alphaChunks = 0;     // chunks carrying at least one blend mask
			int32_t alphaCompressed = 0; // ... of which at least one mask was RLE
			int32_t alphaFailed = 0;     // decode refused the data
			int32_t alphaMismatch = 0;   // decode and encode are not inverse, a real codec bug
			int32_t alphaExact = 0;      // re-encoded MCAL matched the file byte for byte
			bool bigAlpha = false;
		};

		// Checks the alpha codec against one chunk, since decoding then re-encoding has to
		// reproduce the same texels or painting would quietly corrupt a layer. Byte equality
		// against the file is tracked separately because re-encoding always writes uncompressed.
		void MeasureAlpha(Adt::Mcnk const& chunk, uint32_t mapFlags, Coverage& cov)
		{
			Adt::AlphaFormat format = Adt::FormatFor(chunk, mapFlags);

			bool any = false;
			bool compressed = false;
			for (uint32_t i = 0; i < chunk.header.nLayers; ++i)
			{
				Adt::SubChunk const* mcly = chunk.Find(Adt::kMCLY);
				if (!mcly || mcly->data.size() < (i + 1) * sizeof(SMLayer))
					break;

				SMLayer layer{};
				std::memcpy(&layer, mcly->data.data() + i * sizeof(SMLayer), sizeof(SMLayer));

				if (layer.flags & Adt::kLayerUseAlpha)
					any = true;
				if (layer.flags & Adt::kLayerCompressed)
					compressed = true;
			}

			if (!any)
				return;

			++cov.alphaChunks;
			cov.alphaCompressed += compressed ? 1 : 0;

			std::string error;
			Adt::ChunkAlpha decoded;
			if (!Adt::ReadChunkAlpha(chunk, format, decoded, error))
			{
				++cov.alphaFailed;
				return;
			}

			Adt::Mcnk rebuilt = chunk;
			if (!Adt::WriteChunkAlpha(decoded, format, rebuilt, error))
			{
				++cov.alphaFailed;
				return;
			}

			Adt::ChunkAlpha again;
			if (!Adt::ReadChunkAlpha(rebuilt, format, again, error))
			{
				++cov.alphaFailed;
				return;
			}

			if (decoded.maps.size() != again.maps.size())
			{
				++cov.alphaMismatch;
				return;
			}

			for (size_t i = 0; i < decoded.maps.size(); ++i)
			{
				if (std::memcmp(decoded.maps[i].texel, again.maps[i].texel, Adt::kAlphaTexels) != 0)
				{
					++cov.alphaMismatch;
					return;
				}
			}

			Adt::SubChunk const* before = chunk.Find(Adt::kMCAL);
			Adt::SubChunk const* after = rebuilt.Find(Adt::kMCAL);
			if (before && after && before->data == after->data)
				++cov.alphaExact;
		}

		void Measure(Adt::AdtDocument const& doc, Coverage& cov, uint32_t mapFlags)
		{
			++cov.tiles;
			cov.bigAlpha = (mapFlags & 0x4) != 0;

			for (Adt::TopChunk const& top : doc.order)
			{
				if (top.data.empty())
					continue;

				if (top.id == Adt::kMH2O)
					++cov.mh2o;
				else if (top.id == Adt::kMFBO)
					++cov.mfbo;
				else if (top.id == Adt::kMTXF)
					++cov.mtxf;
			}

			bool mclq = false;
			bool mcsh = false;
			bool mccv = false;

			for (Adt::Mcnk const& chunk : doc.chunks)
			{
				if (static_cast<int32_t>(chunk.header.nLayers) > cov.maxLayers)
					cov.maxLayers = static_cast<int32_t>(chunk.header.nLayers);

				MeasureAlpha(chunk, mapFlags, cov);

				for (Adt::SubChunk const& sub : chunk.subs)
				{
					if (sub.data.empty())
						continue;

					if (sub.id == Adt::kMCLQ)
						mclq = true;
					else if (sub.id == Adt::kMCSH)
						mcsh = true;
					else if (sub.id == Adt::kMCCV)
						mccv = true;
				}
			}

			cov.mclq += mclq ? 1 : 0;
			cov.mcsh += mcsh ? 1 : 0;
			cov.mccv += mccv ? 1 : 0;
		}

		// A sweep over a whole map hits far more absent tiles than present ones, so "no such
		// file" has to be distinguishable from a genuine failure rather than both being false.
		enum class TripResult
		{
			Missing,
			Failed,
			Ok
		};

		// Reads a tile off disk, parses it, writes it back out and compares. Nothing the writer
		// produces can be trusted until the bytes come back identical, and `detail` caps the
		// per-difference reporting so a map wide sweep does not flood chat.
		TripResult RoundTripTile(int32_t tileX, int32_t tileY, bool verbose,
		    char const* mapName = nullptr, Coverage* cov = nullptr, bool detail = true,
		    uint32_t mapFlags = 0)
		{
			std::string path = mapName ? Adt::TilePath(mapName, tileX, tileY)
			                           : Adt::TilePath(tileX, tileY);
			std::string error;

			std::vector<uint8_t> original;
			if (!Adt::ReadFileBytes(path.c_str(), original, error))
			{
				if (verbose)
					Print("MapEditor: %d,%d read failed - %s", tileX, tileY, error.c_str());
				return TripResult::Missing;
			}

			Adt::AdtDocument doc;
			if (!Adt::Read(original.data(), static_cast<uint32_t>(original.size()), doc, error))
			{
				if (detail)
					Print("MapEditor: %d,%d parse failed - %s", tileX, tileY, error.c_str());
				return TripResult::Failed;
			}

			if (cov)
				Measure(doc, *cov, mapFlags);

			std::vector<uint8_t> rebuilt;
			if (!Adt::Write(doc, rebuilt, error))
			{
				if (detail)
					Print("MapEditor: %d,%d write failed - %s", tileX, tileY, error.c_str());
				return TripResult::Failed;
			}

			if (verbose)
				Print("MapEditor: %s  %u bytes  %u top chunks  %u MCNK", path.c_str(),
				    static_cast<uint32_t>(original.size()), static_cast<uint32_t>(doc.order.size()),
				    static_cast<uint32_t>(doc.chunks.size()));

			if (rebuilt.size() != original.size())
			{
				if (detail)
					Print("MapEditor: %d,%d size differs - wrote %u, expected %u", tileX, tileY,
					    static_cast<uint32_t>(rebuilt.size()),
					    static_cast<uint32_t>(original.size()));
				return TripResult::Failed;
			}

			uint32_t size = static_cast<uint32_t>(original.size());

			// The verdict comes from the whole buffer, never from the reporting loop below, so
			// that suppressing output cannot turn a mismatch into a pass.
			if (std::memcmp(original.data(), rebuilt.data(), size) == 0)
			{
				if (verbose)
					Print("MapEditor: %d,%d round-trip is byte identical", tileX, tileY);
				return TripResult::Ok;
			}

			// Report differing runs rather than individual bytes, otherwise one wrong field
			// floods the chat frame.
			uint32_t runs = 0;
			uint32_t i = 0;

			while (i < size && runs < 8 && detail)
			{
				if (original[i] == rebuilt[i])
				{
					++i;
					continue;
				}

				uint32_t start = i;
				while (i < size && original[i] != rebuilt[i])
					++i;

				uint32_t length = i - start;
				uint32_t was = 0;
				uint32_t now = 0;
				std::memcpy(&was, original.data() + start, length < 4 ? length : 4);
				std::memcpy(&now, rebuilt.data() + start, length < 4 ? length : 4);

				Print("  diff at 0x%X (%u bytes) %s: was 0x%X now 0x%X", start, length,
				    Adt::DescribeOffset(doc, start).c_str(), was, now);

				// A header field diff only says which field is wrong, not which sub-chunk fed
				// it. Dump the inventory once so the branch the writer took is observable.
				if (runs == 0)
				{
					int32_t mcnk = Adt::ChunkIndexAtOffset(doc, start);
					if (mcnk >= 0)
					{
						for (Adt::SubChunk const& sub : doc.chunks[mcnk].subs)
							Print("    MCNK %d %s declares %u occupies %u", mcnk,
							    Adt::ChunkName(sub.id).c_str(), sub.DeclaredSize(),
							    static_cast<uint32_t>(sub.data.size()));
					}
				}

				++runs;
			}

			if (detail)
				Print("MapEditor: %d,%d round-trip MISMATCH", tileX, tileY);

			return TripResult::Failed;
		}

		int RoundTrip(lua_State*)
		{
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no terrain under the cursor");
				return 0;
			}

			RoundTripTile(cursor.tileX, cursor.tileY, true);
			return 0;
		}

		// Every tile currently resident, which is the cheapest way to cover the spread the plan
		// asks for: water and none, dungeon and outdoor, one and four texture layers.
		int RoundTripAll(lua_State*)
		{
			int32_t tested = 0;
			int32_t failed = 0;

			for (int32_t tileY = 0; tileY < 64; ++tileY)
			{
				for (int32_t tileX = 0; tileX < 64; ++tileX)
				{
					if (!Access::IsAreaReady(Access::GetArea(tileX, tileY)))
						continue;

					++tested;
					if (RoundTripTile(tileX, tileY, false) != TripResult::Ok)
						++failed;
				}
			}

			Print("MapEditor: round-tripped %d loaded tiles, %d failed", tested, failed);
			return 0;
		}

		// Every tile of a map, read straight through SFile rather than off the resident set, so
		// water and heavy texture layers can be covered without flying there. Takes a map name so a
		// dungeon can be swept from outdoors.
		int RoundTripMap(lua_State* L)
		{
			char const* mapName = FrameScript::ToLString(L, 1, false);
			if (!mapName || !*mapName)
				mapName = Access::sMapName;

			int32_t tested = 0;
			int32_t failed = 0;
			int32_t reported = 0;
			Coverage cov;

			// The map being swept is often not the one being stood in, so its alpha format has to
			// come from its own WDT rather than the client's loaded copy.
			uint32_t mapFlags = 0;
			std::string flagsError;
			if (!Adt::ReadMapFlags(mapName, mapFlags, flagsError))
				Print("MapEditor: no MPHD for %s (%s), assuming 4 bit alpha", mapName,
				    flagsError.c_str());

			for (int32_t tileY = 0; tileY < 64; ++tileY)
			{
				for (int32_t tileX = 0; tileX < 64; ++tileX)
				{
					// Reporting every failure over a whole map would flood the chat frame, so
					// only the first few carry their diffs.
					bool detail = reported < 4;
					TripResult result = RoundTripTile(tileX, tileY, false, mapName, &cov, detail,
					    mapFlags);

					if (result == TripResult::Missing)
						continue;

					++tested;
					if (result == TripResult::Failed)
					{
						++failed;
						if (detail)
							++reported;
					}
				}
			}

			Print("MapEditor: %s - round-tripped %d tiles, %d failed", mapName, tested, failed);
			Print("  covered: MH2O %d  MFBO %d  MTXF %d  MCLQ %d  MCSH %d  MCCV %d  max layers %d",
			    cov.mh2o, cov.mfbo, cov.mtxf, cov.mclq, cov.mcsh, cov.mccv, cov.maxLayers);
			Print("  alpha: %s  %d chunks  %d compressed  %d exact  %d MISMATCH  %d FAILED",
			    cov.bigAlpha ? "8 bit" : "4 bit", cov.alphaChunks, cov.alphaCompressed,
			    cov.alphaExact, cov.alphaMismatch, cov.alphaFailed);
			return 0;
		}

		// Reads the hovered tile, rebuilds it, writes it to the loose tree and reloads. The rebuild
		// should come back byte identical, so what this really tests is that the client picks the
		// loose file up over the MPQ and the world looks unchanged.
		int SaveTile(lua_State*)
		{
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no terrain under the cursor");
				return 0;
			}

			char const* mapName = Access::sMapName;
			std::string path = Adt::TilePath(mapName, cursor.tileX, cursor.tileY);

			std::vector<uint8_t> original;
			std::string error;
			if (!Adt::ReadFileBytes(path.c_str(), original, error))
			{
				Print("MapEditor: read failed - %s", error.c_str());
				return 0;
			}

			Adt::AdtDocument doc;
			if (!Adt::Read(original.data(), static_cast<uint32_t>(original.size()), doc, error))
			{
				Print("MapEditor: parse failed - %s", error.c_str());
				return 0;
			}

			std::vector<uint8_t> rebuilt;
			if (!Adt::Write(doc, rebuilt, error))
			{
				Print("MapEditor: write failed - %s", error.c_str());
				return 0;
			}

			bool identical = rebuilt.size() == original.size()
			    && std::memcmp(original.data(), rebuilt.data(), original.size()) == 0;

			if (!Adt::SaveTile(mapName, cursor.tileX, cursor.tileY, rebuilt, error))
			{
				Print("MapEditor: save failed - %s", error.c_str());
				return 0;
			}

			Access::ReloadTile(cursor.tileX, cursor.tileY);

			Print("MapEditor: saved %d,%d (%u bytes, %s) and purged",
			    cursor.tileX, cursor.tileY, static_cast<uint32_t>(rebuilt.size()),
			    identical ? "byte identical" : "DIFFERS from source");
			Print("  %s",
			    Adt::DiskTilePath(mapName, cursor.tileX, cursor.tileY).string().c_str());
			return 0;
		}

		// Shifts every MCVT height in the hovered chunk and saves. A byte-identical save cannot
		// tell "the client loaded our loose file" apart from "the client re-read the MPQ", so a
		// visible step at the chunk border settles it.
		int RaiseChunk(lua_State* L)
		{
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no terrain under the cursor");
				return 0;
			}

			float delta = FrameScript::IsNumber(L, 1)
			    ? static_cast<float>(FrameScript::GetNumber(L, 1))
			    : 5.0f;

			char const* mapName = Access::sMapName;
			std::string path = Adt::TilePath(mapName, cursor.tileX, cursor.tileY);

			std::vector<uint8_t> bytes;
			std::string error;
			if (!Adt::ReadFileBytes(path.c_str(), bytes, error))
			{
				Print("MapEditor: read failed - %s", error.c_str());
				return 0;
			}

			Adt::AdtDocument doc;
			if (!Adt::Read(bytes.data(), static_cast<uint32_t>(bytes.size()), doc, error))
			{
				Print("MapEditor: parse failed - %s", error.c_str());
				return 0;
			}

			Adt::Mcnk* chunk = doc.ChunkAt(cursor.chunkX, cursor.chunkY);
			Adt::SubChunk* mcvt = chunk ? chunk->Find(Adt::kMCVT) : nullptr;
			if (!mcvt || mcvt->data.size() < Adt::kMcvtHeights * sizeof(float))
			{
				Print("MapEditor: chunk %d,%d has no usable MCVT", cursor.chunkX, cursor.chunkY);
				return 0;
			}

			// 145 floats, the 9x9 outer grid interleaved with the 8x8 inner one, all relative to
			// the chunk header's position.z.
			float* heights = reinterpret_cast<float*>(mcvt->data.data());
			for (int32_t i = 0; i < Adt::kMcvtHeights; ++i)
				heights[i] += delta;

			std::vector<uint8_t> rebuilt;
			if (!Adt::Write(doc, rebuilt, error))
			{
				Print("MapEditor: write failed - %s", error.c_str());
				return 0;
			}

			if (!Adt::SaveTile(mapName, cursor.tileX, cursor.tileY, rebuilt, error))
			{
				Print("MapEditor: save failed - %s", error.c_str());
				return 0;
			}

			Access::ReloadTile(cursor.tileX, cursor.tileY);

			Print("MapEditor: raised chunk %d,%d of tile %d,%d by %.2f and purged",
			    cursor.chunkX, cursor.chunkY, cursor.tileX, cursor.tileY, delta);
			return 0;
		}

		// Puts the backup back and reloads, so a bad save is one command away from undone.
		int RestoreTile(lua_State*)
		{
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no terrain under the cursor");
				return 0;
			}

			std::string error;
			if (!Adt::RestoreTile(Access::sMapName, cursor.tileX, cursor.tileY, error))
			{
				Print("MapEditor: restore failed - %s", error.c_str());
				return 0;
			}

			Access::ReloadTile(cursor.tileX, cursor.tileY);
			Print("MapEditor: restored %d,%d and purged", cursor.tileX, cursor.tileY);
			return 0;
		}

		// Purges the hovered tile so CMap::PreUpdateAreas streams it back in next frame. Phase 1
		// uses this to settle whether SFile hands back a cached copy or genuinely re-reads.
		int ReloadTile(lua_State*)
		{
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no terrain under the cursor");
				return 0;
			}

			Access::ReloadTile(cursor.tileX, cursor.tileY);
			Print("MapEditor: purged tile %d,%d", cursor.tileX, cursor.tileY);
			return 0;
		}

		char const* ToolName(Tool tool)
		{
			switch (tool)
			{
				case Tool::Paint: return "paint";
				case Tool::Place: return "place";
				case Tool::Liquid: return "liquid";
				default: return "sculpt";
			}
		}

		char const* ModeName(RuntimeState const& state)
		{
			switch (state.mode)
			{
				case Sculpt::Mode::Flatten: return "flatten";
				case Sculpt::Mode::Smooth: return "smooth";
				case Sculpt::Mode::Noise: return "noise";
				default: return state.sink ? "lower" : "raise";
			}
		}

		char const* FalloffName(Sculpt::Falloff falloff)
		{
			switch (falloff)
			{
				case Sculpt::Falloff::Linear: return "linear";
				case Sculpt::Falloff::Flat: return "flat";
				default: return "smooth";
			}
		}

		// Everything a UI needs to draw itself, in one call so it can poll cheaply on a timer.
		// Fields only ever get appended, so an older addon keeps working.
		int GetState(lua_State* L)
		{
			RuntimeState const& state = State();

			FrameScript::PushBoolean(L, state.enabled ? 1 : 0);
			FrameScript::PushString(L, ToolName(state.tool));
			FrameScript::PushString(L, ModeName(state));
			FrameScript::PushString(L, FalloffName(state.falloff));
			FrameScript::PushNumber(L, state.brushRadius);
			FrameScript::PushNumber(L, state.brushStrength);
			FrameScript::PushNumber(L, state.paintLayer);
			FrameScript::PushBoolean(L, state.showGrid ? 1 : 0);
			FrameScript::PushNumber(L, Session::DirtyCount());
			FrameScript::PushBoolean(L, PlaceOnly() ? 1 : 0);
			return 10;
		}

		// nil when nothing is selected, otherwise kind, index, tile, uniqueId, name and transform.
		int GetSelection(lua_State* L)
		{
			RuntimeState& state = State();
			if (!state.selection.Valid())
			{
				FrameScript::PushNil(L);
				return 1;
			}

			Placements::Ref const& ref = state.selection;
			FrameScript::PushString(L, ref.kind == Placements::Kind::Doodad ? "doodad" : "wmo");
			FrameScript::PushNumber(L, ref.index);
			FrameScript::PushNumber(L, ref.tileX);
			FrameScript::PushNumber(L, ref.tileY);
			FrameScript::PushNumber(L, ref.uniqueId);
			FrameScript::PushString(L, state.selectedName.c_str());
			FrameScript::PushNumber(L, state.selected.position.x);
			FrameScript::PushNumber(L, state.selected.position.y);
			FrameScript::PushNumber(L, state.selected.position.z);
			FrameScript::PushNumber(L, state.selected.rotation.x);
			FrameScript::PushNumber(L, state.selected.rotation.y);
			FrameScript::PushNumber(L, state.selected.rotation.z);
			FrameScript::PushNumber(L, state.selected.scale);
			return 13;
		}

		// The hovered chunk's four layer slots as texture names, empty string for a slot the chunk
		// does not use. Always four values so a UI can index them positionally.
		int GetChunkLayers(lua_State* L)
		{
			CursorInfo cursor = Focus();
			CMapArea* area = Access::GetArea(cursor.tileX, cursor.tileY);
			CMapChunk* chunk = Access::GetChunk(area, cursor.chunkX, cursor.chunkY);

			if (!cursor.valid || !Access::IsAreaReady(area) || !chunk || !chunk->header || !chunk->layers)
			{
				FrameScript::PushNil(L);
				return 1;
			}

			for (uint32_t i = 0; i < 4; ++i)
			{
				char const* name = i < chunk->header->nLayers
				    ? Access::TerrainTextureName(area, chunk->layers[i].textureId)
				    : nullptr;

				FrameScript::PushString(L, name ? name : "");
			}

			return 4;
		}

		// No arguments takes whatever the ray is on. With them it takes a ref straight out of
		// MapEditor_GetNearbyPlacements, so a panel can select something the cursor is nowhere near.
		int SelectPlacement(lua_State* L)
		{
			RuntimeState& state = State();
			char const* kind = FrameScript::ToLString(L, 1, false);

			Placements::Ref picked;
			if (kind && *kind)
			{
				picked.kind = std::strcmp(kind, "wmo") == 0 ? Placements::Kind::MapObj
				                                            : Placements::Kind::Doodad;
				picked.tileX = static_cast<int32_t>(FrameScript::GetNumber(L, 2));
				picked.tileY = static_cast<int32_t>(FrameScript::GetNumber(L, 3));
				picked.index = static_cast<int32_t>(FrameScript::GetNumber(L, 4));
				picked.uniqueId = static_cast<uint32_t>(FrameScript::GetNumber(L, 5));

				if (!Placements::Resolve(picked))
				{
					ClearSelection();
					Print("MapEditor: that placement is no longer in tile %d_%d", picked.tileX,
					    picked.tileY);
					return 0;
				}
			}
			else if (!Placements::Pick(state.rayStart, state.rayEnd, picked))
			{
				ClearSelection();
				Print("MapEditor: no placement under the cursor");
				return 0;
			}

			ClearSelection();
			state.selection = picked;
			if (RefreshSelection())
				PrintSelection();

			return 0;
		}

		int PlacementInfo(lua_State*)
		{
			RefreshSelection();
			PrintSelection();
			return 0;
		}

		int ListPlacements(lua_State* L)
		{
			CursorInfo cursor = StandingAt();
			if (!cursor.valid)
			{
				Print("MapEditor: nowhere to measure from");
				return 0;
			}

			float radius = FrameScript::IsNumber(L, 1) ? static_cast<float>(FrameScript::GetNumber(L, 1)) : 30.0f;

			std::vector<Placements::Info> found;
			Placements::ListNear(cursor.position, radius, found);

			Print("MapEditor: %d placements within %.0f yards", static_cast<int32_t>(found.size()), radius);
			for (Placements::Info const& info : found)
			{
				Print("  %s %d in %d_%d  %.0f yards  %s",
				    info.ref.kind == Placements::Kind::Doodad ? "doodad" : "wmo", info.ref.index,
				    info.ref.tileX, info.ref.tileY, info.distance,
				    info.name.empty() ? "(name unknown)" : info.name.c_str());
			}

			return 0;
		}

		// The same scan ListPlacements prints, as a table a panel can list and click. Every field a
		// later MapEditor_SelectPlacement needs comes back with it, so a row stays selectable after
		// the list has gone stale.
		int GetNearbyPlacements(lua_State* L)
		{
			CursorInfo cursor = StandingAt();
			if (!cursor.valid)
			{
				FrameScript::PushNil(L);
				return 1;
			}

			float radius = FrameScript::IsNumber(L, 1)
			    ? static_cast<float>(FrameScript::GetNumber(L, 1))
			    : 30.0f;

			std::vector<Placements::Info> found;
			Placements::ListNear(cursor.position, radius, found);

			FrameScript::CreateTable(L, static_cast<int>(found.size()), 0);

			for (size_t i = 0; i < found.size(); ++i)
			{
				Placements::Info const& info = found[i];

				FrameScript::CreateTable(L, 0, 11);

				FrameScript::PushString(L,
				    info.ref.kind == Placements::Kind::Doodad ? "doodad" : "wmo");
				FrameScript::SetField(L, -2, "kind");
				FrameScript::PushNumber(L, info.ref.tileX);
				FrameScript::SetField(L, -2, "tileX");
				FrameScript::PushNumber(L, info.ref.tileY);
				FrameScript::SetField(L, -2, "tileY");
				FrameScript::PushNumber(L, info.ref.index);
				FrameScript::SetField(L, -2, "index");
				FrameScript::PushNumber(L, info.ref.uniqueId);
				FrameScript::SetField(L, -2, "uid");
				FrameScript::PushString(L, info.name.c_str());
				FrameScript::SetField(L, -2, "name");
				FrameScript::PushNumber(L, info.distance);
				FrameScript::SetField(L, -2, "dist");
				FrameScript::PushNumber(L, info.originDistance);
				FrameScript::SetField(L, -2, "odist");
				FrameScript::PushNumber(L, info.transform.position.x);
				FrameScript::SetField(L, -2, "x");
				FrameScript::PushNumber(L, info.transform.position.y);
				FrameScript::SetField(L, -2, "y");
				FrameScript::PushNumber(L, info.transform.position.z);
				FrameScript::SetField(L, -2, "z");

				FrameScript::RawSetI(L, -2, static_cast<int>(i) + 1);
			}

			return 1;
		}

		// Relative, since nudging by a known amount is what the keyboard is for and the gizmo
		// already covers dragging to a spot.
		int MovePlacement(lua_State* L)
		{
			RuntimeState& state = State();
			if (!RefreshSelection())
			{
				Print("MapEditor: nothing selected");
				return 0;
			}

			state.selected.position.x += FrameScript::IsNumber(L, 1) ? static_cast<float>(FrameScript::GetNumber(L, 1)) : 0.0f;
			state.selected.position.y += FrameScript::IsNumber(L, 2) ? static_cast<float>(FrameScript::GetNumber(L, 2)) : 0.0f;
			state.selected.position.z += FrameScript::IsNumber(L, 3) ? static_cast<float>(FrameScript::GetNumber(L, 3)) : 0.0f;

			CommitSelection();
			PrintSelection();
			return 0;
		}

		// Degrees, relative, in MDDF order. The first argument is yaw because that is the one
		// anybody actually wants to change.
		int RotatePlacement(lua_State* L)
		{
			RuntimeState& state = State();
			if (!RefreshSelection())
			{
				Print("MapEditor: nothing selected");
				return 0;
			}

			state.selected.rotation.y += FrameScript::IsNumber(L, 1) ? static_cast<float>(FrameScript::GetNumber(L, 1)) : 0.0f;
			state.selected.rotation.x += FrameScript::IsNumber(L, 2) ? static_cast<float>(FrameScript::GetNumber(L, 2)) : 0.0f;
			state.selected.rotation.z += FrameScript::IsNumber(L, 3) ? static_cast<float>(FrameScript::GetNumber(L, 3)) : 0.0f;

			CommitSelection();
			PrintSelection();
			return 0;
		}

		int ScalePlacement(lua_State* L)
		{
			RuntimeState& state = State();
			if (!RefreshSelection())
			{
				Print("MapEditor: nothing selected");
				return 0;
			}

			if (state.selection.kind != Placements::Kind::Doodad)
			{
				Print("MapEditor: MODF has no scale, only doodads can be resized");
				return 0;
			}

			if (!FrameScript::IsNumber(L, 1))
			{
				Print("MapEditor: scale %.3f", state.selected.scale);
				return 0;
			}

			state.selected.scale = static_cast<float>(FrameScript::GetNumber(L, 1));
			CommitSelection();
			PrintSelection();
			return 0;
		}

		int DeletePlacement(lua_State*)
		{
			RuntimeState& state = State();
			if (!state.selection.Valid())
			{
				Print("MapEditor: nothing selected");
				return 0;
			}

			Placements::Ref ref = state.selection;
			std::string error;
			if (!Placements::Remove(ref, error))
			{
				Print("MapEditor: %s", error.c_str());
				return 0;
			}

			// Every index above the hole just shifted, so anything still holding a Ref is wrong.
			ClearSelection();
			Print("MapEditor: deleted %s %d from tile %d_%d, save to keep it",
			    ref.kind == Placements::Kind::Doodad ? "doodad" : "wmo", ref.index, ref.tileX, ref.tileY);
			return 0;
		}

		// Moving an entry only has to keep the live instance looking right, but a new one has
		// nowhere to go: CMapArea::doodadDef and mapObjDef are sized when the tile parses and hold
		// no spare row. So the tile goes to disk and gets purged, and the client builds the
		// instance on the way back in.
		bool PublishTile(int32_t tileX, int32_t tileY)
		{
			std::string error;
			if (!Session::SaveOne(tileX, tileY, error))
			{
				Print("MapEditor: %s", error.c_str());
				return false;
			}

			Access::ReloadTile(tileX, tileY);
			return true;
		}

		// Ten seconds is long enough for the tile to come back and the model to stream in, short
		// enough that a path the client cannot open gives up while you still remember placing it.
		constexpr uint32_t kSettleWindowMs = 10000;

		void WatchNewPlacement(Placements::Ref const& ref, Placements::Transform const& transform)
		{
			if (ref.kind != Placements::Kind::Doodad)
				return;

			RuntimeState& state = State();
			state.pending = ref;
			state.pendingTransform = transform;
			state.pendingUntilMs = GetTickCount() + kSettleWindowMs;
		}

		void SelectNew(Placements::Ref const& ref, char const* what)
		{
			ClearSelection();
			State().selection = ref;
			RefreshSelection();

			Print("MapEditor: %s %s %d in tile %d_%d, uid %u", what,
			    ref.kind == Placements::Kind::Doodad ? "doodad" : "wmo", ref.index, ref.tileX, ref.tileY,
			    ref.uniqueId);
		}

		// Drops a model at the cursor. Type the path with forward slashes: Lua turns a single
		// backslash in a literal into an escape, so "World\test.m2" arrives with a tab in it.
		int AddPlacement(lua_State* L)
		{
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no cursor and no character to work from");
				return 0;
			}

			char const* path = FrameScript::ToLString(L, 1, false);
			if (!path || !*path)
			{
				Print("MapEditor: usage - MapEditor_AddPlacement(\"World/path/model.m2\" [, scale])");
				return 0;
			}

			Placements::Kind kind;
			if (!Placements::KindForPath(path, kind))
			{
				Print("MapEditor: %s is neither a model nor a WMO (.m2, .mdx, .mdl, .wmo)", path);
				return 0;
			}

			Placements::Transform transform;
			transform.position = cursor.position;
			transform.scale = FrameScript::IsNumber(L, 2) ? static_cast<float>(FrameScript::GetNumber(L, 2)) : 1.0f;

			Placements::Ref added;
			std::string error;
			if (!Placements::Add(kind, path, transform, added, error))
			{
				Print("MapEditor: %s", error.c_str());
				return 0;
			}

			if (!PublishTile(added.tileX, added.tileY))
				return 0;

			SelectNew(added, "placed");
			WatchNewPlacement(added, transform);
			return 0;
		}

		// The selection again at the cursor. Handy on its own and the only way to place something
		// the client can name but no listfile ever will.
		int ClonePlacement(lua_State*)
		{
			RuntimeState& state = State();
			if (!state.selection.Valid())
			{
				Print("MapEditor: nothing selected");
				return 0;
			}

			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no cursor and no character to work from");
				return 0;
			}

			Placements::Transform transform = state.selected;
			transform.position = cursor.position;

			Placements::Ref added;
			std::string error;
			if (!Placements::Clone(state.selection, transform, added, error))
			{
				Print("MapEditor: %s", error.c_str());
				return 0;
			}

			if (!PublishTile(added.tileX, added.tileY))
				return 0;

			SelectNew(added, "cloned to");
			WatchNewPlacement(added, transform);
			return 0;
		}

		// Distinct model paths standing near the cursor, nearest first. There is no listfile to
		// browse, so what the artists already put around you is the palette.
		int GetNearbyModels(lua_State* L)
		{
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				FrameScript::PushNil(L);
				return 1;
			}

			float radius = FrameScript::IsNumber(L, 1) ? static_cast<float>(FrameScript::GetNumber(L, 1)) : 100.0f;

			std::vector<std::string> models;
			Placements::ListModels(cursor.position, radius, models);

			// Capped because these come back as return values, and a busy city block can turn up
			// hundreds. Nearest first means the cut falls on the ones you were least likely to want.
			size_t show = models.size() < 40 ? models.size() : 40;
			for (size_t i = 0; i < show; ++i)
				FrameScript::PushString(L, models[i].c_str());

			return static_cast<int>(show);
		}

		// Liquid never draws off a live buffer, so every one of these ends in a save and a reload
		// of each tile it reached. That is cheap enough on one tile, which is why they are commands
		// rather than something bound to a drag.
		int32_t PublishLiquid(std::vector<Liquids::TileRef> const& tiles)
		{
			int32_t published = 0;
			for (Liquids::TileRef const& tile : tiles)
			{
				if (PublishTile(tile.x, tile.y))
					++published;
			}

			return published;
		}

		// The whole of LiquidType.dbc as a table, so the addon can put up a real list instead of
		// asking for a row number. Each entry is { id, name, bank, material }.
		int GetLiquidTypes(lua_State* L)
		{
			std::vector<Liquids::TypeInfo> types;
			Liquids::ListTypes(types);

			FrameScript::CreateTable(L, static_cast<int>(types.size()), 0);

			for (size_t i = 0; i < types.size(); ++i)
			{
				FrameScript::CreateTable(L, 0, 4);

				FrameScript::PushNumber(L, types[i].id);
				FrameScript::SetField(L, -2, "id");
				FrameScript::PushString(L, types[i].name.c_str());
				FrameScript::SetField(L, -2, "name");
				FrameScript::PushNumber(L, types[i].soundBank);
				FrameScript::SetField(L, -2, "bank");
				FrameScript::PushNumber(L, types[i].materialId);
				FrameScript::SetField(L, -2, "material");

				FrameScript::RawSetI(L, -2, static_cast<int>(i) + 1);
			}

			return 1;
		}

		int SetLiquidType(lua_State* L)
		{
			if (!FrameScript::IsNumber(L, 1))
				return 0;

			uint32_t id = static_cast<uint32_t>(FrameScript::GetNumber(L, 1));

			Liquids::TypeInfo info;
			if (!Liquids::GetType(id, info))
			{
				Print("MapEditor: no LiquidType.dbc row %u", id);
				return 0;
			}

			State().liquidType = id;
			return 0;
		}

		// How far above the ground a surface goes when no height is given. Zero is legal, it just
		// puts the surface level with whatever you are pointing at, which is usually invisible.
		int SetLiquidOffset(lua_State* L)
		{
			if (!FrameScript::IsNumber(L, 1))
				return 0;

			float offset = static_cast<float>(FrameScript::GetNumber(L, 1));
			if (offset < 0.0f)
				offset = 0.0f;
			if (offset > 500.0f)
				offset = 500.0f;

			State().liquidOffset = offset;
			return 0;
		}

		int GetLiquidType(lua_State* L)
		{
			uint32_t id = State().liquidType;

			Liquids::TypeInfo info;
			bool known = Liquids::GetType(id, info);

			FrameScript::PushNumber(L, id);
			FrameScript::PushString(L, known ? info.name.c_str() : "?");
			FrameScript::PushNumber(L, known ? info.soundBank : 0);
			FrameScript::PushNumber(L, State().liquidOffset);
			return 4;
		}

		// Fills every cell the brush covers with the selected liquid, flat at the given height. No
		// height means the focus point plus the offset, so pointing at the bottom of a hole you
		// just dug fills it rather than laying a surface inside the floor.
		int AddLiquid(lua_State* L)
		{
			RuntimeState& state = State();
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no cursor and no character to work from");
				return 0;
			}

			float height = FrameScript::IsNumber(L, 1)
			    ? static_cast<float>(FrameScript::GetNumber(L, 1))
			    : cursor.position.z + state.liquidOffset;

			std::vector<Liquids::TileRef> touched;
			std::string error;
			int32_t changed = Liquids::Add(cursor.position, state.brushRadius, state.liquidType,
			    height, touched, error);

			if (!changed)
			{
				Print("MapEditor: added no liquid - %s", error.c_str());
				return 0;
			}

			Liquids::TypeInfo info;
			char const* name = Liquids::GetType(state.liquidType, info) ? info.name.c_str() : "?";

			Print("MapEditor: %s at %.1f over %d chunk%s", name, height, changed,
			    changed == 1 ? "" : "s");
			PublishLiquid(touched);
			return 0;
		}

		int RemoveLiquid(lua_State* L)
		{
			RuntimeState& state = State();
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no cursor and no character to work from");
				return 0;
			}

			int32_t layer = FrameScript::IsNumber(L, 1)
			    ? static_cast<int32_t>(FrameScript::GetNumber(L, 1))
			    : -1;

			std::vector<Liquids::TileRef> touched;
			std::string error;
			int32_t changed = Liquids::Remove(cursor.position, state.brushRadius, layer, touched,
			    error);

			if (!changed)
			{
				Print("MapEditor: removed no liquid - %s", error.c_str());
				return 0;
			}

			Print("MapEditor: cleared liquid from %d chunk%s", changed, changed == 1 ? "" : "s");
			PublishLiquid(touched);
			return 0;
		}

		// Repoints whatever liquid is already there at the selected type. Turning a river into lava
		// is this rather than a remove and an add, so the surface heights survive.
		int RetypeLiquid(lua_State* L)
		{
			RuntimeState& state = State();
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no cursor and no character to work from");
				return 0;
			}

			int32_t layer = FrameScript::IsNumber(L, 1)
			    ? static_cast<int32_t>(FrameScript::GetNumber(L, 1))
			    : -1;

			std::vector<Liquids::TileRef> touched;
			std::string error;
			int32_t changed = Liquids::SetType(cursor.position, state.brushRadius, layer,
			    state.liquidType, touched, error);

			if (!changed)
			{
				Print("MapEditor: retyped nothing - %s", error.c_str());
				return 0;
			}

			Liquids::TypeInfo info;
			char const* name = Liquids::GetType(state.liquidType, info) ? info.name.c_str() : "?";

			Print("MapEditor: %d chunk%s now %s", changed, changed == 1 ? " is" : "s are", name);
			PublishLiquid(touched);
			return 0;
		}

		// MapEditor_SetLiquidHeight(z [, layer]) puts the surface at an absolute world z,
		// MapEditor_NudgeLiquidHeight(delta [, layer]) moves it by that much.
		int LiquidHeight(lua_State* L, bool relative)
		{
			RuntimeState& state = State();
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no cursor and no character to work from");
				return 0;
			}

			if (!FrameScript::IsNumber(L, 1))
			{
				Print("MapEditor: usage - MapEditor_%sLiquidHeight(%s [, layer])",
				    relative ? "Nudge" : "Set", relative ? "delta" : "z");
				return 0;
			}

			float height = static_cast<float>(FrameScript::GetNumber(L, 1));
			int32_t layer = FrameScript::IsNumber(L, 2)
			    ? static_cast<int32_t>(FrameScript::GetNumber(L, 2))
			    : -1;

			std::vector<Liquids::TileRef> touched;
			std::string error;
			int32_t changed = Liquids::SetHeight(cursor.position, state.brushRadius, layer,
			    height, relative, touched, error);

			if (!changed)
			{
				Print("MapEditor: moved nothing - %s", error.c_str());
				return 0;
			}

			Print("MapEditor: surface %s %.2f over %d chunk%s", relative ? "moved by" : "set to",
			    height, changed, changed == 1 ? "" : "s");
			PublishLiquid(touched);
			return 0;
		}

		int SetLiquidHeight(lua_State* L)
		{
			return LiquidHeight(L, false);
		}

		int NudgeLiquidHeight(lua_State* L)
		{
			return LiquidHeight(L, true);
		}

		// Fishable is what a bobber can land on, fatigue is the ocean drowning zone and belongs on
		// open water only. Pass nil for either one to leave it alone.
		int SetLiquidFlags(lua_State* L)
		{
			RuntimeState& state = State();
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no cursor and no character to work from");
				return 0;
			}

			int32_t fishable = FrameScript::Type(L, 1) <= 0 ? -1 : (FrameScript::ToBoolean(L, 1) ? 1 : 0);
			int32_t fatigue = FrameScript::Type(L, 2) <= 0 ? -1 : (FrameScript::ToBoolean(L, 2) ? 1 : 0);

			if (fishable < 0 && fatigue < 0)
			{
				Print("MapEditor: usage - MapEditor_SetLiquidFlags(fishable, fatigue), nil to keep");
				return 0;
			}

			std::vector<Liquids::TileRef> touched;
			std::string error;
			int32_t changed = Liquids::SetFlags(cursor.position, state.brushRadius, fishable,
			    fatigue, touched, error);

			if (!changed)
			{
				Print("MapEditor: no flags changed - %s", error.c_str());
				return 0;
			}

			Print("MapEditor: flags on %d chunk%s", changed, changed == 1 ? "" : "s");
			PublishLiquid(touched);
			return 0;
		}

		// The same readout as MapEditor_LiquidInfo, handed back as a table so a panel can draw it.
		// Not something to poll: Describe opens the tile, which parses the ADT the first time.
		int GetChunkLiquid(lua_State* L)
		{
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				FrameScript::PushNil(L);
				return 1;
			}

			Liquids::ChunkInfo info;
			std::string error;
			if (!Liquids::Describe(cursor.tileX, cursor.tileY, cursor.chunkX, cursor.chunkY, info, error))
			{
				FrameScript::PushNil(L);
				return 1;
			}

			FrameScript::CreateTable(L, 0, 3);

			FrameScript::PushNumber(L, Liquids::CountBits(info.fishable));
			FrameScript::SetField(L, -2, "fishable");
			FrameScript::PushNumber(L, Liquids::CountBits(info.fatigue));
			FrameScript::SetField(L, -2, "fatigue");

			FrameScript::CreateTable(L, static_cast<int>(info.layers.size()), 0);

			for (size_t i = 0; i < info.layers.size(); ++i)
			{
				Liquids::LayerInfo const& layer = info.layers[i];

				Liquids::TypeInfo type;
				bool known = Liquids::GetType(layer.type, type);

				FrameScript::CreateTable(L, 0, 6);

				FrameScript::PushNumber(L, layer.type);
				FrameScript::SetField(L, -2, "type");
				FrameScript::PushString(L, known ? type.name.c_str() : "?");
				FrameScript::SetField(L, -2, "name");
				FrameScript::PushNumber(L, layer.format);
				FrameScript::SetField(L, -2, "format");
				FrameScript::PushNumber(L, layer.cells);
				FrameScript::SetField(L, -2, "cells");
				FrameScript::PushNumber(L, layer.minHeight);
				FrameScript::SetField(L, -2, "minHeight");
				FrameScript::PushNumber(L, layer.maxHeight);
				FrameScript::SetField(L, -2, "maxHeight");

				FrameScript::RawSetI(L, -2, static_cast<int>(i) + 1);
			}

			FrameScript::SetField(L, -2, "layers");
			return 1;
		}

		int LiquidInfo(lua_State*)
		{
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				Print("MapEditor: no terrain under the cursor");
				return 0;
			}

			Liquids::ChunkInfo info;
			std::string error;
			if (!Liquids::Describe(cursor.tileX, cursor.tileY, cursor.chunkX, cursor.chunkY, info, error))
			{
				Print("MapEditor: %s", error.c_str());
				return 0;
			}

			Print("MapEditor: chunk %d,%d of tile %d,%d has %u liquid layer%s", cursor.chunkX,
			    cursor.chunkY, cursor.tileX, cursor.tileY, static_cast<uint32_t>(info.layers.size()),
			    info.layers.size() == 1 ? "" : "s");

			for (size_t i = 0; i < info.layers.size(); ++i)
			{
				Liquids::LayerInfo const& layer = info.layers[i];

				Liquids::TypeInfo type;
				char const* name = Liquids::GetType(layer.type, type) ? type.name.c_str() : "?";

				Print("  layer %u  type %u %s  format %u  %d cell%s  z %.2f to %.2f",
				    static_cast<uint32_t>(i), layer.type, name, layer.format, layer.cells,
				    layer.cells == 1 ? "" : "s", layer.minHeight, layer.maxHeight);
			}

			if (!info.layers.empty())
				Print("  fishable cells %d  fatigue cells %d", Liquids::CountBits(info.fishable),
				    Liquids::CountBits(info.fatigue));

			return 0;
		}

		int SetEnabled(lua_State* L)
		{
			State().enabled = FrameScript::ToBoolean(L, 1);
			return 0;
		}

		int IsEnabled(lua_State* L)
		{
			FrameScript::PushBoolean(L, State().enabled ? 1 : 0);
			return 1;
		}

		// Selecting, dragging and adding placements without the editor taking the mouse. A click
		// that lands on nothing is handed back to the game, so the camera, frames and NPCs all
		// still work with this on.
		int SetPlacementMode(lua_State* L)
		{
			State().placeOnly = FrameScript::ToBoolean(L, 1);
			return 0;
		}

		int SetBrushRadius(lua_State* L)
		{
			if (!FrameScript::IsNumber(L, 1))
				return 0;

			float radius = static_cast<float>(FrameScript::GetNumber(L, 1));
			if (radius < 1.0f)
				radius = 1.0f;
			if (radius > 200.0f)
				radius = 200.0f;

			State().brushRadius = radius;
			return 0;
		}

		int GetBrushRadius(lua_State* L)
		{
			FrameScript::PushNumber(L, State().brushRadius);
			return 1;
		}

		int SetShowGrid(lua_State* L)
		{
			State().showGrid = FrameScript::ToBoolean(L, 1);
			return 0;
		}

		// x, y, z, tileX, tileY, chunkX, chunkY, areaId, isRealCursor. The last one is false when
		// this is the character fallback, so a panel can say where it is really pointing.
		int GetCursorInfo(lua_State* L)
		{
			CursorInfo cursor = Focus();
			if (!cursor.valid)
			{
				FrameScript::PushNil(L);
				return 1;
			}

			FrameScript::PushNumber(L, cursor.position.x);
			FrameScript::PushNumber(L, cursor.position.y);
			FrameScript::PushNumber(L, cursor.position.z);
			FrameScript::PushNumber(L, cursor.tileX);
			FrameScript::PushNumber(L, cursor.tileY);
			FrameScript::PushNumber(L, cursor.chunkX);
			FrameScript::PushNumber(L, cursor.chunkY);
			FrameScript::PushNumber(L, cursor.areaId);
			FrameScript::PushBoolean(L, State().cursor.valid ? 1 : 0);
			return 9;
		}

		// Every intermediate the cursor depends on, so a miss can be attributed instead of
		// guessed at.
		int Debug(lua_State*)
		{
			RuntimeState const& state = State();
			CGWorldFrameFull* worldFrame = CGWorldFrameFull::Current();

			Print("MapEditor: placementMode %d", PlaceOnly() ? 1 : 0);
			Print("MapEditor: enabled %d  mapActive %d  worldFrame %p  camera %p", state.enabled ? 1 : 0,
			    Access::IsActive() ? 1 : 0, worldFrame, worldFrame ? worldFrame->m_camera : nullptr);
			Print("  mouse ddc (%.4f %.4f)", state.mouseX, state.mouseY);
			Print("  ray (%.1f %.1f %.1f) -> (%.1f %.1f %.1f) frac %.4f", state.rayStart.x,
			    state.rayStart.y, state.rayStart.z, state.rayEnd.x, state.rayEnd.y, state.rayEnd.z,
			    state.rayFraction);

			if (state.cursor.valid)
			{
				Print("  hit (%.2f %.2f %.2f)  tile %d,%d  chunk %d,%d  area %d", state.cursor.position.x,
				    state.cursor.position.y, state.cursor.position.z, state.cursor.tileX,
				    state.cursor.tileY, state.cursor.chunkX, state.cursor.chunkY, state.cursor.areaId);

				// The axis convention is only proven if the hit lands inside the chunk we resolved
				// for it, and the nearest vertex sits within half a cell of the hit.
				CMapArea* hitArea = Access::GetArea(state.cursor.tileX, state.cursor.tileY);
				CMapChunk* hitChunk = Access::GetChunk(hitArea, state.cursor.chunkX, state.cursor.chunkY);
				if (hitChunk && hitChunk->vertices)
				{
					C3Vector const& corner = hitChunk->topLeftCoords;
					float dx = corner.x - state.cursor.position.x;
					float dy = corner.y - state.cursor.position.y;
					bool inside = dx >= 0.0f && dx <= Coords::kChunkSize && dy >= 0.0f
					    && dy <= Coords::kChunkSize;

					Print("  chunk corner (%.2f %.2f %.2f)  offset (%.2f %.2f)  inside %d", corner.x,
					    corner.y, corner.z, dx, dy, inside ? 1 : 0);

					int32_t best = 0;
					float bestDist = 1e30f;
					for (int32_t i = 0; i < Access::kVertsPerChunk; ++i)
					{
						float d = Coords::Distance2D(Coords::VertexWorldPos(hitChunk, i), state.cursor.position);
						if (d < bestDist)
						{
							bestDist = d;
							best = i;
						}
					}

					C3Vector nearest = Coords::VertexWorldPos(hitChunk, best);
					Print("  nearest vertex %d at (%.2f %.2f %.2f)  dist2d %.2f  dz %.2f", best, nearest.x,
					    nearest.y, nearest.z, bestDist, nearest.z - state.cursor.position.z);
				}
			}
			else
			{
				Print("  no hit: %s", state.lastMiss);
			}

			// Independent of the cursor: does the tile grid look sane where the camera is?
			if (worldFrame && worldFrame->m_camera)
			{
				C3Vector const& camera = worldFrame->m_camera->m_position;
				int32_t tileX = Coords::TileX(camera);
				int32_t tileY = Coords::TileY(camera);
				CMapArea* area = Access::GetArea(tileX, tileY);

				Print("  camera (%.1f %.1f %.1f) -> tile %d,%d  area %p ready %d", camera.x, camera.y,
				    camera.z, tileX, tileY, area, Access::IsAreaReady(area) ? 1 : 0);

				if (area)
					Print("  area index %d,%d (want %d,%d)", area->index.x, area->index.y, tileX, tileY);
			}

			std::vector<std::string> lines;
			Placements::DebugRay(state.rayStart, state.rayEnd, lines);
			for (std::string const& line : lines)
				Print("%s", line.c_str());

			return 0;
		}
	}

	void Apply()
	{
		sLua.RegisterFunction("MapEditor_SetEnabled", &SetEnabled, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_IsEnabled", &IsEnabled, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SetPlacementMode", &SetPlacementMode, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SetBrushRadius", &SetBrushRadius, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SetBrushStrength", &SetBrushStrength, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SetBrushMode", &SetBrushMode, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SetTool", &SetTool, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SetPaintLayer", &SetPaintLayer, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_ListTextures", &ListTextures, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_ListTileTextures", &ListTileTextures, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_AddLayer", &AddLayer, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_RemoveLayer", &RemoveLayer, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SetFalloff", &SetFalloff, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SaveEdits", &SaveEdits, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_RevertEdits", &RevertEdits, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_GetBrushRadius", &GetBrushRadius, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SetShowGrid", &SetShowGrid, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_GetCursorInfo", &GetCursorInfo, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_DumpTile", &DumpTile, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SaveTile", &SaveTile, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_RaiseChunk", &RaiseChunk, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_RestoreTile", &RestoreTile, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_ReloadTile", &ReloadTile, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_RoundTrip", &RoundTrip, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_RoundTripAll", &RoundTripAll, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_RoundTripMap", &RoundTripMap, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_GetState", &GetState, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_GetSelection", &GetSelection, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_GetChunkLayers", &GetChunkLayers, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SelectPlacement", &SelectPlacement, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_PlacementInfo", &PlacementInfo, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_ListPlacements", &ListPlacements, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_GetNearbyPlacements", &GetNearbyPlacements,
		    LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_MovePlacement", &MovePlacement, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_RotatePlacement", &RotatePlacement, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_ScalePlacement", &ScalePlacement, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_DeletePlacement", &DeletePlacement, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_AddPlacement", &AddPlacement, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_ClonePlacement", &ClonePlacement, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_GetNearbyModels", &GetNearbyModels, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_GetLiquidTypes", &GetLiquidTypes, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SetLiquidType", &SetLiquidType, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_GetLiquidType", &GetLiquidType, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SetLiquidOffset", &SetLiquidOffset, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_AddLiquid", &AddLiquid, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_RemoveLiquid", &RemoveLiquid, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_RetypeLiquid", &RetypeLiquid, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SetLiquidHeight", &SetLiquidHeight, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_NudgeLiquidHeight", &NudgeLiquidHeight, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_SetLiquidFlags", &SetLiquidFlags, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_LiquidInfo", &LiquidInfo, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_GetChunkLiquid", &GetChunkLiquid, LuaFunctionState::FRAME);
		sLua.RegisterFunction("MapEditor_Debug", &Debug, LuaFunctionState::FRAME);
	}

	void OnGameClientInitialize()
	{
		// Registered whatever the enabled flag says, since the handlers check it themselves and it
		// can be flipped long after the game comes up. Guarded because InitializeGame can run twice
		// without a destroy in between.
		if (EventsRegistered())
			return;

		EventRegisterEx(EVENT_ID_MOUSEDOWN, OnMouseDown, nullptr, 10.0f);
		EventRegisterEx(EVENT_ID_MOUSEUP, OnMouseUp, nullptr, 10.0f);
		EventsRegistered() = true;
	}

	void OnGameClientDestroy()
	{
		if (EventsRegistered())
		{
			EventUnregister(EVENT_ID_MOUSEDOWN, OnMouseDown);
			EventUnregister(EVENT_ID_MOUSEUP, OnMouseUp);
			EventsRegistered() = false;
		}

		RuntimeState& state = State();
		state.cursor = {};
		state.painting = false;
		state.lastMiss = "game client destroyed";

		// Tile coordinates mean something else on the next map, so nothing open here survives. The
		// owned MCAL and texture name buffers go with them.
		ClearSelection();

		TextureBrush::Reset();
		TextureLayers::Reset();
		Session::OnMapChanged();
	}

	void BeforeWorldRender(CGWorldFrameFull*)
	{
		RuntimeState& state = State();
		if (!state.enabled)
			return;

		// CGWorldFrame::OnLayerUpdate clears the decal mode to 3 every frame before we get here,
		// so this write is the last one standing when the client draws it a moment later.
		if (state.cursor.valid)
			BrushDecal::Show(state.cursor.position, state.brushRadius, true);
		else
			BrushDecal::Hide();
	}

	void AfterWorldRender(CGWorldFrameFull* worldFrame)
	{
		RuntimeState& state = State();

		// Ahead of the mode check, so switching the editor off right after dropping something does
		// not leave its footprint stuck on the guess.
		SettleNewPlacement();

		if (!AnyModeLive())
		{
			state.cursor = {};
			state.painting = false;
			FlushLiquidStroke();
			return;
		}

		UpdateCursor(worldFrame);

		// No brushes in placement mode, and nothing should be able to start a stroke there either.
		if (state.enabled)
		{
			UpdateStroke();
		}
		else
		{
			state.painting = false;
			FlushLiquidStroke();
		}

		UpdatePlacement(worldFrame);

		if (state.showGrid && state.enabled && worldFrame && worldFrame->m_camera)
			DrawChunkGrid(worldFrame);
	}
}

