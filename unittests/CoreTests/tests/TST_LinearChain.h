#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <memory>

class TST_LinearChain : public UnitTest::Test
{
    TEST_CLASS(TST_LinearChain)
public:
    TST_LinearChain()
        : Test("TST_LinearChain")
    {
        ADD_TEST(TST_LinearChain::runChain);
    }

private:
    TEST_FUNCTION(runChain)
    {
        TEST_START;
        std::atomic<int> counter{0};
        int orderA = -1, orderB = -1, orderC = -1;

        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");
        auto c = std::make_shared<TaskGraph::Task>("C");

        a->setWorkFunction([&] { orderA = counter++; });
        b->setWorkFunction([&] { orderB = counter++; });
        c->setWorkFunction([&] { orderC = counter++; });

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
        TEST_ASSERT(orderA == 0);
        TEST_ASSERT(orderB == 1);
        TEST_ASSERT(orderC == 2);
    }
};

TEST_INSTANTIATE(TST_LinearChain);
