#include "Task.h"
#include "TaskGraphLogger.h"

namespace TaskGraph
{
	Task::Task(const std::string& name)
		: m_name(name)
		, m_isRunning(false)
		, m_done(false)
	{

	}
	Task::~Task()
	{

	}

	void Task::runTask()
	{
		if (m_isRunning)
		{
			Internal::TaskGraphLogger::logError("Task is already running");
			return;
		}
		if (!checkDependencies())
		{
			Internal::TaskGraphLogger::logError("Task can't run because some dependencies aren't processed");
			return;
		}
		m_isRunning = true;
		Internal::TaskGraphLogger::logInfo("Task " + m_name + " started");
		emit started();
		if(m_workFunction)
			m_workFunction();
		else
			work();
		Internal::TaskGraphLogger::logInfo("Task " + m_name + " completed");
		emit compleeted();
		m_isRunning = false;
		m_done = true;
	}

	void Task::reset()
	{
		m_done = false;
		emit resetted();
	}

	bool Task::checkDependencies()
	{
		for (const auto& dep : m_dependencies)
		{
			if (!dep->isDone())
			{
				return false;
			}
		}
		return true;
	}

	bool Task::addDependency(const std::shared_ptr<Task>& task)
	{
		if (m_isRunning)
		{
			Internal::TaskGraphLogger::logError("Can't add dependencies to a running task");
			return false;
		}
		m_dependencies.push_back(task);
		return true;
	}
	bool Task::clearDependencies()
	{
		if (m_isRunning)
		{
			Internal::TaskGraphLogger::logError("Can't clear dependencies of a running task");
			return false;
		}
		m_dependencies.clear();
		return true;
	}

	void Task::work()
	{
		Internal::TaskGraphLogger::logWarning("Task::work() is not implemented");
	}
}