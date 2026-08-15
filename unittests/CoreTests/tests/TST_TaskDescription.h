#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <memory>

class TST_TaskDescription : public UnitTest::Test
{
    TEST_CLASS(TST_TaskDescription)
public:
    TST_TaskDescription()
        : Test("TST_TaskDescription")
    {
        ADD_TEST(TST_TaskDescription::defaultEmptyAndRoundTrip);
    }

private:
    TEST_FUNCTION(defaultEmptyAndRoundTrip)
    {
        TEST_START;

        auto task = std::make_shared<TaskGraph::Task>("A");

        TEST_ASSERT(task->getDescription().empty());

        task->setDescription("x");
        TEST_ASSERT(task->getDescription() == "x");
    }
};

TEST_INSTANTIATE(TST_TaskDescription);
