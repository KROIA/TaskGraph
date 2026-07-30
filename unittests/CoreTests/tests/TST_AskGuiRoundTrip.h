#pragma once

#include "UnitTest.h"
#include "TaskGraph.h"
#include <QCoreApplication>
#include <QObject>
#include <QVariant>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

class TST_AskGuiRoundTrip : public UnitTest::Test
{
    TEST_CLASS(TST_AskGuiRoundTrip)
public:
    TST_AskGuiRoundTrip()
        : Test("TST_AskGuiRoundTrip")
    {
        ADD_TEST(TST_AskGuiRoundTrip::roundTrip);
    }

private:
    TEST_FUNCTION(roundTrip)
    {
        TEST_START;

        QCoreApplication* app = QCoreApplication::instance();
        TEST_ASSERT(app != nullptr);
        if (!app) return;

        auto t = std::make_shared<TaskGraph::Task>("AskGui");
        t->setWorkFunction([](TaskGraph::TaskContext& ctx) {
            QVariant r = ctx.askGui(QVariant(QStringLiteral("prompt")));
            ctx.setResult(r.toInt());
        });

        TaskGraph::TaskScheduler scheduler(2);
        TEST_ASSERT(scheduler.addTask(t));

        QObject::connect(&scheduler, &TaskGraph::TaskScheduler::guiEventRequested,
            [&scheduler](int id, QString /*name*/, QVariant /*payload*/) {
                scheduler.respondToGuiEvent(id, QVariant(42));
            });

        scheduler.runTasksAsync();

        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (scheduler.isRunning() && std::chrono::steady_clock::now() < deadline)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

        TEST_ASSERT(!scheduler.isRunning());
        TEST_ASSERT(t->isDone());
        std::any r = t->getResult();
        TEST_ASSERT(r.has_value());
        TEST_ASSERT(std::any_cast<int>(r) == 42);
    }
};

TEST_INSTANTIATE(TST_AskGuiRoundTrip);
