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
	class TASK_GRAPH_EXPORT Task : public QObject
	{
		Q_OBJECT
		public:

		Task(const std::string &name);
		virtual ~Task();

		// Get the name of the task
		const std::string& getName() const { return m_name; }

		// Set the name of the task
		void setName(const std::string& name) { m_name = name; }

		// Set the work function
		void setWorkFunction(const std::function<void()>& workFunction) { m_workFunction = workFunction; }

		bool addDependency(const std::shared_ptr<Task>& task);
		bool clearDependencies();
		const std::vector<std::shared_ptr<Task>>& getDependencies() const { return m_dependencies; }

		void runTask();
		bool isRunning() const { return m_isRunning; }
		bool isDone() const { return m_done; }
		void reset();
		bool checkDependencies();

		signals:
		void started();
		void compleeted();
		void resetted();

		protected:
		virtual void work();
	
		private:
		

		std::string m_name;
		std::atomic<bool> m_isRunning;
		std::atomic<bool> m_done;

		std::function<void()> m_workFunction;
		std::vector<std::shared_ptr<Task>> m_dependencies;

	};
}