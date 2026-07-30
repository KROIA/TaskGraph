#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <memory>

class TST_ThreadPoolResize : public UnitTest::Test
{
    TEST_CLASS(TST_ThreadPoolResize)
public:
    TST_ThreadPoolResize()
        : Test("TST_ThreadPoolResize")
    {
        ADD_TEST(TST_ThreadPoolResize::resize);
    }

private:
    TEST_FUNCTION(resize)
    {
        TEST_START;
        TaskGraph::TaskScheduler scheduler(4);
        // Force spawn by grow-to-same then check via a grow.
        TEST_ASSERT(scheduler.enableThreads(4));
        TEST_ASSERT(scheduler.getThreadCount() == 4);

        TEST_ASSERT(scheduler.enableThreads(8));
        TEST_ASSERT(scheduler.getThreadCount() == 8);

        TEST_ASSERT(scheduler.enableThreads(2));
        TEST_ASSERT(scheduler.getThreadCount() == 2);

        std::atomic<int> counter{0};
        std::vector<std::shared_ptr<TaskGraph::Task>> tasks;
        for (int i = 0; i < 6; ++i)
        {
            auto t = std::make_shared<TaskGraph::Task>("T" + std::to_string(i));
            t->setWorkFunction([&counter] { counter.fetch_add(1); });
            tasks.push_back(t);
            TEST_ASSERT(scheduler.addTask(t));
        }
        // Chain them to ensure serialised progress works with the shrunken pool.
        for (size_t i = 1; i < tasks.size(); ++i)
            TEST_ASSERT(tasks[i]->addDependency(tasks[i - 1]));

        scheduler.runTasks();
        TEST_ASSERT(counter.load() == 6);
        for (auto& t : tasks)
            TEST_ASSERT(t->isDone());
        TEST_ASSERT(scheduler.getThreadCount() == 2);
    }
};

TEST_INSTANTIATE(TST_ThreadPoolResize);
