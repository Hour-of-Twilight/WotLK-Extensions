#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace ClientData
{
	struct CVar;
}

class Cvar
{
public:
	bool AsBool() const;
	int AsInt() const;
	float AsFloat() const;
	const char* AsString() const;

	void Set(const char* value);
	void Set(int value);
	void Set(bool value)
	{
		Set(value ? 1 : 0);
	}

	const std::string& Name() const
	{
		return m_name;
	}
	ClientData::CVar* Handle() const
	{
		return m_handle;
	}

private:
	friend class Cvars;

	std::string m_name;
	std::string m_help;
	std::string m_default;
	std::string m_value;
	std::function<void(Cvar&)> m_onChange;
	ClientData::CVar* m_handle = nullptr;
};
