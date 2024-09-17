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
	class TASK_GRAPH_EXPORT TaskScheduler : public QObject
	{
		Q_OBJECT
		public:
		enum Error
		{
			noError,
			noTasks,
			circularDependency,

			__count
		};

		TaskScheduler(size_t threadCount = std::thread::hardware_concurrency());
		~TaskScheduler();

		void addTask(const std::shared_ptr<Task>& task);

		void runTasks();

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

		struct TaskThread
		{
			std::shared_ptr<Task> task;
			std::shared_ptr<std::thread> thread;
			//std::condition_variable cv;
			//std::mutex mutex;

			TaskThread()
				: task(nullptr)
				, thread(nullptr)
			{
			}
		};
		std::vector<TaskThread> m_threads;
		std::mutex m_mutex;
		std::condition_variable m_cvTask;
		std::condition_variable m_cvComplete;
		bool m_stopThreads;
		unsigned int m_busyThreads;

		std::deque<std::shared_ptr<Task>> m_tasksToProcess;

		static void taskThreadFunction(TaskScheduler *obj);

	};
}