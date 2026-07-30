#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include "LogObject.h"
#include <memory>

class TST_LoggerPerTask : public UnitTest::Test
{
    TEST_CLASS(TST_LoggerPerTask)
public:
    TST_LoggerPerTask()
        : Test("TST_LoggerPerTask")
    {
        ADD_TEST(TST_LoggerPerTask::perTaskLoggerName);
        ADD_TEST(TST_LoggerPerTask::renameFollowsTask);
        ADD_TEST(TST_LoggerPerTask::ctxLogUsable);
    }

private:
    TEST_FUNCTION(perTaskLoggerName)
    {
        TEST_START;

        auto a = std::make_shared<TaskGraph::Task>("Alpha");
        auto b = std::make_shared<TaskGraph::Task>("Beta");

        TEST_ASSERT(a->logger().getName() == "Alpha");
        TEST_ASSERT(b->logger().getName() == "Beta");
        TEST_ASSERT(a->logger().getName() == a->getName());
        TEST_ASSERT(b->logger().getName() == b->getName());
    }

    TEST_FUNCTION(renameFollowsTask)
    {
        TEST_START;

        auto a = std::make_shared<TaskGraph::Task>("First");
        a->setName("Second");
        TEST_ASSERT(a->getName() == "Second");
        TEST_ASSERT(a->logger().getName() == "Second");
    }

    TEST_FUNCTION(ctxLogUsable)
    {
        TEST_START;

        auto a = std::make_shared<TaskGraph::Task>("Logger.Ctx");
        std::string seen;
        a->setWorkFunction([&](TaskGraph::TaskContext& ctx) {
            // Simply exercise the accessor; a log call must not crash.
            ctx.log().logInfo("hello from task body");
            seen = ctx.log().getName();
        });

        TaskGraph::TaskScheduler scheduler(1);
        TEST_ASSERT(scheduler.addTask(a));
        scheduler.runTasks();
        TEST_ASSERT(a->isDone());
        TEST_ASSERT(seen == "Logger.Ctx");
    }
};

TEST_INSTANTIATE(TST_LoggerPerTask);
