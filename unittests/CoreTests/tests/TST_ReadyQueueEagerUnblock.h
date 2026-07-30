#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

class TST_ReadyQueueEagerUnblock : public UnitTest::Test
{
    TEST_CLASS(TST_ReadyQueueEagerUnblock)
public:
    TST_ReadyQueueEagerUnblock()
        : Test("TST_ReadyQueueEagerUnblock")
    {
        ADD_TEST(TST_ReadyQueueEagerUnblock::eagerUnblock);
    }

private:
    TEST_FUNCTION(eagerUnblock)
    {
        TEST_START;
        using clock = std::chrono::steady_clock;

        // Subgraph 1: A(slow) and B(fast) --> C
        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");
        auto c = std::make_shared<TaskGraph::Task>("C");

        // Subgraph 2: D(fast) --> E(fast)
        auto d = std::make_shared<TaskGraph::Task>("D");
        auto e = std::make_shared<TaskGraph::Task>("E");

        std::atomic<long long> tA_end{0}, tE_end{0};
        auto start = clock::now();

        a->setWorkFunction([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            tA_end = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
        });
        b->setWorkFunction([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
        c->setWorkFunction([&] {});
        d->setWorkFunction([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
        e->setWorkFunction([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            tE_end = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
        });

        TEST_ASSERT(c->addDependency(a));
        TEST_ASSERT(c->addDependency(b));
        TEST_ASSERT(e->addDependency(d));

        TaskGraph::TaskScheduler scheduler(4);
        TEST_ASSERT(scheduler.addTask(a));
        TEST_ASSERT(scheduler.addTask(b));
        TEST_ASSERT(scheduler.addTask(c));
        TEST_ASSERT(scheduler.addTask(d));
        TEST_ASSERT(scheduler.addTask(e));

        scheduler.runTasks();

        TEST_ASSERT(a->isDone());
        TEST_ASSERT(b->isDone());
        TEST_ASSERT(c->isDone());
        TEST_ASSERT(d->isDone());
        TEST_ASSERT(e->isDone());

        // E should have finished well before A did — proves fast chain didn't wait
        // on the slow one in an unrelated subgraph.
        TEST_ASSERT(tE_end.load() > 0);
        TEST_ASSERT(tA_end.load() > 0);
        TEST_ASSERT(tE_end.load() < tA_end.load());
    }
};

TEST_INSTANTIATE(TST_ReadyQueueEagerUnblock);
