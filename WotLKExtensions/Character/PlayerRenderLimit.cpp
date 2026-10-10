#include <Defines.h>
#include "PlayerRenderLimit.h"

#include <ClientDetours.h>
#include <ClientData/CharComponent.h>
#include <ClientData/ClientFunctions.h>
#include <ClientData/GameClient.h>
#include <ClientData/M2Model.h>
#include <ClientData/MathTypes.h>
#include <ClientData/Object.h>
#include <ClientData/ObjectFields.h>
#include <ClientData/ObjectManager.h>
#include <ClientData/SharedDefines.h>
#include <ClientData/Social.h>
#include <ClientData/WowObjects.h>
#include <Logger.h>

#include <algorithm>

namespace
{
	constexpr uint32 kEvaluateIntervalMs = 250;
	constexpr float kKeepVisibleBonusYards = 5.0f;
	constexpr float kNameHeightFactor = 1.25f;
	constexpr uint32 kReleaseDelayMs = 3000;
	constexpr size_t kReleasesPerTick = 8;
	constexpr int kMinLimit = 20;
	constexpr int kMaxLimit = 100;
}

PlayerRenderLimit& PlayerRenderLimit::Instance()
{
	static PlayerRenderLimit instance;
	return instance;
}

void PlayerRenderLimit::SetLimit(int maxPlayers)
{
	m_limit = maxPlayers <= 0 ? kMaxLimit : std::clamp(maxPlayers, kMinLimit, kMaxLimit);
	m_nextEvaluateMs = 0;
}

void PlayerRenderLimit::Tick()
{
	if (!m_limit)
		return;
	uint32 now = static_cast<uint32>(OsGetAsyncTimeMs());
	if (m_nextEvaluateMs && static_cast<int32>(now - m_nextEvaluateMs) < 0)
		return;
	m_nextEvaluateMs = now + kEvaluateIntervalMs;
	Evaluate(now);
}

void PlayerRenderLimit::Reset()
{
	m_hidden.clear();
	m_nextHidden.clear();
	m_relations.clear();
	m_candidates.clear();
	m_nextEvaluateMs = 0;
	m_released = 0;
}

bool PlayerRenderLimit::IsHiddenUnit(const void* unit) const
{
	if (m_hidden.empty() || !unit)
		return false;
	const auto* object = static_cast<const ClientData::CGObject_C*>(unit);
	if (object->GetTypeID() != ClientData::TYPEID_PLAYER)
		return false;
	return m_hidden.contains(object->GetGuid());
}

void PlayerRenderLimit::Evaluate(uint32 now)
{
	if (!ClientData::GameClient::IsInitialized())
		return;
	ClientData::WowPlayer self = ClientData::WowPlayer::Active();
	uint64 selfGuid = ClntObjMgr::GetActivePlayer();
	if (!self || !selfGuid)
	{
		m_hidden.clear();
		m_released = 0;
		return;
	}
	m_scan.self = self.Get();
	m_scan.selfGuid = selfGuid;
	m_scan.targetGuid = self.TargetGuid();
	m_scan.guildId = self.GuildId();

	CollectRelations();
	m_candidates.clear();
	ClntObjMgr::EnumVisibleObjects(CollectCallback, this);

	m_nextHidden.clear();
	if (m_candidates.size() > static_cast<size_t>(m_limit))
	{
		std::sort(m_candidates.begin(), m_candidates.end(), [](const Candidate& a, const Candidate& b)
		{
			if (a.relation != b.relation)
				return a.relation < b.relation;
			return a.distance < b.distance;
		});
		for (size_t i = static_cast<size_t>(m_limit); i < m_candidates.size(); ++i)
		{
			const uint64 guid = m_candidates[i].guid;
			HiddenState state;
			if (auto it = m_hidden.find(guid); it != m_hidden.end())
				state = it->second;
			else
				state.hiddenSinceMs = now;
			m_nextHidden.insert_or_assign(guid, state);
		}
	}
	if (m_nextHidden.size() != m_hidden.size())
		LOG_DEBUG << "player render limit: hiding " << m_nextHidden.size() << " of " << m_candidates.size() << " players, limit " << m_limit;
	std::swap(m_hidden, m_nextHidden);
	ReleaseStale(now);
}

void PlayerRenderLimit::ReleaseStale(uint32 now)
{
	size_t released = 0;
	size_t freed = 0;
	for (auto& entry : m_hidden)
	{
		if (entry.second.released)
		{
			++released;
			continue;
		}
		if (freed >= kReleasesPerTick)
			continue;
		if (static_cast<int32>(now - entry.second.hiddenSinceMs) < static_cast<int32>(kReleaseDelayMs))
			continue;
		ClientData::WowPlayer unit = ClientData::WowPlayer::FromGuid(entry.first);
		if (!unit)
			continue;
		if (!ReleaseUnit(unit.Raw()))
			continue;
		entry.second.released = true;
		++released;
		++freed;
	}
	m_released = released;
	if (freed)
		LOG_DEBUG << "player render limit: released " << freed << " player sheets, " << released << " of " << m_hidden.size() << " hidden now released";
}

