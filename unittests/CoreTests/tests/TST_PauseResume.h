#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

class TST_PauseResume : public UnitTest::Test
{
    TEST_CLASS(TST_PauseResume)
public:
    TST_PauseResume()
        : Test("TST_PauseResume")
    {
        ADD_TEST(TST_PauseResume::pauseResume);
    }

private:
    TEST_FUNCTION(pauseResume)
    {
        TEST_START;
        std::atomic<int> completed{0};

        std::vector<std::shared_ptr<TaskGraph::Task>> tasks;
        for (int i = 0; i < 5; ++i)
        {
            auto t = std::make_shared<TaskGraph::Task>("T" + std::to_string(i));
            t->setWorkFunction([&completed]
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                completed.fetch_add(1);
            });
            tasks.push_back(t);
        }
        for (size_t i = 1; i < tasks.size(); ++i)
            TEST_ASSERT(tasks[i]->addDependency(tasks[i - 1]));

        TaskGraph::TaskScheduler scheduler(2);
        for (auto& t : tasks)
            TEST_ASSERT(scheduler.addTask(t));

        scheduler.runTasksAsync();

        // Wait until at least the first task has completed, then pause.
        while (completed.load() < 1)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        scheduler.pause();
        TEST_ASSERT(scheduler.isPaused());

        // Give any in-flight task a moment to finish; snapshot after that.
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        const int afterPause = completed.load();

        // While paused, no further progress should occur for 200ms.
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        TEST_ASSERT(completed.load() == afterPause);

        scheduler.resume();
        TEST_ASSERT(!scheduler.isPaused());

        // Wait for run to end.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (scheduler.isRunning() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        TEST_ASSERT(completed.load() == 5);
        for (auto& t : tasks)
            TEST_ASSERT(t->isDone());
    }
};

TEST_INSTANTIATE(TST_PauseResume);
