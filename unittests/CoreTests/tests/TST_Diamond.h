#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <memory>

class TST_Diamond : public UnitTest::Test
{
    TEST_CLASS(TST_Diamond)
public:
    TST_Diamond()
        : Test("TST_Diamond")
    {
        ADD_TEST(TST_Diamond::runDiamond);
    }

private:
    TEST_FUNCTION(runDiamond)
    {
        TEST_START;
        std::atomic<int> ran{0};

        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");
        auto c = std::make_shared<TaskGraph::Task>("C");
        auto d = std::make_shared<TaskGraph::Task>("D");

        auto body = [&] { ++ran; };
        a->setWorkFunction(body);
        b->setWorkFunction(body);
        c->setWorkFunction(body);
        d->setWorkFunction(body);

        TEST_ASSERT(b->addDependency(a));
        TEST_ASSERT(c->addDependency(a));
        TEST_ASSERT(d->addDependency(b));
        TEST_ASSERT(d->addDependency(c));

        TaskGraph::TaskScheduler scheduler(4);
        TEST_ASSERT(scheduler.addTask(a));
        TEST_ASSERT(scheduler.addTask(b));
        TEST_ASSERT(scheduler.addTask(c));
        TEST_ASSERT(scheduler.addTask(d));

        scheduler.runTasks();

        TEST_ASSERT(a->isDone());
        TEST_ASSERT(b->isDone());
        TEST_ASSERT(c->isDone());
        TEST_ASSERT(d->isDone());
        TEST_ASSERT(ran.load() == 4);
    }
};

TEST_INSTANTIATE(TST_Diamond);
