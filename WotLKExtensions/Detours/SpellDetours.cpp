#include "ClientDetours.h"
#include <SharedDefines.h>
#include <Spell.h>
#include <Player.h>
#include "../CDBCMgr/CDBCDefs/SpellClassMaskExtension.h"

CLIENT_DETOUR(Spell_C_GetSpellModifiers, 0x007FD970, __cdecl, bool,
    (SpellRow * pSpellRec, uint32_t modOp, int32_t* outFlatMod, int32_t* outPctMod))
{
	if (pSpellRec->m_attributesExC & 0x20000000)
	{
		*outFlatMod = 0;
		*outPctMod = 100;
		return false;
	}

	int32_t flat = 0;
	int32_t pct = 0;
	sPlayer.GetSpellModifiers(pSpellRec, (SpellModOp)modOp, flat, pct);
	*outFlatMod = flat;
	*outPctMod = pct;

	if (*outFlatMod == 0 && *outPctMod == 0)
		return false;

	*outPctMod += 100;
	if (*outPctMod < 0)
		*outPctMod = 0;

	return true;
}

CLIENT_DETOUR(CastSpell, 0x00540310, __cdecl, int, (lua_State * L))
{
	if (!SStrCmpI(FrameScript::ToLString(L, 2, 0), "cursor", 6))
	{
		FrameScript::SetTop(L, -2);
		Spells::s_castAtCursor = true;
	}
	else if (!SStrCmpI(FrameScript::ToLString(L, 2, 0), "self", 4))
	{
		FrameScript::SetTop(L, -2);
		CastSpell(L);

		CGPlayer* activeObjectPtr = ClntObjMgr::GetActivePlayerObj();
		if (activeObjectPtr)
		{
			C3Vector position = activeObjectPtr->unitBase.movementInfo->position;

			TerrainClickEvent terrainClickEvent = {};
			terrainClickEvent.GUID = 0;
			terrainClickEvent.x = position.x;
			terrainClickEvent.y = position.y;
			terrainClickEvent.z = position.z;
			terrainClickEvent.button = 1;
			TerrainClick(&terrainClickEvent);
		}

		return 0;
	}
	return CastSpell(L);
}

CLIENT_DETOUR_THISCALL(OnLayerTrackTerrain, 0x004F66C0, int, (WorldHitTest * a2))
{
	if (Spells::s_castAtCursor)
	{
		Spells::s_castAtCursor = false;
		TerrainClickEvent terrainClickEvent = {};
		terrainClickEvent.GUID = 0;
		terrainClickEvent.x = a2->hitpoint.x;
		terrainClickEvent.y = a2->hitpoint.y;
		terrainClickEvent.z = a2->hitpoint.z;
		terrainClickEvent.button = 1;
		TerrainClick(&terrainClickEvent);
		return 0;
	}
	return OnLayerTrackTerrain(self, a2);
}

C3Vector GetPointAtDistance(const C3Vector& start, const C3Vector& end, float distance)
{
	C3Vector dir;
	dir.x = end.x - start.x;
	dir.y = end.y - start.y;
	dir.z = end.z - start.z;

	float length = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

	if (length == 0.0f)
	{
		return start;
	}

	dir.x /= length;
	dir.y /= length;
	dir.z /= length;

	C3Vector point;
	point.x = start.x + dir.x * distance;
	point.y = start.y + dir.y * distance;
	point.z = start.z + dir.z * distance;

	return point;
}

