#include "TaskScheduler.h"
#include "TaskGraphLogger.h"
#include "CrashReport.h"
#include <map>

namespace TaskGraph
{
	TaskScheduler::TaskScheduler(size_t threadCount)
		: QObject()
	{
		m_stopThreads = false;
		m_busyThreads = 0;
		m_isRunning = false;
		m_deltaProgress = 0;
		m_progress = 0;
		if (threadCount > 0)
			enableThreads(threadCount);
	}
	TaskScheduler::~TaskScheduler()
	{
		disableThreads();

		if(m_asyncThread)
			m_asyncThread->join();
	}

	void TaskScheduler::enableThreads(size_t threadCount)
	{
		disableThreads();
		for (size_t i = 0; i < threadCount; ++i)
			m_threads.push_back(std::make_shared<std::thread>(taskThreadFunction, this, i));
	}
	void TaskScheduler::disableThreads()
	{
		if (m_threads.size() == 0)
			return;

		m_stopThreads = true;
		m_cvTask.notify_all();

		for (auto& thread : m_threads)
			thread->join();
		m_stopThreads = false;
		m_threads.clear();
	}
	void TaskScheduler::addTask(const std::shared_ptr<Task>& task)
	{
		m_allTasks.push_back(task);
	}

	TaskScheduler::Error TaskScheduler::buildTaskGraph()
	{
		TG_SCHEDULER_PROFILING_FUNCTION(TG_COLOR_STAGE_2);
		struct Node
		{
			std::shared_ptr<Task> task = nullptr;
			bool isVisited = false;
		};

		m_lastError = Error::noError;
		std::map<std::shared_ptr<Task>, Node> nodes;
		for (const auto& task : m_allTasks)
		{
			nodes[task].task = task;
		}

		// Check if all dependencies are available in the list
		for (const auto& task : m_allTasks)
		{
			for (const auto& dependency : task->getDependencies())
			{
				if (nodes.find(dependency) == nodes.end())
				{
					Internal::TaskGraphLogger::logError("Dependency " + dependency->getName() + " not found in the list of tasks");
					m_lastError = Error::missingDependency;
					return Error::missingDependency;
				}
			}
		}


		
		m_taskGraph.clear();
		m_taskGraph.push_back(std::vector<std::shared_ptr<Task>>());

		// Build the graph
		while (true)
		{
			bool allVisited = true;
			for (auto& node : nodes)
			{
				if (!node.second.isVisited)
				{
					allVisited = false;
					break;
				}
			}
			if (allVisited)
				break;

			std::vector<std::shared_ptr<Task>> layer;
			for (auto& node : nodes)
			{
				bool allDepsVisited = std::all_of(node.second.task->getDependencies().begin(), node.second.task->getDependencies().end(),
												  [&nodes](const std::shared_ptr<Task>& dep) { return nodes[dep].isVisited; });
				if (!node.second.isVisited && allDepsVisited)
					layer.push_back(node.second.task);
			}
			if (layer.empty())
			{
				Internal::TaskGraphLogger::logError("Dependency graph is not directed acyclic. (Circular dependency)");
				m_lastError = Error::dependencyGraphNotDAG;
				return Error::dependencyGraphNotDAG;
			}
			for (size_t i = 0; i < layer.size(); ++i)
			{
				nodes[layer[i]].isVisited = true;
			}
			m_taskGraph.push_back(layer);
		}
		return Error::noError;
	}

