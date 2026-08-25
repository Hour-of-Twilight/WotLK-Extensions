#include <Editor/FreeCam.h>

#include <ClientData/Bindings.h>
#include <ClientData/Camera.h>
#include <ClientData/ClientFunctions.h>
#include <ClientData/Event.h>
#include <ClientData/M2Model.h>
#include <ClientData/MathTypes.h>
#include <ClientData/Object.h>
#include <ClientData/ObjectManager.h>
#include <ClientData/VectorMath.h>
#include <ClientData/WorldFrame.h>
#include <ClientDetours.h>
#include <CustomLua.h>
#include <SharedDefines.h>

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>

#include <Windows.h>

namespace FreeCam
{
	using namespace ClientData;

	namespace
	{
		// Client KEY codes. Printable keys are ASCII, the numpad block starts at 0x100.
		constexpr uint32_t kKeySpace = 32;
		constexpr uint32_t kKeyMinus = 45;
		constexpr uint32_t kKeyEquals = 61;
		constexpr uint32_t kKeyA = 65;
		constexpr uint32_t kKeyD = 68;
		constexpr uint32_t kKeyE = 69;
		constexpr uint32_t kKeyQ = 81;
		constexpr uint32_t kKeyS = 83;
		constexpr uint32_t kKeyW = 87;
		constexpr uint32_t kKeyX = 88;
		constexpr uint32_t kKeyNumpadPlus = 266;
		constexpr uint32_t kKeyNumpadMinus = 267;

		struct EventDataKey
		{
			uint32_t key;
			uint32_t metaKeyState;
			uint32_t repeat;
			uint32_t time;
		};

		constexpr uint32_t kMsgMoveHeartbeat = 0xEE;
		constexpr uint32_t kHeartbeatIntervalMs = 250;

		// CMovement_C hanging off the player, same block MovementForce writes.
		constexpr uint32_t kMovementOffset = 0x788;
		constexpr uint32_t kPosOffset = 0x10;
		constexpr uint32_t kAnchorOffset = 0x4C;
		constexpr uint32_t kFlagsOffset = 0x44;
		constexpr uint32_t kFlagDisableGravity = 0x400;

		constexpr float kMinSpeed = 1.0f;
		constexpr float kMaxSpeed = 4000.0f;
		constexpr float kSpeedStep = 1.25f;

		struct FreeCamState
		{
			bool active = false;
			bool hideCharacter = true;
			bool syncCharacter = true;

			float speed = 40.0f;  // yards a second with no modifier held
			float maxLag = 60.0f; // how far the character may trail before it gets pulled in

			C3Vector position{};

			// Held keys come from the events rather than GetAsyncKeyState, so losing window
			// focus mid-press cannot leave one stuck down.
			bool forward = false;
			bool back = false;
			bool left = false;
			bool right = false;
			bool up = false;
			bool down = false;

			bool haveLastTick = false;
			LARGE_INTEGER lastTick{};

			bool characterHidden = false;
			bool gravityForced = false;
			bool gravityWasSet = false;
			uint32_t lastHeartbeatMs = 0;

			bool eventsRegistered = false;
		};

