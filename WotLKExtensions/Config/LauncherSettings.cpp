#include <Config/LauncherSettings.h>

#include <Helpers/Util.h>
#include <Logger.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;

namespace
{
	fs::path SettingsPath()
	{
		return Util::GetExeDir() / ".hotlauncher" / "settings.json";
	}

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

	// End of the value token at v. settings.json is flat, so it is a quoted string or a bare literal.
	size_t ValueEnd(const std::string& json, size_t v)
	{
		if (json[v] == '"')
		{
			for (size_t i = v + 1; i < json.size(); ++i)
			{
				if (json[i] == '\\')
					++i;
				else if (json[i] == '"')
					return i + 1;
			}
			return json.size();
		}

		size_t i = v;
		while (i < json.size() && json[i] != ',' && json[i] != '}' && json[i] != ' ' &&
		       json[i] != '\t' && json[i] != '\r' && json[i] != '\n')
			++i;
		return i;
	}

	// Rewrites one key in place so keys this build knows nothing about survive the write.
	std::string ReplaceValue(std::string json, const char* key, const std::string& raw)
	{
		size_t v = ValuePos(json, key);
		if (v != std::string::npos)
		{
			json.replace(v, ValueEnd(json, v) - v, raw);
			return json;
		}

		size_t open = json.find('{');
		if (open == std::string::npos)
			return std::string("{\n  \"") + key + "\": " + raw + "\n}";

		std::string entry = std::string("\n  \"") + key + "\": " + raw;
		size_t next = json.find_first_not_of(" \t\r\n", open + 1);
		if (next != std::string::npos && json[next] != '}')
			entry += ",";
		json.insert(open + 1, entry);
		return json;
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
	fs::path path = SettingsPath();
	std::string json = ReadFile(path);
	if (json.empty())
	{
		LOG_INFO << "launcher settings: none at " << path.string() << ", using defaults";
		return;
	}

	bool hdPatch = false;
	if (ReadBool(json, "hdPatch", hdPatch))
		m_hdPatch = hdPatch;

	double mbps = 0.0;
	if (ReadNumber(json, "maxDownloadMBps", mbps) && mbps > 0.0)
		m_maxDownloadMBps = mbps;

	double realmId = 0.0;
	if (ReadNumber(json, "selectedRealmId", realmId) && realmId > 0.0)
		m_selectedRealmId = static_cast<int>(realmId);

	ReadString(json, "installDir", m_installDir);

	LOG_INFO << "launcher settings: hdPatch=" << (m_hdPatch.load() ? "1" : "0")
	         << " realm=" << m_selectedRealmId.load()
	         << " maxMBps=" << m_maxDownloadMBps.load();
}

void LauncherSettings::SetHdPatch(bool enabled)
{
	Load();
	if (m_hdPatch.exchange(enabled) == enabled)
		return;

	Persist("hdPatch", enabled ? "true" : "false");
}

void LauncherSettings::SetMaxDownloadMBps(double mbps)
{
	if (mbps < 0.0)
		mbps = 0.0;

	Load();
	if (m_maxDownloadMBps.exchange(mbps) == mbps)
		return;

	char buf[32];
	std::snprintf(buf, sizeof(buf), "%g", mbps);
	Persist("maxDownloadMBps", buf);
}

void LauncherSettings::Persist(const char* key, const std::string& rawValue)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	fs::path path = SettingsPath();
	std::error_code ec;
	fs::create_directories(path.parent_path(), ec);

	std::string json = ReadFile(path);
	if (json.find('{') == std::string::npos)
		json = "{}";
	json = ReplaceValue(std::move(json), key, rawValue);

	std::ofstream f(path, std::ios::trunc | std::ios::binary);
	if (!f)
	{
		LOG_ERROR << "launcher settings: could not write " << path.string();
		return;
	}

	f.write(json.data(), static_cast<std::streamsize>(json.size()));
	LOG_INFO << "launcher settings: wrote " << key << "=" << rawValue;
}
