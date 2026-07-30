#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

class TST_ErrorPropagationContinueOthers : public UnitTest::Test
{
    TEST_CLASS(TST_ErrorPropagationContinueOthers)
public:
    TST_ErrorPropagationContinueOthers()
        : Test("TST_ErrorPropagationContinueOthers")
    {
        ADD_TEST(TST_ErrorPropagationContinueOthers::continueOthers);
    }

private:
    TEST_FUNCTION(continueOthers)
    {
        TEST_START;

        // Subgraph 1: A(fail) -> B (should be Skipped)
        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");

        // Subgraph 2: C -> D (independent; both should complete)
        auto c = std::make_shared<TaskGraph::Task>("C");
        auto d = std::make_shared<TaskGraph::Task>("D");

        TEST_ASSERT(b->addDependency(a));
        TEST_ASSERT(d->addDependency(c));

        a->setWorkFunction([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            throw std::runtime_error("nope");
        });
        std::atomic<bool> bRan{false};
        b->setWorkFunction([&] { bRan = true; });
        c->setWorkFunction([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
        std::atomic<bool> dRan{false};
        d->setWorkFunction([&] { dRan = true; });

        TaskGraph::TaskScheduler scheduler(4);
        scheduler.setFailurePolicy(TaskGraph::TaskScheduler::FailurePolicy::ContinueOthers);
        TEST_ASSERT(scheduler.addTask(a));
        TEST_ASSERT(scheduler.addTask(b));
        TEST_ASSERT(scheduler.addTask(c));
        TEST_ASSERT(scheduler.addTask(d));

        scheduler.runTasks();

        TEST_ASSERT(a->getStatus() == TaskGraph::Task::Status::Failed);
        TEST_ASSERT(b->getStatus() == TaskGraph::Task::Status::Skipped);
        TEST_ASSERT(!bRan.load());
        TEST_ASSERT(c->isDone());
        TEST_ASSERT(d->isDone());
        TEST_ASSERT(dRan.load());
    }
};

TEST_INSTANTIATE(TST_ErrorPropagationContinueOthers);
