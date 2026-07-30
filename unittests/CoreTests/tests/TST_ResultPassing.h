#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <memory>

class TST_ResultPassing : public UnitTest::Test
{
    TEST_CLASS(TST_ResultPassing)
public:
    TST_ResultPassing()
        : Test("TST_ResultPassing")
    {
        ADD_TEST(TST_ResultPassing::passIntResult);
    }

private:
    TEST_FUNCTION(passIntResult)
    {
        TEST_START;

        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");

        a->setWorkFunction([](TaskGraph::TaskContext& ctx) {
            ctx.setResult(42);
        });

        std::atomic<int> bSaw{-1};
        std::atomic<bool> bRan{false};
        b->setWorkFunction([a, &bSaw, &bRan](TaskGraph::TaskContext& ctx) {
            bRan = true;
            bSaw = ctx.getDependencyResult<int>(*a);
        });

        TEST_ASSERT(b->addDependency(a));

        TaskGraph::TaskScheduler scheduler(2);
        TEST_ASSERT(scheduler.addTask(a));
        TEST_ASSERT(scheduler.addTask(b));

        scheduler.runTasks();

        TEST_ASSERT(a->isDone());
        TEST_ASSERT(b->isDone());
        TEST_ASSERT(bRan.load());
        TEST_ASSERT(bSaw.load() == 42);
        TEST_ASSERT(TaskGraph::getResultAs<int>(*a) == 42);
    }
};

TEST_INSTANTIATE(TST_ResultPassing);
