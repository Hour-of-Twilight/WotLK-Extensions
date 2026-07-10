#include "Logger.h"

#include <ClientData/SharedDefines.h> // CGChat::AddChatMessage
#include <ctime>
#include <iomanip>
#include <windows.h>
#include <filesystem>

std::string relProjectPath(std::string const& pathIn)
{
	return std::filesystem::path(pathIn).filename().string();
}

Logger& Logger::Instance()
{
	static Logger instance;
	return instance;
}

std::string Logger::Now()
{
	auto t = std::time(nullptr);
	auto tm = *std::localtime(&t);
	std::ostringstream ss;
	ss << std::put_time(&tm, "%H:%M:%S");
	return ss.str();
}

void Logger::EnsureOpen()
{
	if (m_opened)
		return;

	char buffer[MAX_PATH];
	GetModuleFileNameA(nullptr, buffer, MAX_PATH);
	std::filesystem::path dir = std::filesystem::path(buffer).parent_path() / "Logs";
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);

	m_file.open(dir / "hot_log.log", std::ios::out | std::ios::trunc);
	m_opened = true;
}

void Logger::Reset()
{
#if LOG_LEVEL > 0
	std::lock_guard<std::mutex> lock(m_mutex);
	m_file.close();
	m_opened = false;
	EnsureOpen();
#endif
}

void Logger::Emit(const std::string& line)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		EnsureOpen();
		m_file << line << "\n";
		m_file.flush();
	}
	OutputDebugStringA((line + "\n").c_str());
	//CGChat::AddChatMessage(const_cast<char*>(line.c_str()), 0, 0, 0, nullptr, 0, "", 0, 0, 0, 0, 0, nullptr);
}

void Logger::Write(const char* type, const char* source, const char* msg)
{
#if LOG_LEVEL > 0
	std::ostringstream ss;
	ss << "[" << type << "][" << Now() << "][" << source << "] " << msg;
	Emit(ss.str());
#endif
}

LogLine::LogLine(const char* type, const char* file, size_t line)
{
#if LOG_LEVEL > 0
	m_ss << "[" << type << "][" << Logger::Now() << "][" << relProjectPath(file) << ":" << line << "] ";
#endif
}

LogLine::~LogLine()
{
#if LOG_LEVEL > 0
	sLog.Emit(m_ss.str());
#endif
}
