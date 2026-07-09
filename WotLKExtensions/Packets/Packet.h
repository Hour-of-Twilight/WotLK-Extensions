#pragma once

#include <cstdint>
#include <ClientData/GameStructs.h> // CDataStore

class Packet
{
public:
	explicit Packet(uint32_t opcode);
	explicit Packet(CDataStore* incoming);
	~Packet();

	Packet(const Packet&) = delete;
	Packet& operator=(const Packet&) = delete;

	Packet& PutInt8(int8_t v);
	Packet& PutUInt8(uint8_t v);
	Packet& PutInt32(int32_t v);
	Packet& PutUInt32(uint32_t v);
	Packet& PutInt64(int64_t v);
	Packet& PutUInt64(uint64_t v);
	Packet& PutFloat(float v);
	Packet& PutDouble(double v);
	Packet& PutString(const char* v);

	int8_t GetInt8();
	uint8_t GetUInt8();
	int16_t GetInt16();
	uint16_t GetUInt16();
	int32_t GetInt32();
	uint32_t GetUInt32();
	int64_t GetInt64();
	uint64_t GetUInt64();
	float GetFloat();
	double GetDouble();
	void GetString(char* out, uint32_t size);
	void GetBytes(void* dst, uint32_t len);

	void Send();

	void Release();

	CDataStore* Raw()
	{
		return m_ds;
	}

private:
	CDataStore m_owned{};
	CDataStore* m_ds = nullptr;
	bool m_owns = false;
	bool m_released = false;
};