		FreeCamState& State()
		{
			static FreeCamState state;
			return state;
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

		bool* SlotForKey(uint32_t key)
		{
			FreeCamState& state = State();
			switch (key)
			{
				case kKeyW:
					return &state.forward;
				case kKeyS:
					return &state.back;
				case kKeyA:
					return &state.left;
				case kKeyD:
					return &state.right;
				case kKeySpace:
				case kKeyE:
					return &state.up;
				case kKeyX:
				case kKeyQ:
					return &state.down;
				default:
					return nullptr;
			}
		}

		void ClearKeys()
		{
			FreeCamState& state = State();
			state.forward = false;
			state.back = false;
			state.left = false;
			state.right = false;
			state.up = false;
			state.down = false;
		}

		void SetSpeed(float speed, bool announce)
		{
			if (speed < kMinSpeed)
				speed = kMinSpeed;
			if (speed > kMaxSpeed)
				speed = kMaxSpeed;

			State().speed = speed;
			if (announce)
				Print("FreeCam: %.0f yards a second", speed);
		}

		// Speed steps by a factor rather than a fixed amount, so the same key is usable at 5
		// yards a second and at 500. Held repeats change it silently to keep chat quiet.
		bool HandleSpeedKey(uint32_t key, bool announce)
		{
			if (key == kKeyEquals || key == kKeyNumpadPlus)
			{
				SetSpeed(State().speed * kSpeedStep, announce);
				return true;
			}

			if (key == kKeyMinus || key == kKeyNumpadMinus)
			{
				SetSpeed(State().speed / kSpeedStep, announce);
				return true;
			}

			return false;
		}

		int32_t OnKeyDown(const void* data, void*)
		{
			FreeCamState& state = State();
			if (!state.active || !data || Bindings::KeyboardIsCaptured())
				return 1;

			EventDataKey const* key = static_cast<EventDataKey const*>(data);
			if (HandleSpeedKey(key->key, true))
				return 0;

			bool* slot = SlotForKey(key->key);
			if (!slot)
				return 1;

			// Eating the down is what keeps the game's own movement bindings from firing, so
			// the character does not run off while the camera flies.
			*slot = true;
			return 0;
		}

		int32_t OnKeyUp(const void* data, void*)
		{
			FreeCamState& state = State();
			if (!state.active || !data)
				return 1;

			EventDataKey const* key = static_cast<EventDataKey const*>(data);
			bool* slot = SlotForKey(key->key);

			// Only the release of a press we took is ours. Anything else still belongs to the
			// game, which is waiting for it to stop moving.
			if (!slot || !*slot)
				return 1;

			*slot = false;
			return 0;
		}

		int32_t OnKeyRepeat(const void* data, void*)
		{
			FreeCamState& state = State();
			if (!state.active || !data || Bindings::KeyboardIsCaptured())
				return 1;

			EventDataKey const* key = static_cast<EventDataKey const*>(data);
			if (HandleSpeedKey(key->key, false))
				return 0;

			bool* slot = SlotForKey(key->key);
			return slot && *slot ? 0 : 1;
		}

		float StepSeconds()
		{
			FreeCamState& state = State();

			static LARGE_INTEGER frequency = {};
			if (frequency.QuadPart == 0)
				QueryPerformanceFrequency(&frequency);

			LARGE_INTEGER now;
			QueryPerformanceCounter(&now);

			if (!state.haveLastTick)
			{
				state.lastTick = now;
				state.haveLastTick = true;
				return 0.0f;
			}

			float seconds = float(now.QuadPart - state.lastTick.QuadPart) / float(frequency.QuadPart);
			state.lastTick = now;

			if (seconds < 0.0f)
				return 0.0f;
			if (seconds > 0.1f)
				seconds = 0.1f; // a stall should not fling the camera across the map

			return seconds;
		}

		void ApplyCharacterVisibility()
		{
			FreeCamState& state = State();
			CGObject_C* player = ObjectManager::GetActivePlayerObject();
			if (!player)
				return;

			if (!state.active || !state.hideCharacter)
			{
				if (!state.characterHidden)
					return;

				state.characterHidden = false;

				// Rebuilding the display is the only clean way back. Turning every geoset on
				// would also show the ones the game deliberately keeps off, like the hair that
				// lives under a helm.
				player->UpdateDisplayInfo(1);
				return;
			}

			void* model = player->GetObjectModel();
			if (!model)
				return;

			if (!M2Model::IsLoaded(model))
				return;

			// Re-applied every frame so a model rebuild from swapping gear does not put the
			// character back on screen.
			M2Model::SetGeometryVisible(model, 0, 0xFFFFFFFF, 0);
			state.characterHidden = true;
		}

		// The world streams and culls around the player, not around the camera, so with the
		// character left behind you fly into tiles that never load. Dragging it along is what
		// keeps terrain appearing.
		void SyncCharacter(C3Vector const& target)
		{
			FreeCamState& state = State();
			CGObject_C* player = ObjectManager::GetActivePlayerObject();
			if (!player)
				return;

			uint8_t* movement = reinterpret_cast<uint8_t*>(player) + kMovementOffset;
			float* position = reinterpret_cast<float*>(movement + kPosOffset);
			float* anchor = reinterpret_cast<float*>(movement + kAnchorOffset);
			uint32_t* flags = reinterpret_cast<uint32_t*>(movement + kFlagsOffset);

			// Left to fall, the character drops the moment it is dropped in mid air and fights
			// the sync into a yo-yo. The flag goes back to whatever it was on the way out.
			if (!state.gravityForced)
			{
				state.gravityWasSet = (*flags & kFlagDisableGravity) != 0;
				state.gravityForced = true;
			}
			*flags |= kFlagDisableGravity;

			float dx = target.x - position[0];
			float dy = target.y - position[1];
			float dz = target.z - position[2];
			float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

			if (distance > state.maxLag)
			{
				// Pulled most of the way in rather than right onto the camera, so it is not
				// being re-snapped on every single frame while you fly.
				float keep = state.maxLag * 0.5f;
				float amount = (distance - keep) / distance;

				float mx = dx * amount;
				float my = dy * amount;
				float mz = dz * amount;

				position[0] += mx;
				position[1] += my;
				position[2] += mz;
				anchor[0] += mx;
				anchor[1] += my;
				anchor[2] += mz;
			}

			uint32_t nowMs = uint32_t(OsGetAsyncTimeMs());
			if (nowMs - state.lastHeartbeatMs >= kHeartbeatIntervalMs)
			{
				state.lastHeartbeatMs = nowMs;
				CGUnit_C::SendMovementUpdate(player, nowMs, kMsgMoveHeartbeat, 0.0f, 0, 0, 0, 0xFF);
			}
		}

		void ReleaseCharacter()
		{
			FreeCamState& state = State();
			if (!state.gravityForced)
				return;

			state.gravityForced = false;

			CGObject_C* player = ObjectManager::GetActivePlayerObject();
			if (!player || state.gravityWasSet)
				return;

			uint32_t* flags =
			    reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(player) + kMovementOffset + kFlagsOffset);
			*flags &= ~kFlagDisableGravity;
		}

