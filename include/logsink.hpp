#include "isobus/isobus/can_stack_logger.hpp"

#include <deque>
#include <string>

// A log sink for the CAN stack
//================================================================================================
/// @file logsink.hpp
///
/// @brief Defines a sink to store log data for showing to the user later.
/// @author Adrian Del Grosso
///
/// @copyright 2023 Adrian Del Grosso and the Open-Agriculture developers
//================================================================================================
#ifndef LOG_SINK_HPP
#define LOG_SINK_HPP

class CustomLogger : public isobus::CANStackLogger
{
public:
	struct LogInfo
	{
		CANStackLogger::LoggingLevel logLevel;
		const std::string logText;
	};

	void sink_CAN_stack_log(CANStackLogger::LoggingLevel level, const std::string &text) override
	{
		logHistory.push_back({ level, text });

		while (logHistory.size() > 50)
		{
			logHistory.pop_front();
		}
	}

	std::deque<LogInfo> logHistory;
};

static CustomLogger logger;

#endif // LOG_SINK_HPP
