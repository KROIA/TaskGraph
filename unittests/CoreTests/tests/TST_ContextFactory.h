#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>

// App-supplied derived execution context: carries extra per-run state (tag) and
// optionally instruments its own destruction.
struct MyCtx : TaskGraph::TaskContext
{
    int tag;
    std::atomic<int>* dtorCount;

    MyCtx(TaskGraph::Task* task, TaskGraph::TaskScheduler* scheduler, int t,
          std::atomic<int>* dc = nullptr)
        : TaskGraph::TaskContext(task, scheduler)
        , tag(t)
        , dtorCount(dc)
    {}

    ~MyCtx() override
    {
        if (dtorCount)
            dtorCount->fetch_add(1);
    }
};

class TST_ContextFactory : public UnitTest::Test
{
    TEST_CLASS(TST_ContextFactory)
public:
    TST_ContextFactory()
        : Test("TST_ContextFactory")
    {
        ADD_TEST(TST_ContextFactory::defaultContextStillWorks);
        ADD_TEST(TST_ContextFactory::factoryContextInjectedAndBaseWorks);
        ADD_TEST(TST_ContextFactory::factoryContextReusedAcrossRetries);
        ADD_TEST(TST_ContextFactory::virtualDtorRunsOncePerRun);
    }

private:
    // No factory set -> body still gets a working base TaskContext.
    TEST_FUNCTION(defaultContextStillWorks)
    {
        TEST_START;

        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");

        a->setWorkFunction([](TaskGraph::TaskContext& ctx) {
            ctx.setResult(7);
        });

        std::atomic<int> saw{-1};
        std::atomic<bool> cancelSeen{true};
        b->setWorkFunction([a, &saw, &cancelSeen](TaskGraph::TaskContext& ctx) {
            saw = ctx.getDependencyResult<int>(*a);
            cancelSeen = ctx.isCancelRequested();
        });

        TEST_ASSERT(b->addDependency(a));

        TaskGraph::TaskScheduler scheduler(2);
        TEST_ASSERT(scheduler.addTask(a));
        TEST_ASSERT(scheduler.addTask(b));

        scheduler.runTasks();

        TEST_ASSERT(a->isDone());
        TEST_ASSERT(b->isDone());
        TEST_ASSERT(saw.load() == 7);
        TEST_ASSERT(cancelSeen.load() == false);
    }

    // Factory set -> body receives derived context; tag injected AND base
    // facilities (getDependencyResult, setResult, isCancelRequested) work.
    TEST_FUNCTION(factoryContextInjectedAndBaseWorks)
    {
        TEST_START;

        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");

        a->setWorkFunction([](TaskGraph::TaskContext& ctx) {
            ctx.setResult(100);
        });

        std::atomic<int> sawTag{-1};
        std::atomic<int> sawDep{-1};
        std::atomic<bool> sawCancel{true};
        b->setWorkFunction([a, &sawTag, &sawDep, &sawCancel](TaskGraph::TaskContext& ctx) {
            MyCtx& mc = static_cast<MyCtx&>(ctx);
            sawTag = mc.tag;
            sawDep = ctx.getDependencyResult<int>(*a);
            sawCancel = ctx.isCancelRequested();
            ctx.setResult(mc.tag * 2);
        });

        TEST_ASSERT(b->addDependency(a));

        TaskGraph::TaskScheduler scheduler(2);
        scheduler.setContextFactory([](TaskGraph::Task* task, TaskGraph::TaskScheduler* sched) {
            return std::make_unique<MyCtx>(task, sched, 55);
        });
        TEST_ASSERT(scheduler.addTask(a));
        TEST_ASSERT(scheduler.addTask(b));

        scheduler.runTasks();

        TEST_ASSERT(a->isDone());
        TEST_ASSERT(b->isDone());
        TEST_ASSERT(sawTag.load() == 55);
        TEST_ASSERT(sawDep.load() == 100);
        TEST_ASSERT(sawCancel.load() == false);
        TEST_ASSERT(TaskGraph::getResultAs<int>(*b) == 110);
    }

    // Same factory-built context (same injected tag) is visible on every retry.
    TEST_FUNCTION(factoryContextReusedAcrossRetries)
    {
        TEST_START;

        auto t = std::make_shared<TaskGraph::Task>("Flaky");
        t->setMaxRetries(2);
        t->setRetryBackoff(std::chrono::milliseconds(5));

        std::atomic<int> attempts{0};
        std::atomic<bool> tagStable{true};
        t->setWorkFunction([&](TaskGraph::TaskContext& ctx) {
            MyCtx& mc = static_cast<MyCtx&>(ctx);
            if (mc.tag != 77)
                tagStable = false;
            int n = ++attempts;
            if (n < 3)
                throw std::runtime_error("not yet");
        });

        TaskGraph::TaskScheduler scheduler(2);
        scheduler.setFailurePolicy(TaskGraph::TaskScheduler::FailurePolicy::FailFast);
        scheduler.setContextFactory([](TaskGraph::Task* task, TaskGraph::TaskScheduler* sched) {
            return std::make_unique<MyCtx>(task, sched, 77);
        });
        TEST_ASSERT(scheduler.addTask(t));

        scheduler.runTasks();

        TEST_ASSERT(attempts.load() == 3);
        TEST_ASSERT(tagStable.load());
        TEST_ASSERT(t->isDone());
    }

    // Virtual dtor: derived context destroyed exactly once per task-run, even
    // across a multi-attempt retry sequence (one context per run).
    TEST_FUNCTION(virtualDtorRunsOncePerRun)
    {
        TEST_START;

        std::atomic<int> dtorCount{0};

        auto t = std::make_shared<TaskGraph::Task>("Flaky");
        t->setMaxRetries(2);
        t->setRetryBackoff(std::chrono::milliseconds(1));

        std::atomic<int> attempts{0};
        t->setWorkFunction([&](TaskGraph::TaskContext& ctx) {
            int n = ++attempts;
            if (n < 2)
                throw std::runtime_error("retry once");
        });

        TaskGraph::TaskScheduler scheduler(2);
        scheduler.setFailurePolicy(TaskGraph::TaskScheduler::FailurePolicy::FailFast);
        scheduler.setContextFactory([&dtorCount](TaskGraph::Task* task, TaskGraph::TaskScheduler* sched) {
            return std::make_unique<MyCtx>(task, sched, 1, &dtorCount);
        });
        TEST_ASSERT(scheduler.addTask(t));

        scheduler.runTasks();

        TEST_ASSERT(t->isDone());
        TEST_ASSERT(attempts.load() == 2);
        TEST_ASSERT(dtorCount.load() == 1);
    }
};

TEST_INSTANTIATE(TST_ContextFactory);
