#include "Log.h"

#include <Tracy.hpp>

#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

std::shared_ptr<spdlog::logger> Log::coreLogger;
std::ostringstream Log::coreLoggerStream;

void Log::Init() {
	ZoneScoped;

	spdlog::set_pattern("%^[%T] %n: %v%s");

	if (!coreLogger)
	{
		auto coreLoggerStreamSink = std::make_shared<spdlog::sinks::ostream_sink_st>(Log::coreLoggerStream);
		auto coreLoggerStdStreamSink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
		spdlog::sinks_init_list sinks = {coreLoggerStdStreamSink, coreLoggerStreamSink};
		coreLogger = std::make_shared<spdlog::logger>("CORE", sinks);
	}
	coreLogger->set_level(spdlog::level::trace);
	spdlog::set_default_logger(coreLogger);
}