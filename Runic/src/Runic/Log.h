#pragma once

#include <memory>

#include "spdlog/spdlog.h"

class Log
{
public:
	static void Init();

	inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return m_coreLogger; }
	inline static std::ostringstream& GetCoreLoggerStream() { return m_coreLoggerStream; }
private:
	static std::shared_ptr<spdlog::logger> m_coreLogger;
	static std::ostringstream m_coreLoggerStream;
};

#ifdef _DEBUG
#define LOG_CORE_TRACE(...) ::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LOG_CORE_INFO(...)  ::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LOG_CORE_WARN(...)  ::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LOG_CORE_ERROR(...) ::Log::GetCoreLogger()->error(__VA_ARGS__)
#else
#define LOG_CORE_TRACE(...) 
#define LOG_CORE_INFO(...)  
#define LOG_CORE_WARN(...)  
#define LOG_CORE_ERROR(...) 
#endif
