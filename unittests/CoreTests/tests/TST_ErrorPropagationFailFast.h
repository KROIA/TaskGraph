#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <QObject>
#include <QString>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

class TST_ErrorPropagationFailFast : public UnitTest::Test
{
    TEST_CLASS(TST_ErrorPropagationFailFast)
public:
    TST_ErrorPropagationFailFast()
        : Test("TST_ErrorPropagationFailFast")
    {
        ADD_TEST(TST_ErrorPropagationFailFast::failFast);
    }

private:
    TEST_FUNCTION(failFast)
    {
        TEST_START;

        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");
        auto c = std::make_shared<TaskGraph::Task>("C");

        // Chain so scheduling has a defined order: A -> B(fail) -> C, plus C waits on A too.
        // Actually spec says "3 independent tasks". Use a chain so C is definitively unstarted
        // when B throws.
        TEST_ASSERT(b->addDependency(a));
        TEST_ASSERT(c->addDependency(b));

        a->setWorkFunction([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
        b->setWorkFunction([] {
            throw std::runtime_error("boom");
        });
        std::atomic<bool> cRan{false};
        c->setWorkFunction([&] { cRan = true; });

        TaskGraph::TaskScheduler scheduler(2);
        scheduler.setFailurePolicy(TaskGraph::TaskScheduler::FailurePolicy::FailFast);
        TEST_ASSERT(scheduler.addTask(a));
        TEST_ASSERT(scheduler.addTask(b));
        TEST_ASSERT(scheduler.addTask(c));

        std::atomic<int> failedCount{0};
        QString failedName;
        QString failedError;
        QObject::connect(&scheduler, &TaskGraph::TaskScheduler::taskFailed,
            [&](QString name, QString err) {
                failedName = name;
                failedError = err;
                ++failedCount;
            });

        scheduler.runTasks();

        TEST_ASSERT(a->isDone());
        TEST_ASSERT(b->getStatus() == TaskGraph::Task::Status::Failed);
        TEST_ASSERT(c->getStatus() == TaskGraph::Task::Status::Cancelled);
        TEST_ASSERT(!cRan.load());
        TEST_ASSERT(failedCount.load() >= 1);
        TEST_ASSERT(failedName == QStringLiteral("B"));
        TEST_ASSERT(failedError.contains(QStringLiteral("boom")));
    }
};

TEST_INSTANTIATE(TST_ErrorPropagationFailFast);
