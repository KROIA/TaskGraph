#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <QCoreApplication>
#include <QThread>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

class TST_GuiAffinityMarshaling : public UnitTest::Test
{
    TEST_CLASS(TST_GuiAffinityMarshaling)
public:
    TST_GuiAffinityMarshaling()
        : Test("TST_GuiAffinityMarshaling")
    {
        ADD_TEST(TST_GuiAffinityMarshaling::marshalToGui);
    }

private:
    TEST_FUNCTION(marshalToGui)
    {
        TEST_START;

        // Requires the QCoreApplication set up in unittests/CoreTests/main.cpp.
        QCoreApplication* app = QCoreApplication::instance();
        TEST_ASSERT(app != nullptr);
        if (!app)
            return;

        auto t = std::make_shared<TaskGraph::Task>("GuiTask");
        t->setAffinity(TaskGraph::Task::TaskAffinity::Gui);

        std::atomic<bool> ranOnGuiThread{false};
        std::atomic<bool> ranAtAll{false};
        QThread* guiThread = app->thread();
        t->setWorkFunction([&, guiThread] {
            ranAtAll = true;
            ranOnGuiThread = (QThread::currentThread() == guiThread);
        });

        TaskGraph::TaskScheduler scheduler(2);
        TEST_ASSERT(scheduler.addTask(t));

        scheduler.runTasksAsync();

        // Pump the GUI event loop until the task runs and the scheduler unwinds.
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (scheduler.isRunning() && std::chrono::steady_clock::now() < deadline)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        // Flush any lingering queued events.
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

        TEST_ASSERT(!scheduler.isRunning());
        TEST_ASSERT(ranAtAll.load());
        TEST_ASSERT(ranOnGuiThread.load());
        TEST_ASSERT(t->isDone());
    }
};

TEST_INSTANTIATE(TST_GuiAffinityMarshaling);
