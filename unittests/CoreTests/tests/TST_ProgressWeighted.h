#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <QObject>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>

class TST_ProgressWeighted : public UnitTest::Test
{
    TEST_CLASS(TST_ProgressWeighted)
public:
    TST_ProgressWeighted()
        : Test("TST_ProgressWeighted")
    {
        ADD_TEST(TST_ProgressWeighted::weightedProgress);
    }

private:
    TEST_FUNCTION(weightedProgress)
    {
        TEST_START;

        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");
        auto c = std::make_shared<TaskGraph::Task>("C");
        a->setWeight(1.0f);
        b->setWeight(2.0f);
        c->setWeight(7.0f);

        std::atomic<bool> abGate{true};
        std::atomic<bool> cDone{false};
        a->setWorkFunction([&] {
            while (abGate.load()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        });
        b->setWorkFunction([&] {
            while (abGate.load()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        });
        c->setWorkFunction([&] { cDone = true; });

        TaskGraph::TaskScheduler scheduler(4);
        TEST_ASSERT(scheduler.addTask(a));
        TEST_ASSERT(scheduler.addTask(b));
        TEST_ASSERT(scheduler.addTask(c));

        std::atomic<float> lastF{-1.0f};
        QObject::connect(&scheduler, &TaskGraph::TaskScheduler::progressChangedF,
            [&](float f) { lastF.store(f); });

        scheduler.runTasksAsync();

        // Wait for C's completion signal to reach us while A and B are gated.
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (cDone.load() && std::fabs(lastF.load() - 0.7f) < 1e-3f)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        TEST_ASSERT(cDone.load());
        TEST_ASSERT(std::fabs(lastF.load() - 0.7f) < 1e-3f);

        abGate.store(false);

        deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (scheduler.isRunning() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

        TEST_ASSERT(!scheduler.isRunning());
        TEST_ASSERT(a->isDone());
        TEST_ASSERT(b->isDone());
        TEST_ASSERT(c->isDone());
        TEST_ASSERT(std::fabs(lastF.load() - 1.0f) < 1e-4f);
    }
};

TEST_INSTANTIATE(TST_ProgressWeighted);
