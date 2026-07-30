#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

class TST_DynamicSpawn : public UnitTest::Test
{
    TEST_CLASS(TST_DynamicSpawn)
public:
    TST_DynamicSpawn()
        : Test("TST_DynamicSpawn")
    {
        ADD_TEST(TST_DynamicSpawn::spawnFromBody);
    }

private:
    TEST_FUNCTION(spawnFromBody)
    {
        TEST_START;

        auto b = std::make_shared<TaskGraph::Task>("B");
        std::atomic<bool> bRan{false};
        b->setWorkFunction([&] { bRan = true; });

        auto a = std::make_shared<TaskGraph::Task>("A");
        std::atomic<bool> aRan{false};
        a->setWorkFunction([b, &aRan](TaskGraph::TaskContext& ctx) {
            aRan = true;
            ctx.spawn(b);
        });

        TaskGraph::TaskScheduler scheduler(2);
        TEST_ASSERT(scheduler.addTask(a));

        scheduler.runTasksAsync();
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        // Wait for the async runner to flip isRunning on.
        while (!scheduler.isRunning() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        while (scheduler.isRunning() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

        TEST_ASSERT(!scheduler.isRunning());
        TEST_ASSERT(aRan.load());
        TEST_ASSERT(bRan.load());
        TEST_ASSERT(a->isDone());
        TEST_ASSERT(b->isDone());
        TEST_ASSERT(scheduler.getTotalTasks() == 2u);
    }
};

TEST_INSTANTIATE(TST_DynamicSpawn);
