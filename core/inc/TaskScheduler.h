#pragma once

#include "TaskGraph_base.h"
#include "Task.h"
#include <QObject>
#include <vector>

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

		TaskScheduler();
		~TaskScheduler();

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


	};
}