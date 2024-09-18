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
	using TaskList = std::vector<std::shared_ptr<Task>>;
	using TaskDeque = std::deque<std::shared_ptr<Task>>;

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
			taskAlreadyAdded,		/// < The Task is already added to the TaskScheduler
			missingDependency,		/// < The TaskScheduler does not contain a dependent Task. 
			                        ///   Make sure that all dependencies are added to the TaskScheduler
			dependencyGraphNotDAG,  /// < The dependency graph contains a cycle. 
			                        ///   A cycle in the dependency graph is not allowed.
									///   DAG: Directed Acyclic Graph
			alreadyRunning,			/// < The TaskScheduler is already running
			busy,				    /// < Action can't be performed because the TaskScheduler is busy
									
			__count					/// < Number of error codes
		};

		/// <summary>
		/// Creates a TaskScheduler with a number of threads.
		/// If the threadCount is 0, the TaskScheduler will not use threads.
		/// </summary>
		/// <param name="threadCount"></param>
		TaskScheduler(size_t threadCount = std::thread::hardware_concurrency());
		~TaskScheduler();

		/// <summary>
		/// Enables the TaskScheduler to use threads.
		/// If the threadCount is 0, the TaskScheduler will not use threads.
		/// </summary>
		/// <param name="threadCount"></param>
		/// <returns>
		///		true, if the desired threadcount was applyed.
		///		false, if the TaskScheduler is still running.
		/// </returns>
		bool enableThreads(size_t threadCount);

		/// <summary>
		/// Disables the TaskScheduler to use threads.
		/// </summary>
		/// <returns>
		/// 	true, if the threads were disabled.
		///		false, if the TaskScheduler is still running.
		/// </returns>
		bool disableThreads();

		/// <summary>
		/// Adds a Task to the TaskScheduler.
		/// </summary>
		/// <param name="task"></param>
		/// <returns>
		///		true, if the task was added.
		/// 	false, if the taks can't be added because it is already added.
		/// </returns>
		bool addTask(const std::shared_ptr<Task>& task);

		/// <summary>
		/// Starts the scheduling of the tasks.
		/// This function will block until all tasks are completed.
		/// </summary>
		void runTasks();

		/// <summary>
		/// Starts the scheduling of the tasks.
		/// This function is non-blocking.
		/// </summary>
		void runTasksAsync();

		/// <summary>
		/// Reset is needed to run the tasks again.
		/// </summary>
		void resetTasks();

		/// <summary>
		/// Removes all tasks from the TaskScheduler.
		/// </summary>
		void clear();

		/// <summary>
		/// Gets the percentage of the progress.
		/// </summary>
		/// <returns>Progress in a range from 0 to 100</returns>
		int getProgress() const { return m_progress; }

		/// <summary>
		/// Gets the busy state of the TaskScheduler.
		/// </summary>
		/// <returns>true, if the TaskScheduler is busy, otherwise false</returns>
		bool isRunning() const { return m_isRunning; }

		/// <summary>
		/// Gets the number of threads used to run the tasks.
		/// If the TaskScheduler is not using threads, the return value is 0.
		/// </summary>
		/// <returns>number of threads used to run the tasks</returns>
		unsigned int getThreadCount() const { return m_threads.size(); }

		/// <summary>
		/// Gets the amount of threads that are currently busy.
		/// </summary>
		/// <returns>amount of busy threads</returns>
		unsigned int getBusyThreadCount() const { return m_busyThreads; }

		/// <summary>
		/// Gets the last error that occured.
		/// Methods which can fail will set the last error.
		/// </summary>
		/// <returns></returns>
		Error getLastError() const { return m_lastError; }

		/// <summary>
		/// Gets a 2D list of tasks.
		/// The first index represents layers of tasks.
		/// Each layer can be executed in parallel.
		/// </summary>
		/// <returns></returns>
		std::vector<TaskList> getTaskGraph() const;


		signals:
		// Signals can be emitted from different threads
		
		/// <summary>
		/// Gets emitted when the TaskScheduler starts to run the tasks.
		/// </summary>
		void started();

		/// <summary>
		/// Gets emitted when the TaskScheduler has completed all tasks.
		/// </summary>
		void compleeted();

		/// <summary>
		/// Gets emitted when the TaskScheduler has resetted all tasks.
		/// </summary>
		void resetted();

		/// <summary>
		/// Gets emitted when the TaskScheduler has updated the progress.
		/// </summary>
		/// <param name="progress"></param>
		void progressUpdate(int progress); // progress in percentage

		

		private:

		/// <summary>
		/// Sorts the tasks in the correct execution order.
		/// The result is stored in taskGraph.
		/// </summary>
		/// <param name="taskGraph"></param>
		/// <returns></returns>
		Error buildTaskGraph(std::vector<TaskList> &taskGraph) const;

		/// <summary>
		/// Stores all tasks in a random order
		/// </summary>
		TaskList m_allTasks;

		
		/// <summary>
		/// After calling buildTaskGraph, the tasks are sorted in the correct execution order.
		/// taskGraph contains a list of multiple tasks. 
		/// Each list can be executed in parallel and are independent of each other.
		/// The first list must be executet first.
		/// </summary>
		std::vector<TaskList> m_taskGraph;

		/// <summary>
		/// Threadpool to run the tasks
		/// </summary>
		std::vector<std::shared_ptr<std::thread>> m_threads;
		std::mutex m_mutex;
		std::condition_variable m_cvTask;
		std::condition_variable m_cvComplete;
		bool m_stopThreads;
		unsigned int m_busyThreads;
		std::atomic<bool> m_isRunning;
		int m_deltaProgress; 
		std::atomic<int> m_progress;

		TaskDeque m_tasksToProcess;
		std::shared_ptr<std::thread> m_asyncThread = nullptr;
		mutable Error m_lastError = Error::noError;

		static void taskThreadFunction(TaskScheduler *obj, int threadIndex);

	};
}