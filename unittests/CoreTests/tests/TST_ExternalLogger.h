#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include "LogObject.h"
#include <memory>

class TST_ExternalLogger : public UnitTest::Test
{
    TEST_CLASS(TST_ExternalLogger)
public:
    TST_ExternalLogger()
        : Test("TST_ExternalLogger")
    {
        ADD_TEST(TST_ExternalLogger::externalRoutesLogger);
        ADD_TEST(TST_ExternalLogger::nullptrMeansNoLogger);
        ADD_TEST(TST_ExternalLogger::noneModeSharedSink);
        ADD_TEST(TST_ExternalLogger::noneModeNoLogs);
        ADD_TEST(TST_ExternalLogger::ctxLogUsesExternal);
        ADD_TEST(TST_ExternalLogger::ownLoggerStable);
    }

private:
    TEST_FUNCTION(externalRoutesLogger)
    {
        TEST_START;

        Log::LogObject ext("ExternalLogger");
        auto a = std::make_shared<TaskGraph::Task>("Alpha");

        a->setExternalLogger(&ext);
        TEST_ASSERT(a->hasExternalLogger());
        TEST_ASSERT(&a->logger() == &ext);
    }

    TEST_FUNCTION(nullptrMeansNoLogger)
    {
        TEST_START;

        Log::LogObject ext("ExternalLogger");
        auto a = std::make_shared<TaskGraph::Task>("Beta");
        auto own = std::make_shared<TaskGraph::Task>("BetaOwn");

        a->setExternalLogger(&ext);
        TEST_ASSERT(a->hasExternalLogger());

        // nullptr now switches to None mode (shared disabled sink), NOT back to own.
        a->setExternalLogger(nullptr);
        TEST_ASSERT(!a->hasExternalLogger());
        TEST_ASSERT(&a->logger() != &ext);
        TEST_ASSERT(&a->logger() != &own->logger());
    }

    TEST_FUNCTION(noneModeSharedSink)
    {
        TEST_START;

        auto t1 = std::make_shared<TaskGraph::Task>("None1");
        auto t2 = std::make_shared<TaskGraph::Task>("None2");
        auto own = std::make_shared<TaskGraph::Task>("OwnNode");

        t1->setExternalLogger(nullptr);
        t2->setExternalLogger(nullptr);

        // Both None-mode tasks share the single process-wide sink.
        TEST_ASSERT(&t1->logger() == &t2->logger());
        TEST_ASSERT(&t1->logger() != &own->logger());
    }

    TEST_FUNCTION(noneModeNoLogs)
    {
        TEST_START;

        auto a = std::make_shared<TaskGraph::Task>("None.Body");
        a->setExternalLogger(nullptr);

        Log::LogObject* seen = nullptr;
        a->setWorkFunction([&](TaskGraph::TaskContext& ctx) {
            ctx.log().logInfo("should be suppressed");
            seen = &ctx.log();
        });

        TaskGraph::TaskScheduler scheduler(1);
        TEST_ASSERT(scheduler.addTask(a));
        scheduler.runTasks();
        TEST_ASSERT(a->isDone());
        TEST_ASSERT(seen == &a->logger());
        TEST_ASSERT(!a->logger().isEnabled());
    }

    TEST_FUNCTION(ctxLogUsesExternal)
    {
        TEST_START;

        Log::LogObject ext("ExternalLogger");
        auto a = std::make_shared<TaskGraph::Task>("Ctx.External");
        a->setExternalLogger(&ext);

        Log::LogObject* seen = nullptr;
        a->setWorkFunction([&](TaskGraph::TaskContext& ctx) {
            seen = &ctx.log();
        });

        TaskGraph::TaskScheduler scheduler(1);
        TEST_ASSERT(scheduler.addTask(a));
        scheduler.runTasks();
        TEST_ASSERT(a->isDone());
        TEST_ASSERT(seen == &ext);
    }

    TEST_FUNCTION(ownLoggerStable)
    {
        TEST_START;

        auto a = std::make_shared<TaskGraph::Task>("Gamma");
        TEST_ASSERT(!a->hasExternalLogger());
        TEST_ASSERT(&a->logger() == &a->logger());
        TEST_ASSERT(a->logger().getName() == "Gamma");
    }
};

TEST_INSTANTIATE(TST_ExternalLogger);
