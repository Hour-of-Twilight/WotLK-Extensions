#ifndef FLAG_96

#define FLAG_96

#include <Defines.h>
class flag96
{
private:
	uint32 part[3];

public:
	flag96(uint32 p1 = 0, uint32 p2 = 0, uint32 p3 = 0)
	{
		part[0] = p1;
		part[1] = p2;
		part[2] = p3;
	}

	inline bool IsEqual(uint32 p1 = 0, uint32 p2 = 0, uint32 p3 = 0) const
	{
		return (part[0] == p1 && part[1] == p2 && part[2] == p3);
	}

	inline bool HasFlag(uint32 p1 = 0, uint32 p2 = 0, uint32 p3 = 0) const
	{
		return (part[0] & p1 || part[1] & p2 || part[2] & p3);
	}

	inline void Set(uint32 p1 = 0, uint32 p2 = 0, uint32 p3 = 0)
	{
		part[0] = p1;
		part[1] = p2;
		part[2] = p3;
	}

	inline bool operator==(flag96 const& right) const
	{
		return (
		    part[0] == right.part[0] &&
		    part[1] == right.part[1] &&
		    part[2] == right.part[2]);
	}

	inline bool operator!=(flag96 const& right) const
	{
		return !(*this == right);
	}

	inline flag96 operator&(flag96 const& right) const
	{
		return flag96(part[0] & right.part[0], part[1] & right.part[1], part[2] & right.part[2]);
	}

	inline flag96& operator&=(flag96 const& right)
	{
		part[0] &= right.part[0];
		part[1] &= right.part[1];
		part[2] &= right.part[2];
		return *this;
	}

	inline flag96 operator|(flag96 const& right) const
	{
		return flag96(part[0] | right.part[0], part[1] | right.part[1], part[2] | right.part[2]);
	}

	inline flag96& operator|=(flag96 const& right)
	{
		part[0] |= right.part[0];
		part[1] |= right.part[1];
		part[2] |= right.part[2];
		return *this;
	}

	inline flag96 operator~() const
	{
		return flag96(~part[0], ~part[1], ~part[2]);
	}

	inline flag96 operator^(flag96 const& right) const
	{
		return flag96(part[0] ^ right.part[0], part[1] ^ right.part[1], part[2] ^ right.part[2]);
	}

	inline flag96& operator^=(flag96 const& right)
	{
		part[0] ^= right.part[0];
		part[1] ^= right.part[1];
		part[2] ^= right.part[2];
		return *this;
	}

	inline operator bool() const
	{
		return (part[0] != 0 || part[1] != 0 || part[2] != 0);
	}

	inline bool operator!() const
	{
		return !(bool(*this));
	}

	inline uint32& operator[](uint8 el)
	{
		return part[el];
	}

	inline uint32 const& operator[](uint8 el) const
	{
		return part[el];
	}
};
#endif
