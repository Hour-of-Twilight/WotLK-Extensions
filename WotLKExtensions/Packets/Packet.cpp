#include "Packet.h"

#include <ClientData/ClientFunctions.h> // CDataStore_C, ClientServices::SendPacket
#include <cstring>

Packet::Packet(uint32_t opcode)
{
	m_ds = &m_owned;
	m_owns = true;
	CDataStore_C::GenPacket(m_ds);
	CDataStore_C::PutInt32(m_ds, static_cast<int32_t>(opcode));
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

Packet& Packet::PutInt8(int8_t v)
{
	CDataStore_C::PutInt8(m_ds, v);
	return *this;
}

Packet& Packet::PutUInt8(uint8_t v)
{
	CDataStore_C::PutInt8(m_ds, static_cast<int8_t>(v));
	return *this;
}

Packet& Packet::PutInt32(int32_t v)
{
	CDataStore_C::PutInt32(m_ds, v);
	return *this;
}

Packet& Packet::PutUInt32(uint32_t v)
{
	CDataStore_C::PutInt32(m_ds, static_cast<int32_t>(v));
	return *this;
}

Packet& Packet::PutInt64(int64_t v)
{
	CDataStore_C::PutInt64(m_ds, v);
	return *this;
}

Packet& Packet::PutUInt64(uint64_t v)
{
	CDataStore_C::PutInt64(m_ds, static_cast<int64_t>(v));
	return *this;
}

Packet& Packet::PutFloat(float v)
{
	CDataStore_C::PutFloat(m_ds, v);
	return *this;
}

Packet& Packet::PutDouble(double v)
{
	int64_t bits;
	std::memcpy(&bits, &v, sizeof(double));
	CDataStore_C::PutInt64(m_ds, bits);
	return *this;
}

Packet& Packet::PutString(const char* v)
{
	CDataStore_C::PutCString(m_ds, const_cast<char*>(v));
	return *this;
}

int8_t Packet::GetInt8()
{
	int8_t v = 0;
	CDataStore_C::GetInt8(m_ds, &v);
	return v;
}

uint8_t Packet::GetUInt8()
{
	return static_cast<uint8_t>(GetInt8());
}

int16_t Packet::GetInt16()
{
	int16_t v = 0;
	CDataStore_C::GetInt16(m_ds, &v);
	return v;
}

uint16_t Packet::GetUInt16()
{
	return static_cast<uint16_t>(GetInt16());
}

int32_t Packet::GetInt32()
{
	int32_t v = 0;
	CDataStore_C::GetInt32(m_ds, &v);
	return v;
}

uint32_t Packet::GetUInt32()
{
	uint32_t v = 0;
	CDataStore_C::GetUInt32(m_ds, &v);
	return v;
}

int64_t Packet::GetInt64()
{
	int64_t v = 0;
	CDataStore_C::GetInt64(m_ds, &v);
	return v;
}

uint64_t Packet::GetUInt64()
{
	return static_cast<uint64_t>(GetInt64());
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
	uint32_t read = static_cast<uint32_t>(m_ds->m_read);
	if (CDataStore_C::FetchRead(m_ds, read, 8))
	{
		uint8_t* base = reinterpret_cast<uint8_t*>(m_ds->m_buffer) - m_ds->m_base;
		out = *reinterpret_cast<double*>(base + read);
		m_ds->m_read += 8;
	}
	return out;
}

void Packet::GetString(char* out, uint32_t size)
{
	CDataStore_C::GetCString(m_ds, out, size);
}

void Packet::GetBytes(void* dst, uint32_t len)
{
	if (len == 0)
		return;
	uint32_t read = static_cast<uint32_t>(m_ds->m_read);
	if (CDataStore_C::FetchRead(m_ds, read, len))
	{
		uint8_t* base = reinterpret_cast<uint8_t*>(m_ds->m_buffer) - m_ds->m_base;
		std::memcpy(dst, base + read, len);
		m_ds->m_read += static_cast<int32_t>(len);
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
