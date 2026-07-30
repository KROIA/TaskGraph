#ifdef QT_ENABLED
#include <QApplication>
#endif
#include <iostream>
#include <cstdlib>
#include "TaskGraph.h"
#include "Logger.h"
#include "CrashReport.h"

#ifdef QT_WIDGETS_ENABLED
#include <QWidget>
#include "gui/TaskGraphWidget.h"
#endif


class TestTask : public TaskGraph::Task
{
	public:
	TestTask(const std::string& name)
		: TaskGraph::Task(name)
	{
	}
	private:
	void work(TaskGraph::TaskContext& ctx) override
	{
		logger().logInfo(getName() + " started");
		int steps = 4;
		for (int i = 0; i < steps; ++i)
		{
			if (ctx.isCancelRequested())
			{
				logger().logWarning(getName() + " cancelled");
				return;
			}
			logger().logInfo(getName() + " step " + std::to_string(i + 1) + "/" + std::to_string(steps));
			if (i == 1)
				logger().logWarning(getName() + " taking longer than expected");
			if (i == 2)
				logger().logError(getName() + " retrying step " + std::to_string(i + 1));
			logger().logDebug(getName() + " internal state OK");
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
		logger().logInfo(getName() + " done");
	}
};

class AskGuiTask : public TaskGraph::Task
{
public:
	AskGuiTask(const std::string& name)
		: TaskGraph::Task(name)
	{
	}
private:
	void work(TaskGraph::TaskContext& ctx) override
	{
		logger().logInfo(getName() + " requesting GUI input...");
		QVariant response = ctx.askGui(QVariant("What value should " + QString::fromStdString(getName()) + " use?"));
		if (!response.isValid())
		{
			logger().logWarning(getName() + " got no response (cancelled?)");
			return;
		}
		std::string val = response.toString().toStdString();
		logger().logInfo(getName() + " got response: " + val);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		logger().logInfo(getName() + " done with value: " + val);
	}
};

void exiting();
void exceptionCallback();
int main(int argc, char* argv[])
{
	std::atexit(exiting);

	CrashReport::Profiler::start();
	CrashReport::LibraryInfo::printInfo();
	CrashReport::ExceptionHandler::setup("crashFiles");
	//CrashReport::ExceptionHandler::setExceptionCallback(exceptionCallback);
#ifdef QT_WIDGETS_ENABLED
	QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#ifdef QT_ENABLED
	QApplication app(argc, argv);
#endif
	TaskGraph::Profiler::start();
	TaskGraph::LibraryInfo::printInfo();
/*#ifdef QT_WIDGETS_ENABLED
	QWidget* widget = TaskGraph::LibraryInfo::createInfoWidget();
	if (widget)
		widget->show();
#endif*/

	Log::UI::NativeConsoleView consoleView;

	unsigned int hwThreads = std::thread::hardware_concurrency();
	TaskGraph::TaskScheduler scheduler(hwThreads > 0 ? hwThreads : 8);
	std::shared_ptr<TestTask> task1 = std::make_shared<TestTask>(std::string("Task1"));
	std::shared_ptr<TestTask> task2 = std::make_shared<TestTask>(std::string("Task2"));
	std::shared_ptr<TestTask> task3 = std::make_shared<TestTask>(std::string("Task3"));
	std::shared_ptr<TestTask> task4 = std::make_shared<TestTask>(std::string("Task4"));
	std::shared_ptr<TestTask> task5 = std::make_shared<TestTask>(std::string("Task5"));
	std::shared_ptr<TestTask> task6 = std::make_shared<TestTask>(std::string("Task6"));
	std::shared_ptr<TestTask> task7 = std::make_shared<TestTask>(std::string("Task7"));
	std::shared_ptr<TestTask> task8 = std::make_shared<TestTask>(std::string("Task8"));
	std::shared_ptr<TestTask> task9 = std::make_shared<TestTask>(std::string("Task9"));
	std::shared_ptr<TestTask> task10 = std::make_shared<TestTask>(std::string("Task10"));
	auto askTask = std::make_shared<AskGuiTask>(std::string("AskGuiTask"));

	// give some tasks non-default config for inspector demo
	task1->setWeight(2.0f);
	task3->setTimeout(std::chrono::milliseconds(10000));
	task5->setMaxRetries(3);
	task5->setRetryBackoff(std::chrono::milliseconds(200));
	task7->setWeight(0.5f);
	task10->setWeight(3.0f);

	scheduler.addTask(task1);
	scheduler.addTask(task2);
	scheduler.addTask(task3);
	scheduler.addTask(task4);
	scheduler.addTask(task5);
	scheduler.addTask(task6);
	scheduler.addTask(task7);
	scheduler.addTask(task8);
	scheduler.addTask(task9);
	scheduler.addTask(task10);
	scheduler.addTask(askTask);

	task10->addDependency(task9);
	task10->addDependency(task8);
	task10->addDependency(task7);

	task9->addDependency(task6);
	task8->addDependency(task5);
	task7->addDependency(task4);

	task4->addDependency(task1);
	task4->addDependency(task2);
	task3->addDependency(task1);
	task3->addDependency(task2);
	task5->addDependency(task1);
	task5->addDependency(task2);

	// askTask depends on task1 so it runs after, prompts GUI mid-run
	askTask->addDependency(task1);

#ifdef QT_WIDGETS_ENABLED
	TaskGraph::Gui::TaskGraphWidget graphWidget(&scheduler, TaskGraph::Gui::presetEditorFactory());
	graphWidget.resize(1100, 700);
	graphWidget.setWindowTitle("TaskGraph Editor");
	graphWidget.show();

	scheduler.runTasksAsync();
#else
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	scheduler.runTasks();
#endif


	int ret = 0;
#ifdef QT_ENABLED
	ret = app.exec();
#endif
	// Stop any in-progress async run before stack-local widgets and tasks
	// are destroyed — prevents worker threads from emitting into dead objects
	scheduler.cancel();
	while (scheduler.isRunning())
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	TaskGraph::Profiler::stop((std::string(TaskGraph::LibraryInfo::name) + ".prof").c_str());
	return ret;
}

void exiting()
{

}
void exceptionCallback()
{
	//std::cout << "\nCallback reached\n\n";
}
