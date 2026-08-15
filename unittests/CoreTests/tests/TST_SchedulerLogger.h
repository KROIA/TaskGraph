#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include "LogObject.h"
#include <memory>

class TST_SchedulerLogger : public UnitTest::Test
{
    TEST_CLASS(TST_SchedulerLogger)
public:
    TST_SchedulerLogger()
        : Test("TST_SchedulerLogger")
    {
        ADD_TEST(TST_SchedulerLogger::defaultNoLogger);
        ADD_TEST(TST_SchedulerLogger::injectedLoggerUsed);
        ADD_TEST(TST_SchedulerLogger::threadCountStillWorks);
    }

private:
    TEST_FUNCTION(defaultNoLogger)
    {
        TEST_START;

        TaskGraph::TaskScheduler sched;
        // No logger provided: logger() returns the shared disabled sink.
        TEST_ASSERT(sched.logger().isEnabled() == false);
    }

    TEST_FUNCTION(injectedLoggerUsed)
    {
        TEST_START;

        Log::LogObject ext("run");
        TaskGraph::TaskScheduler sched(1, &ext);

        // The injected logger is used verbatim (identity), scheduler does not own it.
        TEST_ASSERT(&sched.logger() == &ext);
        TEST_ASSERT(sched.logger().isEnabled() == true);
    }

    TEST_FUNCTION(threadCountStillWorks)
    {
        TEST_START;

        TaskGraph::TaskScheduler sched(2);
        // Single-arg thread-count ctor still works; no logger => disabled sink.
        TEST_ASSERT(sched.logger().isEnabled() == false);
    }
};

TEST_INSTANTIATE(TST_SchedulerLogger);
