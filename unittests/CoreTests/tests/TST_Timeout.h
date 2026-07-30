#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <chrono>
#include <memory>
#include <thread>

class TST_Timeout : public UnitTest::Test
{
    TEST_CLASS(TST_Timeout)
public:
    TST_Timeout()
        : Test("TST_Timeout")
    {
        ADD_TEST(TST_Timeout::timesOut);
    }

private:
    TEST_FUNCTION(timesOut)
    {
        TEST_START;

        auto t = std::make_shared<TaskGraph::Task>("Slow");
        t->setTimeout(std::chrono::milliseconds(50));
        t->setWorkFunction([t] {
            for (int i = 0; i < 100; ++i)
            {
                if (t->isCancelRequested())
                    return;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        TaskGraph::TaskScheduler scheduler(2);
        TEST_ASSERT(scheduler.addTask(t));

        scheduler.runTasks();

        TEST_ASSERT(t->getStatus() == TaskGraph::Task::Status::Failed);
        TEST_ASSERT(t->getLastError().contains(QStringLiteral("timeout")));
    }
};

TEST_INSTANTIATE(TST_Timeout);
