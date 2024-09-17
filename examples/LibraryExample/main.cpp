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
#endif


class TestTask : public TaskGraph::Task
{
	public:
	TestTask(const std::string& name)
		: TaskGraph::Task(name)
	{
		//setWorkFunction([this]() { work(); });
	}
	private:
	void work() override
	{
		static Log::LogObject log("workLogger");
		log.logInfo("Task " +getName() +" is running");
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
#ifdef QT_WIDGETS_ENABLED
	QWidget* widget = TaskGraph::LibraryInfo::createInfoWidget();
	if (widget)
		widget->show();
#endif

	Log::UI::NativeConsoleView consoleView;
	
	TaskGraph::TaskScheduler scheduler(0);
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

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	scheduler.runTasks();
	//scheduler.runTasksAsync();


	int ret = 0;
#ifdef QT_ENABLED
	ret = app.exec();
#endif
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