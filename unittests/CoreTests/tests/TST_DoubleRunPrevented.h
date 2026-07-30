#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

class TST_DoubleRunPrevented : public UnitTest::Test
{
    TEST_CLASS(TST_DoubleRunPrevented)
public:
    TST_DoubleRunPrevented()
        : Test("TST_DoubleRunPrevented")
    {
        ADD_TEST(TST_DoubleRunPrevented::concurrentRunTask);
    }

private:
    TEST_FUNCTION(concurrentRunTask)
    {
        TEST_START;
        std::atomic<int> counter{0};
        auto t = std::make_shared<TaskGraph::Task>("T");
        t->setWorkFunction([&]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ++counter;
        });

        constexpr int nThreads = 8;
        std::vector<std::thread> threads;
        threads.reserve(nThreads);
        std::atomic<bool> go{false};
        for (int i = 0; i < nThreads; ++i)
        {
            threads.emplace_back([&]
            {
                while (!go.load(std::memory_order_acquire)) { /* spin */ }
                t->runTask();
            });
        }
        go.store(true, std::memory_order_release);
        for (auto& th : threads)
            th.join();

        TEST_ASSERT(counter.load() == 1);
        TEST_ASSERT(t->isDone());
    }
};

TEST_INSTANTIATE(TST_DoubleRunPrevented);
