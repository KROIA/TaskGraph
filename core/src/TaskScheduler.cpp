#include "TaskScheduler.h"
#include "TaskGraphLogger.h"

namespace TaskGraph
{
	TaskScheduler::TaskScheduler()
	{

	}
	TaskScheduler::~TaskScheduler()
	{

	}


	TaskScheduler::Error TaskScheduler::buildTaskGraph()
	{
		return Error::circularDependency;
	}
}