float Dot(const C3Vector& a, const C3Vector& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

C3Vector Normalize(const C3Vector& v)
{
	float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
	if (lenSq > 0.0f)
	{
		float invLen = 1.0f / sqrtf(lenSq);
		return { v.x * invLen, v.y * invLen, v.z * invLen };
	}
	return { 0.0f, 0.0f, 0.0f };
}

CLIENT_DETOUR_THISCALL(GetLineSegment, 0x004F6450, int, (float a2, float a3, C3Vector* a4, C3Vector* a5))
{
	int ret = GetLineSegment(self, a2, a3, a4, a5);

	if (Spell_C::IsTargeting() && Spell_C::CanTargetTerrain())
	{
		CGPlayer* activeObjectPtr = ClntObjMgr::GetActivePlayerObj();
		if (activeObjectPtr)
		{
			PendingSpellCast* targetingSpell = *reinterpret_cast<PendingSpellCast**>(0x00D3F4E4);
			if (targetingSpell)
			{
				auto data = &targetingSpell->data;
				float minSpellDist = 0.0f, maxSpellDist = 0.0f;
				Spell_C::GetSpellRange(activeObjectPtr, data->spellId, &minSpellDist, &maxSpellDist, 0);

				float left = 0.0f, right = maxSpellDist * 3;
				float bestDist = -1.0f;
				C3Vector bestPoint, bestPos;

				C3Vector point, pos, hitpoint;
				float distance = 1.0f;
				C3Vector playerPos = activeObjectPtr->unitBase.movementInfo->position;

				bool shouldCheck = true;

				if (TraceLine(a4, a5, &hitpoint, &distance, 0x10111, 0))
				{
					WorldHitTest test = { 0, hitpoint, distance, *a4, *a5 };
					OnLayerTrackTerrain(self, &test);
					shouldCheck = *(int*)0x00AC79A4 != 0; // s_spellShadowStyle - 0 Is "Success"
				}

				if (!shouldCheck)
					return ret;

				C3Vector start = *a4;
				C3Vector end = *a5;
				C3Vector dir = Normalize({ end.x - start.x, end.y - start.y, end.z - start.z });
				C3Vector camToPlayer = { playerPos.x - start.x, playerPos.y - start.y, playerPos.z - start.z };
				float t = Dot(camToPlayer, dir);

				if (t > 0.0f)
				{
					start = { start.x + dir.x * t, start.y + dir.y * t, start.z + dir.z * t };
				}

				float minDist = maxSpellDist * 1; // Spells::g_spell_min_clip_distance_percentage_cvar->m_numberValue;
				while (right - left > 0.5f)
				{
					float mid = (left + right) * 0.5f;
					point = GetPointAtDistance(start, end, mid);
					pos = { point.x, point.y, point.z - 500.0f };
					distance = 1.0f;

					if (TraceLine(&point, &pos, &hitpoint, &distance, 0x10111, 0))
					{
						float dist = CGUnit_C::GetDistanceToPos((CGUnit*)activeObjectPtr, &hitpoint);

						if (dist < minDist * minDist)
						{
							left = mid;
							continue;
						}

						WorldHitTest test{};
						test.distance = distance;
						test.start = point;
						test.end = pos;
						test.hitpoint = hitpoint;
						OnLayerTrackTerrain(self, &test);

						if (*(int*)0x00AC79A4 == 0) // s_spellShadowStyle - 0 Is "Success"
						{
							bestDist = dist;
							bestPoint = point;
							bestPos = pos;
							left = mid;
						}
						else
						{
							right = mid;
						}
					}
					else
					{
						right = mid;
					}
				}

				if (bestDist > minDist * minDist)
				{
					*a4 = bestPoint;
					*a5 = bestPos;
				}
			}
		}
	}

	return ret;
}

CLIENT_DETOUR(CGItem_C__GetCooldowns, 0x00706C50, __cdecl, ItemCache*, (int32_t itemId, uint32_t* outCooldown, uint32_t* outCatCooldown, uint32_t* outCategory))
{
	*outCooldown = 0;
	*outCatCooldown = 0;
	*outCategory = 0;

	void* pCacheDB = WDB_CACHE_ITEM;

	ItemCache* pItemCache = DBItemCache_GetInfoBlockByID(pCacheDB, itemId, nullptr, nullptr, 0, 0);

	if (!pItemCache)
		return nullptr;

	for (int slot = 0; slot < 5; slot++)
	{
		uint32_t spellId = (uint32_t)pItemCache->SpellId[slot];
		int32_t spellTrigger = pItemCache->SpellTrigger[slot];

		if (spellId == 0)
			continue;

		if (spellTrigger != ITEM_SPELLTRIGGER_ON_USE && spellTrigger != ITEM_SPELLTRIGGER_ON_NO_DELAY_USE)
			continue;

		*outCooldown = (uint32_t)pItemCache->SpellCooldown[slot];
		*outCatCooldown = (uint32_t)pItemCache->SpellCatCooldown[slot];
		*outCategory = (uint32_t)pItemCache->SpellCategory[slot];

		SpellRow spellRec;
		memset(&spellRec, 0, sizeof(SpellRow));
		if (ClientDB::GetLocalizedRow(g_SpellDB, spellId, &spellRec))
		{
			if (pItemCache->SpellCooldown[slot] == -1)
				*outCooldown = spellRec.m_recoveryTime;

			if (pItemCache->SpellCatCooldown[slot] == -1)
				*outCatCooldown = spellRec.m_categoryRecoveryTime;

			int32_t flatMod = 0, pctMod = 0;
			bool gotMods = Spell_C::GetSpellModifiers(&spellRec, 0x15, &flatMod, &pctMod);

			if (gotMods)
			{
				if (outCooldown != nullptr)
				{
					int32_t modified = ((int32_t)*outCooldown + flatMod) * pctMod / 100;
					*outCooldown = modified > 0 ? (uint32_t)modified : 0;
				}
				if (outCatCooldown != nullptr)
				{
					int32_t modified = ((int32_t)*outCatCooldown + flatMod) * pctMod / 100;
					*outCatCooldown = modified > 0 ? (uint32_t)modified : 0;
				}
			}
		}
		break;
	}
	return pItemCache;
}

CLIENT_DETOUR_THISCALL(AddCreatureStatsToTooltip, 0x00620950, void, (CGUnit * unit, char* outputBuffer, int bufferSize))
{
	char tempBuffer[32];
	int minDmg, maxDmg;

	UnitFields* unitData = unit->unitData;
	bool isPlayer = (unit->objectBase.ObjectData->OBJECT_FIELD_TYPE & TYPEMASK_PLAYER) != 0;
	bool staffMember = sPlayer.GetSecurityLevel() >= 1;
	if (!isPlayer && staffMember)
	{
		SStr::Printf(outputBuffer, bufferSize, "Entry: %u | Display %u", unit->objectBase.ObjectData->OBJECT_FIELD_ENTRY, unitData->displayId);
		Tooltip::sub_61FEC0(self, outputBuffer, nullptr, dword_AD2D30, dword_AD2D30, 0);
	}

	minDmg = (int)std::ceil(unitData->mainHandMinDamage);
	maxDmg = (int)std::ceil(unitData->mainHandMaxDamage);
	if (minDmg == maxDmg)
	{
		const char* damageTemplate = FrameScript::GetText("SINGLE_DAMAGE_TEMPLATE", -1, 0);
		SStr::Printf(outputBuffer, bufferSize, damageTemplate, minDmg);
	}
	else
	{
		const char* damageTemplate = FrameScript::GetText("DAMAGE_TEMPLATE", -1, 0);
		SStr::Printf(outputBuffer, bufferSize, damageTemplate, minDmg, maxDmg);
	}

	Tooltip::sub_61FEC0(self, outputBuffer, nullptr, dword_AD2D30, dword_AD2D30, 0);

	int physicalDmgClass = MiscFunctions::GetPhysicalDamageClassID();
	int armor = (unitData->resistances[physicalDmgClass] > 0) ? unitData->resistances[physicalDmgClass] : 0;

	const char* armorTemplate = FrameScript::GetText("ARMOR_TEMPLATE", -1, 0);
	SStr::Printf(outputBuffer, bufferSize, armorTemplate, armor);
	Tooltip::sub_61FEC0(self, outputBuffer, nullptr, dword_AD2D30, dword_AD2D30, 0);

	const char* hpTemplate = FrameScript::GetText("HP_TEMPLATE", -1, 0);
	SStr::Printf(outputBuffer, bufferSize, hpTemplate, unitData->unitMaxHealth);
	Tooltip::sub_61FEC0(self, outputBuffer, nullptr, dword_AD2D30, dword_AD2D30, 0);

	for (int resistIdx = 0; resistIdx < 7; resistIdx++)
	{
		if (resistIdx == MiscFunctions::GetPhysicalDamageClassID())
			continue;

		if (unitData->resistances[resistIdx] <= 0)
			continue;

		SStr::Printf(tempBuffer, 32, "RESISTANCE%d_NAME", resistIdx);
		const char* resistName = FrameScript::GetText(tempBuffer, -1, 0);

		if (!resistName || resistName[0] == '\0')
			continue;

		const char* resistTemplate = FrameScript::GetText("RESISTANCE_TEMPLATE", -1, 0);
		SStr::Printf(outputBuffer, bufferSize, resistTemplate, unitData->resistances[resistIdx], resistName);
		Tooltip::sub_61FEC0(self, outputBuffer, nullptr, dword_AD2D30, dword_AD2D30, 0);
	}
}

CLIENT_DETOUR(CancelSpellsInterruptedByMoving, 0x00806620, __cdecl, void, ())
{
	SpellRow spellRec;

	SpellCastNode* ebx = *(SpellCastNode**)dword_AF5254;

	if ((uintptr_t)ebx & 1)
		ebx = NULL;

	while (ebx != NULL && !((uintptr_t)ebx & 1))
	{
		SpellCastNode* next = (SpellCastNode*)ebx->prev;

		if (ebx != *(SpellCastNode**)spellCast)
		{
			WowClientDb_C::GetRow(&spellRec);

			uint32_t spellId = ebx->spellId;

			if (spellId >= g_SpellDB->minIndex && spellId <= g_SpellDB->maxIndex)
			{
				uint32_t index = spellId - g_SpellDB->minIndex;
				void** recordsById = (void**)g_SpellDB->Rows;

				if (recordsById[index] != NULL)
				{
					if (*g_isDBCCompressed)
						ClientDB::DecompressRow(recordsById[index], 0x2A8, &spellRec);
					else
						memcpy(&spellRec, recordsById[index], 0x2A8);

					if (!sPlayer.CanCastWhileMoving(&spellRec))
					{
						if (spellRec.m_interruptFlags & 0x1)
							Spell_C::CancelSpell(ebx, 0, 1, 0x28);
					}
				}
			}

			WowClientDb_C::nullsub_3(&spellRec);
		}

		ebx = next;
	}
}

CLIENT_FUNCTION(CheckToCancelSpellsDueToMovement, 0x00806620, __cdecl, void, ())

CLIENT_DETOUR(CheckToCancelCurrentChannelSpell, 0x005FAA40, __cdecl, void, ())
{
	SpellRow spellRec;

	uint64_t playerGuid = ClntObjMgr::GetActivePlayer();

	CGPlayer* player = (CGPlayer*)ClntObjMgr::ObjectPtr(playerGuid, 0x10);

	if (!player)
	{
		return;
	}

	CheckToCancelSpellsDueToMovement();

	uint64_t localPlayerGuid = *(uint64_t*)0x00CA1238;

	if (player->unitBase.objectBase.ObjectData->OBJECT_FIELD_GUID != localPlayerGuid)
		return;

	uint32_t channelingSpellId = player->unitBase.unitData->channelSpell;

	if (channelingSpellId == 0)
		return;

	// Get spell data
	WowClientDb_C::GetRow(&spellRec);

	if (!ClientDB::GetLocalizedRow(g_SpellDB, channelingSpellId, &spellRec))
		return;

	if (!sPlayer.CanCastWhileMoving(&spellRec))
	{
		if (spellRec.m_channelInterruptFlags & 0x8)
			((void(__cdecl*)(uint32_t))0x008008D0)(channelingSpellId);
	}
}
