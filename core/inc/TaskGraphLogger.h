#pragma once

#include "TaskGraph_base.h"
#include "Logger.h"




namespace TaskGraph
{
	namespace Internal
	{
		class TaskGraphLogger
		{
			TaskGraphLogger();
			public:
			static TaskGraphLogger& instance();

			static Log::LogObject& logger();

			static void log(const Log::Message& msg);

			static void log(const std::string& msg);
			static void log(const std::string& msg, Log::Level level);
			static void log(const std::string& msg, Log::Level level, const Log::Color& col);

			static void logTrace(const std::string& msg);
			static void logDebug(const std::string& msg);
			static void logInfo(const std::string& msg);
			static void logWarning(const std::string& msg);
			static void logError(const std::string& msg);
			static void logCustom(const std::string& msg);
			private:

			Log::LogObject m_logger;
		};
	}
}