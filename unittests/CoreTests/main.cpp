#include <iostream>
#include <QCoreApplication>
#include "TaskGraph.h"
#include "tests.h"


int main(int argc, char* argv[])
{
	// A QCoreApplication is required so tasks with TaskAffinity::Gui can be
	// marshaled via QMetaObject::invokeMethod(Qt::QueuedConnection) to the
	// main thread. Tests pump events with QCoreApplication::processEvents().
	QCoreApplication app(argc, argv);

	TaskGraph::LibraryInfo::printInfo();

	std::cout << "Running "<< UnitTest::Test::getTests().size() << " tests...\n";
	UnitTest::Test::TestResults results;
	UnitTest::Test::runAllTests(results);
	UnitTest::Test::printResults(results);

	return results.getSuccess();
}
