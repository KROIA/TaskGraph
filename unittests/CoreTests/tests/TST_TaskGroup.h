#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

class TST_TaskGroup : public UnitTest::Test
{
    TEST_CLASS(TST_TaskGroup)
public:
    TST_TaskGroup()
        : Test("TST_TaskGroup")
    {
        ADD_TEST(TST_TaskGroup::groupDependency);
    }

private:
    TEST_FUNCTION(groupDependency)
    {
        TEST_START;
        std::atomic<int> completedBeforeD{0};
        std::atomic<bool> dRan{false};
        std::atomic<int> dSawCount{0};

        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");
        auto c = std::make_shared<TaskGraph::Task>("C");
        auto d = std::make_shared<TaskGraph::Task>("D");

        auto work = [&completedBeforeD]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            completedBeforeD.fetch_add(1);
        };
        a->setWorkFunction(work);
        b->setWorkFunction(work);
        c->setWorkFunction(work);
        d->setWorkFunction([&]
        {
            dSawCount.store(completedBeforeD.load());
            dRan.store(true);
        });

        TaskGraph::TaskGroup group("ABC");
        group.addTask(a);
        group.addTask(b);
        group.addTask(c);

        TEST_ASSERT(d->addDependency(group));

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
        TEST_ASSERT(dRan.load());
        TEST_ASSERT(dSawCount.load() == 3);
    }
};

TEST_INSTANTIATE(TST_TaskGroup);
