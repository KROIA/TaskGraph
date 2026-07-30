#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

class TST_MutationDuringRunRejected : public UnitTest::Test
{
    TEST_CLASS(TST_MutationDuringRunRejected)
public:
    TST_MutationDuringRunRejected()
        : Test("TST_MutationDuringRunRejected")
    {
        ADD_TEST(TST_MutationDuringRunRejected::rejectMutation);
    }

private:
    TEST_FUNCTION(rejectMutation)
    {
        TEST_START;
        auto slow = std::make_shared<TaskGraph::Task>("Slow");
        std::atomic<bool> started{false};
        slow->setWorkFunction([&]
        {
            started.store(true, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        });

        TaskGraph::TaskScheduler scheduler(2);
        TEST_ASSERT(scheduler.addTask(slow));
        scheduler.runTasksAsync();

        // Wait until the scheduler is actually running.
        while (!scheduler.isRunning())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        auto extra = std::make_shared<TaskGraph::Task>("Extra");
        bool added = scheduler.addTask(extra);
        TEST_ASSERT(!added);
        TEST_ASSERT(scheduler.getLastError() == TaskGraph::TaskScheduler::Error::busy);

        // Let async finish before scheduler destructs.
        while (scheduler.isRunning())
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
};

TEST_INSTANTIATE(TST_MutationDuringRunRejected);
