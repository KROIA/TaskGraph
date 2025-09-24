#pragma once

#include "TaskGraph_base.h"
#include <QObject>
#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <atomic>

namespace TaskGraph
{
	/// <summary>
	/// A task is a workblock wich can be run by the TaskScheduler.
	/// Each task shuld be processable by a single thread.
	/// A task can have dependencies on other tasks, which must be processed before this task can be run.
	/// Do not interact between tasks, because they can be processed in parallel.
	/// </summary>
	class TASK_GRAPH_API Task : public QObject
	{
		Q_OBJECT
		public:

		Task();
		Task(const std::string &name);
		Task(const Task& other);
		virtual ~Task();

		/// <summary>
		/// Returns the name of the task
		/// </summary>
		/// <returns>name of the task</returns>
		const std::string& getName() const { return m_name; }

		/// <summary>
		/// Sets a name for this task
		/// </summary>
		/// <param name="name"></param>
		void setName(const std::string& name) { m_name = name; }

		/// <summary>
		/// Set a custom work function which will be called when the task is run
		/// The custom work function will be called instead of the "work" function
		/// If no custom work function is set, the "work" function will be called
		/// </summary>
		/// <param name="workFunction"></param>
		void setWorkFunction(const std::function<void()>& workFunction) { m_workFunction = workFunction; }

		/// <summary>
		/// Add a task on which this task depends on
		/// This task can only be run if all dependencies are processed
		/// </summary>
		/// <param name="task"></param>
		/// <returns>
		///		true, if the task was added to the dependency list
		///		false, if the task can't be added to the dependency list.
		///			   Reason: task is already running.	
		/// </returns>
		bool addDependency(const std::shared_ptr<Task>& task);

		/// <summary>
		/// Clears the list of dependencies
		/// </summary>
		/// <returns>
		///		true, if the list of dependencies was cleared. 
		///		false, if the task is currently running.    
		/// </returns>
		bool clearDependencies();

		/// <summary>
		/// Gets a list of task on which this task depends
		/// This task can only be run if all dependencies are processed
		/// </summary>
		/// <returns>vector of tasks</returns>
		const std::vector<std::shared_ptr<Task>>& getDependencies() const { return m_dependencies; }

		/// <summary>
		/// Checks if a custom work function is set, if a custom work function is set, 
		/// it will be called, otherwise the "work" function will be called.
		/// </summary>
		/// <returns>true, if the task was processed, false if the task can't be run</returns>
		bool runTask();

		/// <summary>
		/// Flag that indicates if the task is running
		/// </summary>
		/// <returns>true, if the task is running</returns>
		bool isRunning() const { return m_isRunning; }

		/// <summary>
		/// Flag that indicates if the task is done
		/// </summary>
		/// <returns>true, if the task is done</returns>
		bool isDone() const { return m_done; }

		/// <summary>
		/// Resets the "done" flag of the task and emits the "resetted" signal
		/// </summary>
		void reset();

		/// <summary>
		///		Check if all dependencies are processed
		/// </summary>
		/// <returns>true, if all dependencies are processed, otherwise false</returns>
		bool checkDependencies();

		signals:
		// Signals can be emitted from different threads

		/// <summary>
		/// Gets emitted when the task gets executed
		/// </summary>
		void started();

		/// <summary>
		/// Gets emitted when the task has completed execution
		/// </summary>
		void compleeted();

		/// <summary>
		/// Gets emitted when the task has been resetted
		/// </summary>
		void resetted();

		protected:

		/// <summary>
		///		Work function that is called when the task is run 
		///		Override this function with the custom work payload of the task
		/// </summary>
		virtual void work();
	
		private:
		std::string m_name;
		std::atomic<bool> m_isRunning;
		std::atomic<bool> m_done;

		std::function<void()> m_workFunction;
		std::vector<std::shared_ptr<Task>> m_dependencies;
	};
}