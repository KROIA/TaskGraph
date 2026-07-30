#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <memory>
#include <thread>

class TST_RemoveTask : public UnitTest::Test
{
    TEST_CLASS(TST_RemoveTask)
public:
    TST_RemoveTask()
        : Test("TST_RemoveTask")
    {
        ADD_TEST(TST_RemoveTask::addRemoveRoundTrip);
        ADD_TEST(TST_RemoveTask::removeDetachesDeps);
        ADD_TEST(TST_RemoveTask::removeWhileRunningRejected);
    }

private:
    TEST_FUNCTION(addRemoveRoundTrip)
    {
        TEST_START;
        TaskGraph::TaskScheduler scheduler(2);
        auto a = std::make_shared<TaskGraph::Task>("A");
        a->setWorkFunction([]{});

        TEST_ASSERT(scheduler.addTask(a));
        TEST_ASSERT(scheduler.removeTask(a));

        // task is gone — graph should be empty
        auto graph = scheduler.getTaskGraph();
        TEST_ASSERT(graph.empty());

        // re-add should work
        TEST_ASSERT(scheduler.addTask(a));
        scheduler.runTasks();
        TEST_ASSERT(a->isDone());
    }

    TEST_FUNCTION(removeDetachesDeps)
    {
        TEST_START;
        TaskGraph::TaskScheduler scheduler(2);
        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");
        auto c = std::make_shared<TaskGraph::Task>("C");
        a->setWorkFunction([]{});
        b->setWorkFunction([]{});
        c->setWorkFunction([]{});

        b->addDependency(a);
        c->addDependency(b);

        scheduler.addTask(a);
        scheduler.addTask(b);
        scheduler.addTask(c);

        // remove b — c should lose its dependency on b
        TEST_ASSERT(scheduler.removeTask(b));

        // c should now have no deps and run fine with just a
        scheduler.runTasks();
        TEST_ASSERT(a->isDone());
        TEST_ASSERT(c->isDone());
    }

    TEST_FUNCTION(removeWhileRunningRejected)
    {
        TEST_START;
        TaskGraph::TaskScheduler scheduler(2);
        auto a = std::make_shared<TaskGraph::Task>("A");
        std::atomic<bool> go{false};
        a->setWorkFunction([&go]{
            while (!go.load(std::memory_order_acquire))
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });

        scheduler.addTask(a);
        scheduler.runTasksAsync();

        // wait until actually running
        while (!scheduler.isRunning())
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

        TEST_ASSERT(!scheduler.removeTask(a));
        TEST_ASSERT(scheduler.getLastError() == TaskGraph::TaskScheduler::Error::busy);

        go.store(true, std::memory_order_release);
        while (scheduler.isRunning())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
};

TEST_INSTANTIATE(TST_RemoveTask);
