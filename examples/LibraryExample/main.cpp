#ifdef QT_ENABLED
#include <QApplication>
#endif
#include <iostream>
#include "TaskGraph.h"
#include "Logger.h"

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
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
};
int main(int argc, char* argv[])
{
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
	
	TaskGraph::TaskScheduler scheduler(4);
	std::shared_ptr<TestTask> task1 = std::make_shared<TestTask>(std::string("Task1"));
	std::shared_ptr<TestTask> task2 = std::make_shared<TestTask>(std::string("Task2"));
	std::shared_ptr<TestTask> task3 = std::make_shared<TestTask>(std::string("Task3"));
	std::shared_ptr<TestTask> task4 = std::make_shared<TestTask>(std::string("Task4"));
	std::shared_ptr<TestTask> task5 = std::make_shared<TestTask>(std::string("Task5"));
	std::shared_ptr<TestTask> task6 = std::make_shared<TestTask>(std::string("Task6"));

	scheduler.addTask(task1);
	scheduler.addTask(task2);
	scheduler.addTask(task3);	
	scheduler.addTask(task4);
	scheduler.addTask(task5);
	scheduler.addTask(task6);

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	scheduler.runTasks();


	int ret = 0;
#ifdef QT_ENABLED
	ret = app.exec();
#endif
	TaskGraph::Profiler::stop((std::string(TaskGraph::LibraryInfo::name)+".prof").c_str());
	return ret;
}