		void Fly(CGCamera* camera, float seconds)
		{
			FreeCamState& state = State();

			C3Vector forward{};
			camera->Forward(forward);
			forward = VectorMath::Normalize(forward);

			// Strafe off world up rather than the camera's own right vector, so looking up or
			// down does not tilt the sideways step. Straight up or down makes that degenerate,
			// and only there is the camera's right worth falling back on.
			C3Vector const worldUp{ 0.0f, 0.0f, 1.0f };
			C3Vector right = VectorMath::Cross(forward, worldUp);
			if (VectorMath::Length(right) < 0.001f)
				camera->Right(right);
			right = VectorMath::Normalize(right);

			C3Vector move{};
			if (state.forward)
				move = VectorMath::Add(move, forward);
			if (state.back)
				move = VectorMath::Subtract(move, forward);
			if (state.right)
				move = VectorMath::Add(move, right);
			if (state.left)
				move = VectorMath::Subtract(move, right);
			if (state.up)
				move.z += 1.0f;
			if (state.down)
				move.z -= 1.0f;

			float length = VectorMath::Length(move);
			if (length > 0.0f && seconds > 0.0f)
			{
				float speed = state.speed;
				if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
					speed *= 4.0f;
				if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
					speed *= 0.25f;

				state.position = VectorMath::Add(state.position, VectorMath::Scale(move, speed * seconds / length));
			}
		}

		void Tick(CGCamera* camera)
		{
			FreeCamState& state = State();

			ApplyCharacterVisibility();

			if (!state.active)
			{
				state.haveLastTick = false;
				return;
			}

			Fly(camera, StepSeconds());

			// Written after the client has had its say, so the camera-versus-terrain collision
			// it just did is thrown away. That is the whole point of the mode.
			camera->m_position = state.position;

			if (state.syncCharacter)
				SyncCharacter(state.position);
		}

