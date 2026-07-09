#pragma once
#include <sstream>
#include <string>
#include <fstream>
#include <mutex>

std::string relProjectPath(std::string const& pathIn);

class Logger
{
public:
	static Logger& Instance();

	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	void Reset();
	void Write(const char* type, const char* source, const char* msg);
	void Emit(const std::string& line);

	static std::string Now();

private:
	Logger() = default;
	void EnsureOpen();

	std::mutex m_mutex;
	std::ofstream m_file;
	bool m_opened = false;
};

#define sLog Logger::Instance()

class LogLine
{
public:
	LogLine(const char* type, const char* file, size_t line);
	~LogLine();

	LogLine(const LogLine&) = delete;
	LogLine& operator=(const LogLine&) = delete;

	template <typename T>
	LogLine& operator<<(T const& obj)
	{
#if LOG_LEVEL > 0
		m_ss << obj;
#endif
		return *this;
	}

private:
	std::ostringstream m_ss;
};

// clang-format off
#if LOG_LEVEL >= 4
#define LOG_DEBUG LogLine("DEBUG", __FILE__, __LINE__)
#else
#define LOG_DEBUG if (false) LogLine("DEBUG", 0, 0)
#endif

#if LOG_LEVEL >= 3
#define LOG_INFO LogLine("INFO", __FILE__, __LINE__)
#else
#define LOG_INFO if (false) LogLine("INFO", 0, 0)
#endif

#if LOG_LEVEL >= 2
#define LOG_WARN LogLine("WARN", __FILE__, __LINE__)
#else
#define LOG_WARN if (false) LogLine("WARN", 0, 0)
#endif

#if LOG_LEVEL >= 1
#define LOG_ERROR LogLine("ERROR", __FILE__, __LINE__)
#else
#define LOG_ERROR if (false) LogLine("ERROR", 0, 0)
#endif
// clang-format on
