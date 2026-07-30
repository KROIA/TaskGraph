#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <memory>

class TST_RerunWithoutReset : public UnitTest::Test
{
    TEST_CLASS(TST_RerunWithoutReset)
public:
    TST_RerunWithoutReset()
        : Test("TST_RerunWithoutReset")
    {
        ADD_TEST(TST_RerunWithoutReset::runTwice);
        ADD_TEST(TST_RerunWithoutReset::cancelThenRerun);
    }

private:
    // Run a chain to completion, then run again without resetTasks()
    TEST_FUNCTION(runTwice)
    {
        TEST_START;
        std::atomic<int> counter{0};

        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");
        auto c = std::make_shared<TaskGraph::Task>("C");

        a->setWorkFunction([&] { counter++; });
        b->setWorkFunction([&] { counter++; });
        c->setWorkFunction([&] { counter++; });

        TEST_ASSERT(b->addDependency(a));
        TEST_ASSERT(c->addDependency(b));

        TaskGraph::TaskScheduler scheduler(2);
        TEST_ASSERT(scheduler.addTask(a));
        TEST_ASSERT(scheduler.addTask(b));
        TEST_ASSERT(scheduler.addTask(c));

        scheduler.runTasks();

        TEST_ASSERT(a->isDone());
        TEST_ASSERT(b->isDone());
        TEST_ASSERT(c->isDone());
        TEST_ASSERT(counter.load() == 3);

        // Second run without resetTasks()
        scheduler.runTasks();

        TEST_ASSERT(a->isDone());
        TEST_ASSERT(b->isDone());
        TEST_ASSERT(c->isDone());
        TEST_ASSERT(counter.load() == 6);
    }

    // Cancel mid-run, then re-run without resetTasks() — all tasks should complete
    TEST_FUNCTION(cancelThenRerun)
    {
        TEST_START;
        std::atomic<int> execCount{0};

        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");
        auto c = std::make_shared<TaskGraph::Task>("C");

        TaskGraph::TaskScheduler scheduler(1);

        a->setWorkFunction([&] {
            execCount++;
            scheduler.cancel();
        });
        b->setWorkFunction([&] { execCount++; });
        c->setWorkFunction([&] { execCount++; });

        TEST_ASSERT(b->addDependency(a));
        TEST_ASSERT(c->addDependency(b));

        TEST_ASSERT(scheduler.addTask(a));
        TEST_ASSERT(scheduler.addTask(b));
        TEST_ASSERT(scheduler.addTask(c));

        // First run: A executes and cancels; B and C should not complete
        scheduler.runTasks();
        TEST_ASSERT(execCount.load() >= 1);

        // Re-run without resetTasks() — all three should reach Done
        execCount.store(0);
        a->setWorkFunction([&] { execCount++; });

        scheduler.runTasks();

        TEST_ASSERT(a->isDone());
        TEST_ASSERT(b->isDone());
        TEST_ASSERT(c->isDone());
        TEST_ASSERT(execCount.load() == 3);
    }
};

TEST_INSTANTIATE(TST_RerunWithoutReset);
