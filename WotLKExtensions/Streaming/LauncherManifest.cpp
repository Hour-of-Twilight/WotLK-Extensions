#include "LauncherManifest.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>

namespace fs = std::filesystem;

namespace Streaming
{
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

		void SkipWs(const std::string& s, size_t& i)
		{
			while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n'))
				++i;
		}

		bool ParseStr(const std::string& s, size_t& i, std::string& out)
		{
			SkipWs(s, i);
			if (i >= s.size() || s[i] != '"')
				return false;
			++i;
			out.clear();
			while (i < s.size())
			{
				char c = s[i++];
				if (c == '"')
					return true;
				if (c != '\\')
				{
					out += c;
					continue;
				}
				if (i >= s.size())
					return false;
				char e = s[i++];
				switch (e)
				{
				case '"':
					out += '"';
					break;
				case '\\':
					out += '\\';
					break;
				case '/':
					out += '/';
					break;
				case 'n':
					out += '\n';
					break;
				case 't':
					out += '\t';
					break;
				case 'r':
					out += '\r';
					break;
				case 'b':
					out += '\b';
					break;
				case 'f':
					out += '\f';
					break;
				case 'u':
				{
					if (i + 4 > s.size())
						return false;
					unsigned v = std::strtoul(s.substr(i, 4).c_str(), nullptr, 16);
					i += 4;
					out += (v < 0x80) ? static_cast<char>(v) : '?';
					break;
				}
				default:
					return false;
				}
			}
			return false;
		}

		bool FindString(const std::string& s, const char* key, std::string& out)
		{
			std::string needle = std::string("\"") + key + "\"";
			size_t k = s.find(needle);
			if (k == std::string::npos)
				return false;
			size_t i = k + needle.size();
			SkipWs(s, i);
			if (i >= s.size() || s[i] != ':')
				return false;
			++i;
			return ParseStr(s, i, out);
		}
	}

	bool ParseManifest(const std::string& json, Manifest& out)
	{
		FindString(json, "baseUrl", out.baseUrl);

		size_t f = json.find("\"files\"");
		if (f == std::string::npos)
			return false;
		size_t i = json.find('[', f);
		if (i == std::string::npos)
			return false;
		++i;

		while (true)
		{
			SkipWs(json, i);
			if (i >= json.size() || json[i] == ']')
				break;
			if (json[i] != '{')
			{
				++i;
				continue;
			}

			ManifestFile mf;
			int depth = 0;
			for (; i < json.size(); ++i)
			{
				char c = json[i];
				if (c == '"')
				{
					size_t ks = i;
					std::string key;
					if (!ParseStr(json, ks, key))
						return false;
					size_t after = ks;
					SkipWs(json, after);
					if (after < json.size() && json[after] == ':')
					{
						size_t v = after + 1;
						SkipWs(json, v);
						if (key == "path")
						{
							ParseStr(json, v, mf.path);
							i = v - 1;
						}
						else if (key == "sha256")
						{
							ParseStr(json, v, mf.sha256);
							i = v - 1;
						}
						else if (key == "size")
						{
							mf.size = std::strtoll(json.c_str() + v, nullptr, 10);
							i = v - 1;
						}
						else if (key == "hd")
						{
							mf.hd = (json.compare(v, 4, "true") == 0);
							i = v - 1;
						}
						else
							i = ks - 1; // unknown key: skip its name, let the loop continue
					}
					else
						i = ks - 1;
				}
				else if (c == '}')
					break;
			}
			if (!mf.path.empty())
				out.files.push_back(std::move(mf));
			if (i < json.size())
				++i;
		}
		return true;
	}

	void LoadLauncherUrls(const std::wstring& installDir, std::wstring& manifestUrl, std::string& patchBaseUrl)
	{
		manifestUrl = L"https://download.houroftwilight.net/patch/manifest.json";
		patchBaseUrl = "https://download.houroftwilight.net/patch";

		std::string json = ReadFile(fs::path(installDir) / "launcher.json");
		if (json.empty())
			return;

		std::string m, b;
		if (FindString(json, "manifestUrl", m) && !m.empty())
			manifestUrl.assign(m.begin(), m.end());
		if (FindString(json, "patchBaseUrl", b) && !b.empty())
			patchBaseUrl = b;
	}
}