		bool PlayerPosition(C3Vector& out)
		{
			CGObject_C* player = ObjectManager::GetActivePlayerObject();
			if (!player)
				return false;

			player->GetPosition(out);
			return true;
		}

		void Activate()
		{
			FreeCamState& state = State();
			if (state.active)
				return;

			CGWorldFrameFull* worldFrame = CGWorldFrameFull::Current();
			if (!worldFrame || !worldFrame->m_camera)
			{
				Print("FreeCam: no world camera to take over");
				return;
			}

			// Starting where the camera already is means the view does not jump on the way in.
			state.position = worldFrame->m_camera->m_position;
			state.haveLastTick = false;
			ClearKeys();
			state.active = true;

			Print("FreeCam: on at %.0f yards a second. W A S D to fly, space and X for up and down, "
			      "shift for fast, ctrl for slow, - and = for speed. Mouse look is unchanged.",
			    state.speed);
		}

		void Deactivate()
		{
			FreeCamState& state = State();
			if (!state.active)
				return;

			state.active = false;
			ClearKeys();
			state.haveLastTick = false;

			ReleaseCharacter();
			ApplyCharacterVisibility();

			Print("FreeCam: off");
		}

		int SetEnabled(lua_State* L)
		{
			if (FrameScript::ToBoolean(L, 1))
				Activate();
			else
				Deactivate();

			FrameScript::PushBoolean(L, State().active ? 1 : 0);
			return 1;
		}

		int Toggle(lua_State* L)
		{
			if (State().active)
				Deactivate();
			else
				Activate();

			FrameScript::PushBoolean(L, State().active ? 1 : 0);
			return 1;
		}

		int IsEnabled(lua_State* L)
		{
			FrameScript::PushBoolean(L, State().active ? 1 : 0);
			return 1;
		}

		int SetSpeedLua(lua_State* L)
		{
			if (FrameScript::IsNumber(L, 1))
				SetSpeed(float(FrameScript::GetNumber(L, 1)), false);

			FrameScript::PushNumber(L, State().speed);
			return 1;
		}

		int AdjustSpeed(lua_State* L)
		{
			float factor = FrameScript::IsNumber(L, 1) ? float(FrameScript::GetNumber(L, 1)) : kSpeedStep;
			if (factor > 0.0f)
				SetSpeed(State().speed * factor, false);

			FrameScript::PushNumber(L, State().speed);
			return 1;
		}

		int SetHideCharacter(lua_State* L)
		{
			State().hideCharacter = FrameScript::ToBoolean(L, 1);
			ApplyCharacterVisibility();
			return 0;
		}

		int SetSyncCharacter(lua_State* L)
		{
			FreeCamState& state = State();
			state.syncCharacter = FrameScript::ToBoolean(L, 1);

			if (!state.syncCharacter)
			{
				ReleaseCharacter();
				Print("FreeCam: character sync off. Tiles away from where you left it will not stream in.");
			}

			return 0;
		}

		int SetMaxLag(lua_State* L)
		{
			if (FrameScript::IsNumber(L, 1))
			{
				float lag = float(FrameScript::GetNumber(L, 1));
				if (lag < 5.0f)
					lag = 5.0f;
				if (lag > 500.0f)
					lag = 500.0f;
				State().maxLag = lag;
			}

			FrameScript::PushNumber(L, State().maxLag);
			return 1;
		}

		// Drops the camera back on the character, which is the way out of having flown somewhere
		// the character could not follow.
		int Recenter(lua_State* L)
		{
			FreeCamState& state = State();

			C3Vector position{};
			if (!state.active || !PlayerPosition(position))
			{
				FrameScript::PushBoolean(L, 0);
				return 1;
			}

			state.position = position;
			state.position.z += 2.0f;
			state.haveLastTick = false;

			FrameScript::PushBoolean(L, 1);
			return 1;
		}

