#pragma once

#include <memory>

#include "spdlog/spdlog.h"

class Log
{
public:
	static void Init();

	inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return coreLogger; }
	inline static std::ostringstream& GetCoreLoggerStream() { return coreLoggerStream; }
private:
	static std::shared_ptr<spdlog::logger> coreLogger;
	static std::ostringstream coreLoggerStream;
};

#ifdef _DEBUG
#define LOG_CORE_TRACE(...) ::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LOG_CORE_INFO(...)  ::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LOG_CORE_WARN(...)  ::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LOG_CORE_ERROR(...) ::Log::GetCoreLogger()->error(__VA_ARGS__)
#endif

#ifdef _RELEASE
#define LOG_CORE_TRACE(...) 
#define LOG_CORE_INFO(...)  
#define LOG_CORE_WARN(...)  
#define LOG_CORE_ERROR(...) 
#endif

