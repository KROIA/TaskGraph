#pragma once

#include "TaskGraph_base.h"
#include "Task.h"
#include <QObject>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>

namespace TaskGraph
{
	/// <summary>
	/// The TaskScheduler contains a list of tasks.
	/// The TaskScheduler will run the tasks in the correct order.
	/// Independent Tasks will be run in parallel.
	/// </summary>
	class TASK_GRAPH_EXPORT TaskScheduler : public QObject
	{
		Q_OBJECT
		public:
		enum Error
		{
			noError,				/// < No error
			noTasks,				/// < No tasks to run	
			missingDependency,		/// < The TaskScheduler does not contain a dependent Task. 
			                        ///   Make sure that all dependencies are added to the TaskScheduler
			dependencyGraphNotDAG,  /// < The dependency graph contains a cycle. 
			                        ///   A cycle in the dependency graph is not allowed.
									///   DAG: Directed Acyclic Graph
			alreadyRunning,			/// < The TaskScheduler is already running
									
			__count					/// < Number of error codes
		};

		TaskScheduler(size_t threadCount = std::thread::hardware_concurrency());
		~TaskScheduler();


		void enableThreads(size_t threadCount);
		void disableThreads();

		void addTask(const std::shared_ptr<Task>& task);

		void runTasks();
		void runTasksAsync();
		void resetTasks();
		void clear();

		int getProgress() const { return m_progress; }
		bool isRunning() const { return m_isRunning; }
		unsigned int getThreadCount() const { return m_threads.size(); }
		unsigned int getBusyThreadCount() const { return m_busyThreads; }

		Error getLastError() const { return m_lastError; }

		signals:
		void started();
		void compleeted();
		void resetted();

		void progressUpdate(int progress); // progress in percentage

		private:

		Error buildTaskGraph();

		std::vector<std::shared_ptr<Task>> m_allTasks;

		
		// Each vector in the vector represents a layer of the graph
		// Each layer can be processed in parallel
		std::vector<std::vector<std::shared_ptr<Task>>> m_taskGraph;


		std::vector<std::shared_ptr<std::thread>> m_threads;
		std::mutex m_mutex;
		std::condition_variable m_cvTask;
		std::condition_variable m_cvComplete;
		bool m_stopThreads;
		unsigned int m_busyThreads;
		std::atomic<bool> m_isRunning;
		int m_deltaProgress; 
		std::atomic<int> m_progress;

		std::deque<std::shared_ptr<Task>> m_tasksToProcess;
		std::shared_ptr<std::thread> m_asyncThread = nullptr;
		Error m_lastError = Error::noError;

		static void taskThreadFunction(TaskScheduler *obj, int threadIndex);

	};
}