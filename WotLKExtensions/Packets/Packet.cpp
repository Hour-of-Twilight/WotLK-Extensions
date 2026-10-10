#include <Defines.h>
#include "Packet.h"

#include <ClientData/ClientFunctions.h> // CDataStore_C, ClientServices::SendPacket
#include <cstring>

Packet::Packet(uint32 opcode)
{
	m_ds = &m_owned;
	m_owns = true;
	CDataStore_C::GenPacket(m_ds);
	CDataStore_C::PutInt32(m_ds, static_cast<int32>(opcode));
}

Packet::Packet(CDataStore* incoming)
{
	m_ds = incoming;
	m_owns = false;
}

Packet::~Packet()
{
	if (m_owns)
		Release();
}

Packet& Packet::PutInt8(int8 v)
{
	CDataStore_C::PutInt8(m_ds, v);
	return *this;
}

Packet& Packet::PutUInt8(uint8 v)
{
	CDataStore_C::PutInt8(m_ds, static_cast<int8>(v));
	return *this;
}

Packet& Packet::PutInt16(int16 v)
{
	CDataStore_C::PutInt16(m_ds, v);
	return *this;
}

Packet& Packet::PutUInt16(uint16 v)
{
	CDataStore_C::PutInt16(m_ds, static_cast<int16>(v));
	return *this;
}

Packet& Packet::PutInt32(int32 v)
{
	CDataStore_C::PutInt32(m_ds, v);
	return *this;
}

Packet& Packet::PutUInt32(uint32 v)
{
	CDataStore_C::PutInt32(m_ds, static_cast<int32>(v));
	return *this;
}

Packet& Packet::PutInt64(int64 v)
{
	CDataStore_C::PutInt64(m_ds, v);
	return *this;
}

Packet& Packet::PutUInt64(uint64 v)
{
	CDataStore_C::PutInt64(m_ds, static_cast<int64>(v));
	return *this;
}

Packet& Packet::PutFloat(float v)
{
	CDataStore_C::PutFloat(m_ds, v);
	return *this;
}

Packet& Packet::PutDouble(double v)
{
	int64 bits;
	std::memcpy(&bits, &v, sizeof(double));
	CDataStore_C::PutInt64(m_ds, bits);
	return *this;
}

Packet& Packet::PutString(const char* v)
{
	CDataStore_C::PutCString(m_ds, const_cast<char*>(v));
	return *this;
}

int8 Packet::GetInt8()
{
	int8 v = 0;
	CDataStore_C::GetInt8(m_ds, &v);
	return v;
}

uint8 Packet::GetUInt8()
{
	return static_cast<uint8>(GetInt8());
}

int16 Packet::GetInt16()
{
	int16 v = 0;
	CDataStore_C::GetInt16(m_ds, &v);
	return v;
}

uint16 Packet::GetUInt16()
{
	return static_cast<uint16>(GetInt16());
}

int32 Packet::GetInt32()
{
	int32 v = 0;
	CDataStore_C::GetInt32(m_ds, &v);
	return v;
}

uint32 Packet::GetUInt32()
{
	uint32 v = 0;
	CDataStore_C::GetUInt32(m_ds, &v);
	return v;
}

int64 Packet::GetInt64()
{
	int64 v = 0;
	CDataStore_C::GetInt64(m_ds, &v);
	return v;
}

uint64 Packet::GetUInt64()
{
	return static_cast<uint64>(GetInt64());
}

float Packet::GetFloat()
{
	float v = 0.0f;
	CDataStore_C::GetFloat(m_ds, &v);
	return v;
}

double Packet::GetDouble()
{
	double out = 0.0;
	uint32 read = static_cast<uint32>(m_ds->m_read);
	if (CDataStore_C::FetchRead(m_ds, read, 8))
	{
		uint8* base = reinterpret_cast<uint8*>(m_ds->m_buffer) - m_ds->m_base;
		out = *reinterpret_cast<double*>(base + read);
		m_ds->m_read += 8;
	}
	return out;
}

void Packet::GetString(char* out, uint32 size)
{
	CDataStore_C::GetCString(m_ds, out, size);
}

void Packet::GetBytes(void* dst, uint32 len)
{
	if (len == 0)
		return;
	uint32 read = static_cast<uint32>(m_ds->m_read);
	if (CDataStore_C::FetchRead(m_ds, read, len))
	{
		uint8* base = reinterpret_cast<uint8*>(m_ds->m_buffer) - m_ds->m_base;
		std::memcpy(dst, base + read, len);
		m_ds->m_read += static_cast<int32>(len);
	}
	else
	{
		std::memset(dst, 0, len);
	}
}

void Packet::Send()
{
	m_ds->m_read = 0;
	ClientServices::SendPacket(m_ds);
}

void Packet::Release()
{
	if (m_ds && !m_released)
	{
		CDataStore_C::Release(m_ds);
		m_released = true;
	}
}
