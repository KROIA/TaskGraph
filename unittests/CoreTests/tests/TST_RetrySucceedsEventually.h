#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>

class TST_RetrySucceedsEventually : public UnitTest::Test
{
    TEST_CLASS(TST_RetrySucceedsEventually)
public:
    TST_RetrySucceedsEventually()
        : Test("TST_RetrySucceedsEventually")
    {
        ADD_TEST(TST_RetrySucceedsEventually::retryThenSucceed);
    }

private:
    TEST_FUNCTION(retryThenSucceed)
    {
        TEST_START;

        auto t = std::make_shared<TaskGraph::Task>("Flaky");
        t->setMaxRetries(2);
        t->setRetryBackoff(std::chrono::milliseconds(5));

        std::atomic<int> attempts{0};
        t->setWorkFunction([&] {
            int n = ++attempts;
            if (n < 3)
                throw std::runtime_error("not yet");
        });

        TaskGraph::TaskScheduler scheduler(2);
        scheduler.setFailurePolicy(TaskGraph::TaskScheduler::FailurePolicy::FailFast);
        TEST_ASSERT(scheduler.addTask(t));

        scheduler.runTasks();

        TEST_ASSERT(attempts.load() == 3);
        TEST_ASSERT(t->isDone());
    }
};

TEST_INSTANTIATE(TST_RetrySucceedsEventually);
