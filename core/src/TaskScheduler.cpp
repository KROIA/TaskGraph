#include "TaskScheduler.h"
#include "TaskGraphLogger.h"

namespace TaskGraph
{
	TaskScheduler::TaskScheduler(size_t threadCount)
		: QObject()
	{
		m_stopThreads = false;
		m_busyThreads = 0;
		for (size_t i = 0; i < threadCount; ++i)
		{
			m_threads.push_back(TaskThread());
			m_threads[i].thread = std::make_shared<std::thread>(taskThreadFunction, this);
		}
	}
	TaskScheduler::~TaskScheduler()
	{
		m_stopThreads = true;
		m_cvTask.notify_all();

		for (auto& thread : m_threads)
		{
			thread.thread->join();
		}
	}

	void TaskScheduler::addTask(const std::shared_ptr<Task>& task)
	{
		m_allTasks.push_back(task);
	}

	TaskScheduler::Error TaskScheduler::buildTaskGraph()
	{
		m_taskGraph.clear();
		m_taskGraph.push_back(std::vector<std::shared_ptr<Task>>());
		for (const auto& task : m_allTasks)
		{
			if (task->getDependencies().empty())
			{
				m_taskGraph[0].push_back(task);
			}
		}

		// Clear the task graph
		m_allTasks.clear();
		//return Error::circularDependency;
		return Error::noError;
	}

	void TaskScheduler::runTasks()
	{
		// Build the task graph
		Error error = buildTaskGraph();
		if (error != Error::noError)
		{
			Internal::TaskGraphLogger::logError("Error building task graph: " + std::to_string(error));
			return;
		}

		Internal::TaskGraphLogger::logInfo("Executing tasks");
		for (size_t i = 0; i < m_taskGraph.size(); ++i)
		{
			for (const auto& task : m_taskGraph[i])
			{
				m_tasksToProcess.push_back(task);
			}

			Internal::TaskGraphLogger::logInfo("Processing layer " + std::to_string(i) + " with "+std::to_string(m_tasksToProcess.size())+ " tasks");
			// Notify the threads that there are tasks to process
			m_cvTask.notify_all();

			// Wait for all tasks to be completed
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_cvComplete.wait(lock, [this] { return m_tasksToProcess.empty() && m_busyThreads == 0; });
			}
		}

	}


	void TaskScheduler::taskThreadFunction(TaskScheduler* obj)
	{
		Log::LogObject log("Thread "+std::to_string(std::this_thread::get_id()._Get_underlying_id()));
		std::shared_ptr<Task> currentTask = nullptr;
		while (true)
		{
			// Wait for a task to be assigned
			{
				std::unique_lock<std::mutex> lock(obj->m_mutex);
				log.logInfo("Thread waiting for task");
				obj->m_cvTask.wait(lock, [obj] { return obj->m_stopThreads || !obj->m_tasksToProcess.empty(); });
				if (!obj->m_tasksToProcess.empty())
				{
					currentTask = obj->m_tasksToProcess.front();
					obj->m_tasksToProcess.pop_front();
					++obj->m_busyThreads;
					log.logInfo("Task " + currentTask->getName() + " assigned to thread");
				}
			}
			if (obj->m_stopThreads)
				break;

			// Execute the task
			log.logInfo("Task " + currentTask->getName() + " started");

			currentTask->runTask();
			log.logInfo("Task " + currentTask->getName() + " completed");

			// Notify the task scheduler that the task has been completed
			{
				std::unique_lock<std::mutex> lock(obj->m_mutex);
				--obj->m_busyThreads;
				obj->m_cvComplete.notify_one();
			}
		}
		log.logInfo("Thread stopped");
	}
}