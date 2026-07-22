#include <Config/LauncherSettings.h>

#include <Helpers/Util.h>
#include <Logger.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace
{
	std::string ReadFile(const fs::path& p)
	{
		std::ifstream f(p, std::ios::binary);
		if (!f)
			return "";
		std::ostringstream ss;
		ss << f.rdbuf();
		return ss.str();
	}

	// Position of the value for a top-level key, or npos.
	size_t ValuePos(const std::string& json, const char* key)
	{
		std::string needle = std::string("\"") + key + "\"";
		size_t k = json.find(needle);
		if (k == std::string::npos)
			return std::string::npos;

		size_t c = json.find(':', k + needle.size());
		if (c == std::string::npos)
			return std::string::npos;

		size_t v = c + 1;
		while (v < json.size() && (json[v] == ' ' || json[v] == '\t' || json[v] == '\r' || json[v] == '\n'))
			++v;

		return v < json.size() ? v : std::string::npos;
	}

	bool ReadBool(const std::string& json, const char* key, bool& out)
	{
		size_t v = ValuePos(json, key);
		if (v == std::string::npos)
			return false;

		out = json.compare(v, 4, "true") == 0;
		return true;
	}

	bool ReadNumber(const std::string& json, const char* key, double& out)
	{
		size_t v = ValuePos(json, key);
		if (v == std::string::npos)
			return false;

		out = std::strtod(json.c_str() + v, nullptr);
		return true;
	}

	// Only handles the escapes the launcher can actually emit in a path.
	bool ReadString(const std::string& json, const char* key, std::string& out)
	{
		size_t v = ValuePos(json, key);
		if (v == std::string::npos || json[v] != '"')
			return false;

		out.clear();
		for (size_t i = v + 1; i < json.size(); ++i)
		{
			char c = json[i];
			if (c == '"')
				return true;
			if (c == '\\' && i + 1 < json.size())
				c = json[++i];
			out += c;
		}
		return false;
	}
}

LauncherSettings& LauncherSettings::Instance()
{
	static LauncherSettings instance;
	return instance;
}

void LauncherSettings::Load()
{
	std::call_once(m_once, [this]
	{
		LoadFromDisk();
	});
}

void LauncherSettings::LoadFromDisk()
{
	fs::path path = Util::GetExeDir() / ".hotlauncher" / "settings.json";
	std::string json = ReadFile(path);
	if (json.empty())
	{
		LOG_INFO << "launcher settings: none at " << path.string() << ", using defaults";
		return;
	}

	ReadBool(json, "hdPatch", m_hdPatch);

	double mbps = 0.0;
	if (ReadNumber(json, "maxDownloadMBps", mbps) && mbps > 0.0)
		m_maxDownloadBytesPerSecond = static_cast<long long>(mbps * 1024 * 1024);

	double realmId = 0.0;
	if (ReadNumber(json, "selectedRealmId", realmId) && realmId > 0.0)
		m_selectedRealmId = static_cast<int>(realmId);

	ReadString(json, "installDir", m_installDir);

	LOG_INFO << "launcher settings: hdPatch=" << (m_hdPatch ? "1" : "0")
	         << " realm=" << m_selectedRealmId
	         << " maxBps=" << m_maxDownloadBytesPerSecond;
}
