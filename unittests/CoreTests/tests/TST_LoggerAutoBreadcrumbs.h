#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include "LogObject.h"
#include <atomic>
#include <memory>
#include <stdexcept>

// Pragmatic check: run a small graph, one task fails. Assert final task states.
// Also assert the loggers of each task carry the task's name — indirect
// confirmation the scheduler routed breadcrumbs via the per-task logger.
class TST_LoggerAutoBreadcrumbs : public UnitTest::Test
{
    TEST_CLASS(TST_LoggerAutoBreadcrumbs)
public:
    TST_LoggerAutoBreadcrumbs()
        : Test("TST_LoggerAutoBreadcrumbs")
    {
        ADD_TEST(TST_LoggerAutoBreadcrumbs::breadcrumbsFire);
    }

private:
    TEST_FUNCTION(breadcrumbsFire)
    {
        TEST_START;

        auto a = std::make_shared<TaskGraph::Task>("BC.A");
        auto b = std::make_shared<TaskGraph::Task>("BC.B");
        auto c = std::make_shared<TaskGraph::Task>("BC.C");

        std::atomic<int> ran{0};
        a->setWorkFunction([&] { ++ran; });
        b->setWorkFunction([&] { ++ran; });
        c->setWorkFunction([&] { throw std::runtime_error("boom"); });

        TEST_ASSERT(b->addDependency(a));

        TaskGraph::TaskScheduler scheduler(2);
        scheduler.setFailurePolicy(TaskGraph::TaskScheduler::FailurePolicy::ContinueOthers);
        TEST_ASSERT(scheduler.addTask(a));
        TEST_ASSERT(scheduler.addTask(b));
        TEST_ASSERT(scheduler.addTask(c));

        scheduler.runTasks();

        TEST_ASSERT(a->getStatus() == TaskGraph::Task::Status::Done);
        TEST_ASSERT(b->getStatus() == TaskGraph::Task::Status::Done);
        TEST_ASSERT(c->getStatus() == TaskGraph::Task::Status::Failed);

        TEST_ASSERT(a->logger().getName() == "BC.A");
        TEST_ASSERT(b->logger().getName() == "BC.B");
        TEST_ASSERT(c->logger().getName() == "BC.C");
    }
};

TEST_INSTANTIATE(TST_LoggerAutoBreadcrumbs);
