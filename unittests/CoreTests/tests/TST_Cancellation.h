#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <QObject>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

class TST_Cancellation : public UnitTest::Test
{
    TEST_CLASS(TST_Cancellation)
public:
    TST_Cancellation()
        : Test("TST_Cancellation")
    {
        ADD_TEST(TST_Cancellation::cooperativeCancel);
    }

private:
    TEST_FUNCTION(cooperativeCancel)
    {
        TEST_START;

        auto t = std::make_shared<TaskGraph::Task>("Loop");
        std::atomic<bool> entered{false};
        std::atomic<bool> exitedEarly{false};
        t->setWorkFunction([&] {
            entered = true;
            for (int i = 0; i < 10000; ++i)
            {
                if (t->isCancelRequested())
                {
                    exitedEarly = true;
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });

        TaskGraph::TaskScheduler scheduler(2);
        TEST_ASSERT(scheduler.addTask(t));

        std::atomic<bool> cancelledSignalFired{false};
        QObject::connect(&scheduler, &TaskGraph::TaskScheduler::cancelled,
            [&] { cancelledSignalFired = true; });

        scheduler.runTasksAsync();

        // Wait for the body to start.
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!entered.load() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        TEST_ASSERT(entered.load());

        scheduler.cancel();

        // Wait for the run to wind down.
        deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (scheduler.isRunning() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

        TEST_ASSERT(!scheduler.isRunning());
        TEST_ASSERT(exitedEarly.load());
        TEST_ASSERT(t->getStatus() == TaskGraph::Task::Status::Cancelled);
        TEST_ASSERT(cancelledSignalFired.load());
    }
};

TEST_INSTANTIATE(TST_Cancellation);