	void TaskScheduler::runTasks()
	{
		TG_SCHEDULER_PROFILING_FUNCTION(TG_COLOR_STAGE_1);
		m_lastError = Error::noError;
		if (m_isRunning)
		{
			Internal::TaskGraphLogger::logError("Task scheduler is already running");
			m_lastError = Error::alreadyRunning;
			return;
		}
		m_isRunning = true;

		

		// Build the task graph
		Error error = buildTaskGraph();
		if (error != Error::noError)
		{
			Internal::TaskGraphLogger::logError("Error building task graph: " + std::to_string(error));
			m_lastError = error;
			return;
		}

		if (m_allTasks.empty())
		{
			Internal::TaskGraphLogger::logWarning("No tasks to run");
			m_lastError = Error::noTasks;
			return;
		}

		emit started();
		Internal::TaskGraphLogger::logInfo("Executing tasks");
		m_progress = 0;
		m_deltaProgress = 100 / m_allTasks.size();
		for (size_t i = 0; i < m_taskGraph.size(); ++i)
		{
			if (m_threads.size() > 0)
			{
				for (const auto& task : m_taskGraph[i])
					m_tasksToProcess.push_back(task);

				Internal::TaskGraphLogger::logInfo("Processing layer " + std::to_string(i) + " with " + std::to_string(m_tasksToProcess.size()) + " tasks");
				// Notify the threads that there are tasks to process
				m_cvTask.notify_all();

				// Wait for all tasks to be completed
				{
					std::unique_lock<std::mutex> lock(m_mutex);
					m_cvComplete.wait(lock, [this] { return m_tasksToProcess.empty() && m_busyThreads == 0; });
				}
			}
			else
			{
				// Single threaded execution
				for (const auto& task : m_taskGraph[i])
				{
					task->runTask();
					m_progress += m_deltaProgress;
					emit progressUpdate(m_progress);
				}
			}
		}
		m_isRunning = false;
		m_progress = 100;
		emit progressUpdate(m_progress);
		emit compleeted();
	}

	void TaskScheduler::runTasksAsync()
	{
		if (m_isRunning)
			return;

		if(m_asyncThread)
			m_asyncThread->join();

		m_asyncThread = std::make_shared<std::thread>([this] 
			{ 
				  TG_SCHEDULER_PROFILING_THREAD("AsyncThread");
				  runTasks(); 
			});
	}

	void TaskScheduler::resetTasks()
	{
		for (const auto& task : m_allTasks)
			task->reset();
		emit resetted();
	}

	void TaskScheduler::clear()
	{
		m_allTasks.clear();
		m_taskGraph.clear();
	}


	void TaskScheduler::taskThreadFunction(TaskScheduler* obj, int threadIndex)
	{
		TG_SCHEDULER_PROFILING_THREAD(std::string("TaskThread["+std::to_string(threadIndex)+"]").c_str());
		STACK_WATCHER_FUNC;
		//Log::LogObject log("Thread "+std::to_string(std::this_thread::get_id()._Get_underlying_id()));
		std::shared_ptr<Task> currentTask = nullptr;
		while (true)
		{
			// Wait for a task to be assigned
			{
				TG_SCHEDULER_PROFILING_BLOCK("WaitForTask", TG_COLOR_STAGE_2);
				std::unique_lock<std::mutex> lock(obj->m_mutex);
				//log.logInfo("Thread waiting for task");
				obj->m_cvTask.wait(lock, [obj] { return obj->m_stopThreads || !obj->m_tasksToProcess.empty(); });
				if (!obj->m_tasksToProcess.empty())
				{
					currentTask = obj->m_tasksToProcess.front();
					obj->m_tasksToProcess.pop_front();
					++obj->m_busyThreads;
					//log.logInfo("Task " + currentTask->getName() + " assigned to thread");
				}
			}
			if (obj->m_stopThreads)
				break;

			// Execute the task
			//log.logInfo("Task " + currentTask->getName() + " started");
			TG_GENERAL_PROFILING_NONSCOPED_BLOCK("Process task", TG_COLOR_STAGE_2);
			currentTask->runTask();
			TG_GENERAL_PROFILING_END_BLOCK;

			//log.logInfo("Task " + currentTask->getName() + " completed");

			// Notify the task scheduler that the task has been completed
			{
				std::unique_lock<std::mutex> lock(obj->m_mutex);
				--obj->m_busyThreads;
				obj->m_progress += obj->m_deltaProgress;
				obj->m_cvComplete.notify_one();
			}
			emit obj->progressUpdate(obj->m_progress);
		}
		//log.logInfo("Thread stopped");
	}
}