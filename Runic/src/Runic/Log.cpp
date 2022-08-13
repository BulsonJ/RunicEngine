#include "Runic/Log.h"

#include <Tracy.hpp>

#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

std::shared_ptr<spdlog::logger> Log::m_coreLogger;
std::ostringstream Log::m_coreLoggerStream;

void Log::Init() {
	ZoneScoped;

	spdlog::set_pattern("%^[%T] %n: %v%s");

	if (!m_coreLogger)
	{
		auto coreLoggerStreamSink = std::make_shared<spdlog::sinks::ostream_sink_st>(Log::m_coreLoggerStream);
		auto coreLoggerStdStreamSink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
		spdlog::sinks_init_list sinks = {coreLoggerStdStreamSink, coreLoggerStreamSink};
		m_coreLogger = std::make_shared<spdlog::logger>("CORE", sinks);
	}
	m_coreLogger->set_level(spdlog::level::trace);
	spdlog::set_default_logger(m_coreLogger);
}