#include <Defines.h>
#include <SharedDefines.h>
#include <Player.h>
#include <ClientDetours.h>
#include "Logger.h"
#include <CustomPacket.h>
#include <Packets/Packet.h>

CLIENT_DETOUR_THISCALL(ClientConnection__HandleCharEnum, 0x00464C10, int, (uint32 size, uint32 flags, CDataStore* pData))
{
	sPlayer.SetInWorld(false);
	return ClientConnection__HandleCharEnum(self, size, flags, pData);
}

CLIENT_DETOUR_THISCALL(RealmConnection__constructor, 0x004650E0, void*, (void* vtable))
{
	void* result = RealmConnection__constructor(self, vtable);
	sCustomPacket.SetCustomRealmHandlers(self);
	return result;
}

CLIENT_DETOUR(Packet_SMSG_SET_X_SPELL_MODIFIER, 0x007FDC60, __cdecl, int, (int arg0, int opcode, int arg2, CDataStore* pStore))
{
	Packet r(pStore);
	uint8 op = r.GetUInt8();
	uint8 mask = r.GetUInt8();
	uint32 val = r.GetUInt32();
	uint32 family = r.GetUInt32();

	if (family >= C_MAX_SPELL_MOD_FAMILY)
		return 1;

	sPlayer.SetSpellMod((uint8)family, (SpellModOp)mask, op, opcode == 0x267, (int32)val);
	return 1;
}

CLIENT_DETOUR_THISCALL(ClientServices__RequestCharacterCreate, 0x006B1620, void, (int arg))
{
	*(uint32*)((char*)self + 0x2F5C) = 0;
	*(uint32*)((char*)self + 0x2F4C) = 5;
	*(uint32*)((char*)self + 0x2F50) = 0x2E;
	*(uint32*)((char*)self + 0x2F44) = 0;

	CDataStore ds{};
	memset(&ds, 0, sizeof(ds));

	ds.vTable = (CDataStore_vTable*)CDataStore__v_table;
	ds.m_buffer = nullptr;
	ds.m_base = 0;
	ds.m_alloc = 0;
	ds.m_size = 0;
	ds.m_read = 0;

	CDataStore_C::PutInt32(&ds, 0x36);

	char* name = (char*)arg;

	CDataStore_C::PutCString(&ds, name);

	CDataStore_C::PutInt8(&ds, *(uint8*)(arg + 0x30));
	CDataStore_C::PutInt8(&ds, *(uint8*)(arg + 0x31));
	CDataStore_C::PutInt8(&ds, *(uint8*)(arg + 0x32));
	CDataStore_C::PutInt8(&ds, *(uint8*)(arg + 0x33));
	CDataStore_C::PutInt8(&ds, *(uint8*)(arg + 0x34));
	CDataStore_C::PutInt8(&ds, *(uint8*)(arg + 0x35));
	CDataStore_C::PutInt8(&ds, *(uint8*)(arg + 0x36));
	CDataStore_C::PutInt8(&ds, *(uint8*)(arg + 0x37));
	CDataStore_C::PutInt8(&ds, *(uint8*)(arg + 0x38));
	CDataStore_C::PutInt8(&ds, sPlayer.GetCharCreateGameMode());

	void* conn = ClientServices::GetCurrent();
	ClientServices::SendPacket(&ds);

	if (ds.m_buffer)
		CDataStore_C::Release(&ds);
}
