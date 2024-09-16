#include "TaskGraphLogger.h"

namespace TaskGraph
{
	namespace Internal
	{
		TaskGraphLogger::TaskGraphLogger()
			: m_logger("TaskGraph")
		{

		}
		
		TaskGraphLogger& TaskGraphLogger::instance()
		{
			static TaskGraphLogger logger;
			return logger;
		}

		Log::LogObject& TaskGraphLogger::logger()
		{
			return instance().m_logger;
		}

		void TaskGraphLogger::log(const Log::Message& msg)
		{
			logger().log(msg);
		}

		void TaskGraphLogger::log(const std::string& msg)
		{
			logger().log(msg);
		}
		void TaskGraphLogger::log(const std::string& msg, Log::Level level)
		{
			logger().log(msg, level);
		}
		void TaskGraphLogger::log(const std::string& msg, Log::Level level, const Log::Color& col)
		{
			logger().log(msg, level, col);
		}

		void TaskGraphLogger::logTrace(const std::string& msg)
		{
			logger().logTrace(msg);
		}
		void TaskGraphLogger::logDebug(const std::string& msg)
		{
			logger().logDebug(msg);
		}
		void TaskGraphLogger::logInfo(const std::string& msg)
		{
			logger().logInfo(msg);
		}
		void TaskGraphLogger::logWarning(const std::string& msg)
		{
			logger().logWarning(msg);
		}
		void TaskGraphLogger::logError(const std::string& msg)
		{
			logger().logError(msg);
		}
		void TaskGraphLogger::logCustom(const std::string& msg)
		{
			logger().logCustom(msg);
		}
	}
}