		int GetState(lua_State* L)
		{
			FreeCamState const& state = State();

			FrameScript::PushBoolean(L, state.active ? 1 : 0);
			FrameScript::PushNumber(L, state.speed);
			FrameScript::PushBoolean(L, state.hideCharacter ? 1 : 0);
			FrameScript::PushBoolean(L, state.syncCharacter ? 1 : 0);
			FrameScript::PushNumber(L, state.maxLag);
			FrameScript::PushNumber(L, state.position.x);
			FrameScript::PushNumber(L, state.position.y);
			FrameScript::PushNumber(L, state.position.z);
			return 8;
		}

		int __cdecl CGCamera_UpdateCallback_FreeCamDetour(void* param, CGCamera* camera)
		{
			int result = CGCamera_UpdateCallback(param, camera);

			// The same callback drives every camera in the client, including the one spinning
			// the model in the character panel, so only the world's own is ours to move.
			CGWorldFrameFull* worldFrame = CGWorldFrameFull::Current();
			if (camera && worldFrame && camera == worldFrame->m_camera)
				Tick(camera);

			return result;
		}

		int CGCamera_UpdateCallback_FreeCamResult =
		    ClientDetours::Add("FreeCam::CGCamera_UpdateCallback", &CGCamera_UpdateCallback,
		        CGCamera_UpdateCallback_FreeCamDetour, __FILE__, __LINE__);
	}

	void Apply()
	{
		sLua.RegisterFunction("FreeCam_SetEnabled", &SetEnabled, LuaFunctionState::FRAME);
		sLua.RegisterFunction("FreeCam_Toggle", &Toggle, LuaFunctionState::FRAME);
		sLua.RegisterFunction("FreeCam_IsEnabled", &IsEnabled, LuaFunctionState::FRAME);
		sLua.RegisterFunction("FreeCam_SetSpeed", &SetSpeedLua, LuaFunctionState::FRAME);
		sLua.RegisterFunction("FreeCam_AdjustSpeed", &AdjustSpeed, LuaFunctionState::FRAME);
		sLua.RegisterFunction("FreeCam_SetHideCharacter", &SetHideCharacter, LuaFunctionState::FRAME);
		sLua.RegisterFunction("FreeCam_SetSyncCharacter", &SetSyncCharacter, LuaFunctionState::FRAME);
		sLua.RegisterFunction("FreeCam_SetMaxLag", &SetMaxLag, LuaFunctionState::FRAME);
		sLua.RegisterFunction("FreeCam_Recenter", &Recenter, LuaFunctionState::FRAME);
		sLua.RegisterFunction("FreeCam_GetState", &GetState, LuaFunctionState::FRAME);
	}

	void OnGameClientInitialize()
	{
		FreeCamState& state = State();

		// Registered whether or not the mode is on, since the handlers check that themselves and
		// it can be switched on long after the game is up. Guarded because InitializeGame can run
		// again without a destroy in between.
		if (state.eventsRegistered)
			return;

		EventRegisterEx(EVENT_ID_KEYDOWN, OnKeyDown, nullptr, 10.0f);
		EventRegisterEx(EVENT_ID_KEYUP, OnKeyUp, nullptr, 10.0f);
		EventRegisterEx(EVENT_ID_KEYDOWN_REPEATING, OnKeyRepeat, nullptr, 10.0f);
		state.eventsRegistered = true;
	}

	void OnGameClientDestroy()
	{
		FreeCamState& state = State();

		if (state.eventsRegistered)
		{
			EventUnregister(EVENT_ID_KEYDOWN, OnKeyDown);
			EventUnregister(EVENT_ID_KEYUP, OnKeyUp);
			EventUnregister(EVENT_ID_KEYDOWN_REPEATING, OnKeyRepeat);
			state.eventsRegistered = false;
		}

		// The player object is on its way out, so the hide and the gravity flag go with it and
		// there is nothing left to restore.
		state.active = false;
		state.characterHidden = false;
		state.gravityForced = false;
		state.haveLastTick = false;
		ClearKeys();
	}
}