bool PlayerRenderLimit::ReleaseUnit(void* unit)
{
	void* component = ClientData::WowUnit(unit).CharComponent();
	if (!component)
		return false;
	void* model = ClientData::CharComponent::Model(component);
	if (ClientData::M2Model::IsLoaded(model))
		ClientData::M2Model::ReplaceTexture(model, ClientData::M2Model::kBaseSkinTextureSlot, nullptr);
	CGUnit_C::FreeComponent(unit);
	return true;
}

bool PlayerRenderLimit::ShouldSuppressComponent(const void* unit) const
{
	return IsHiddenUnit(unit);
}

void PlayerRenderLimit::SuppressFrame(void* unit) const
{
	using namespace ClientData;
	void* model = CGUnit_C::GetObjectModel(unit);
	if (!model)
		return;
	uint32& flags = *reinterpret_cast<uint32*>(static_cast<uint8*>(model) + M2Model::kFlagsOffset);
	if (M2Model::AttachParent(model))
	{
		flags &= ~(M2Model::kFlagDrawChild | M2Model::kFlagEventsChild);
		return;
	}
	flags &= ~(M2Model::kFlagDrawRoot | M2Model::kFlagEventsRoot);
	M2Model::SetAnimating(model, 0);
}

void PlayerRenderLimit::CollectRelations()
{
	using namespace ClientData;
	m_relations.clear();
	for (uint32 i = 0; i < Party::kSlots; ++i)
	{
		if (uint64 guid = Party::s_memberGuids[i])
			m_relations.try_emplace(guid, Relation::Party);
	}
	uint32 raidCount = Raid::Count();
	for (uint32 i = 0; i < raidCount; ++i)
	{
		if (uint64 guid = Raid::MemberGuid(i))
			m_relations.try_emplace(guid, Relation::Raid);
	}
	if (const uint8* friends = Friends::List())
	{
		for (uint32 i = 0; i < Friends::kMaxFriends; ++i)
		{
			uint64 guid = Friends::FriendGuid(friends, i);
			if (!guid)
				break;
			m_relations.try_emplace(guid, Relation::Friend);
		}
	}
}

int __cdecl PlayerRenderLimit::CollectCallback(uint32 guidLow, uint32 guidHigh, void* param)
{
	auto* self = static_cast<PlayerRenderLimit*>(param);
	uint64 guid = (static_cast<uint64>(guidHigh) << 32) | guidLow;
	if (guid == self->m_scan.selfGuid)
		return 1;
	ClientData::WowPlayer unit = ClientData::WowPlayer::FromGuid(guid);
	if (!unit)
		return 1;

	Relation relation = Relation::Stranger;
	if (guid == self->m_scan.targetGuid)
		relation = Relation::Target;
	else if (auto it = self->m_relations.find(guid); it != self->m_relations.end())
		relation = it->second;
	else if (self->m_scan.guildId && unit.GuildId() == self->m_scan.guildId)
		relation = Relation::Guild;

	float distance = unit.DistanceTo(ClientData::WowObject(self->m_scan.self));
	if (!self->m_hidden.contains(guid))
		distance -= kKeepVisibleBonusYards;
	self->m_candidates.push_back({ guid, relation, distance });
	return 1;
}

CLIENT_DETOUR_THISCALL(CGWorldFrame__UpdateObject_RenderLimit, 0x004F8D10, void, (void* object, int cullFlags))
{
	if (!sPlayerRenderLimit.IsHiddenUnit(object))
	{
		CGWorldFrame__UpdateObject_RenderLimit(self, object, cullFlags);
		return;
	}
	sPlayerRenderLimit.SuppressFrame(object);
}

CLIENT_DETOUR_THISCALL(CGPlayer_C__ShouldRender_RenderLimit, 0x006E0840, void, (int cullFlags, int* outNotVisible, int* outDontRender))
{
	CGPlayer_C__ShouldRender_RenderLimit(self, cullFlags, outNotVisible, outDontRender);
	if (!sPlayerRenderLimit.IsHiddenUnit(self))
		return;
	*outDontRender = 1;
	void* model = CGUnit_C::GetObjectModel(self);
	if (model && !ClientData::M2Model::AttachParent(model))
		ClientData::M2Model::SetAnimating(model, 0);
}

CLIENT_DETOUR_THISCALL(CGUnit_C__GetNamePosition_RenderLimit, 0x0071FEF0, ClientData::C3Vector*, (ClientData::C3Vector * out))
{
	ClientData::WowUnit unit(self);
	const bool hasModel = unit.BodyModel() != nullptr;
	if (hasModel && !sPlayerRenderLimit.IsHiddenUnit(self))
		return CGUnit_C__GetNamePosition_RenderLimit(self, out);
	*out = unit.Position();
	float height = unit.ObjectHeight();
	float scale = unit.RenderScale();
	out->z += height * scale * kNameHeightFactor;
	return out;
}
