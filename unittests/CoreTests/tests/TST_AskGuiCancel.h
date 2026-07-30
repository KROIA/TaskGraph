#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <QCoreApplication>
#include <QVariant>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

class TST_AskGuiCancel : public UnitTest::Test
{
    TEST_CLASS(TST_AskGuiCancel)
public:
    TST_AskGuiCancel()
        : Test("TST_AskGuiCancel")
    {
        ADD_TEST(TST_AskGuiCancel::cancelWhileWaiting);
    }

private:
    TEST_FUNCTION(cancelWhileWaiting)
    {
        TEST_START;

        QCoreApplication* app = QCoreApplication::instance();
        TEST_ASSERT(app != nullptr);
        if (!app) return;

        auto t = std::make_shared<TaskGraph::Task>("AskGuiCancel");
        std::atomic<bool> entered{false};
        std::atomic<bool> exited{false};
        std::atomic<bool> gotInvalid{false};
        t->setWorkFunction([&](TaskGraph::TaskContext& ctx) {
            entered = true;
            QVariant r = ctx.askGui(QVariant(QStringLiteral("prompt")));
            gotInvalid = !r.isValid();
            exited = true;
        });

        TaskGraph::TaskScheduler scheduler(2);
        TEST_ASSERT(scheduler.addTask(t));

        // Intentionally never respond.

        scheduler.runTasksAsync();

        // Pump events so guiEventRequested emission (queued) is delivered — even
        // though no receiver is connected, this exercises the queued signal.
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (!entered.load() && std::chrono::steady_clock::now() < deadline)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        TEST_ASSERT(entered.load());

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        scheduler.cancel();

        deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (scheduler.isRunning() && std::chrono::steady_clock::now() < deadline)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

        TEST_ASSERT(!scheduler.isRunning());
        TEST_ASSERT(exited.load());
        TEST_ASSERT(gotInvalid.load());
        TEST_ASSERT(t->getStatus() == TaskGraph::Task::Status::Cancelled);
    }
};

TEST_INSTANTIATE(TST_AskGuiCancel);
