#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <memory>

class TST_CycleRejected : public UnitTest::Test
{
    TEST_CLASS(TST_CycleRejected)
public:
    TST_CycleRejected()
        : Test("TST_CycleRejected")
    {
        ADD_TEST(TST_CycleRejected::rejectCycle);
    }

private:
    TEST_FUNCTION(rejectCycle)
    {
        TEST_START;
        auto a = std::make_shared<TaskGraph::Task>("A");
        auto b = std::make_shared<TaskGraph::Task>("B");

        TEST_ASSERT(b->addDependency(a));
        // Adding A->B when B->A already exists must be rejected at edge-add time,
        // OR runTasks must report dependencyGraphNotDAG.
        bool addOk = a->addDependency(b);
        if (addOk)
        {
            TaskGraph::TaskScheduler scheduler(2);
            scheduler.addTask(a);
            scheduler.addTask(b);
            scheduler.runTasks();
            TEST_ASSERT(scheduler.getLastError() == TaskGraph::TaskScheduler::Error::dependencyGraphNotDAG);
        }
        else
        {
            TEST_ASSERT(!addOk);
        }
    }
};

TEST_INSTANTIATE(TST_CycleRejected);
