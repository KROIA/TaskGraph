#include "gui/TaskGraphWidget.h"

#if defined(QT_WIDGETS_ENABLED)
#include "gui/TaskGraphScene.h"
#include "gui/TaskGraphView.h"
#include "gui/SchedulerControlBar.h"
#include "TaskScheduler.h"
#include <QVBoxLayout>

namespace TaskGraph
{
namespace Gui
{
    TaskGraphWidget::TaskGraphWidget(
        TaskScheduler* scheduler,
        std::shared_ptr<ITaskGraphComponentFactory> factory,
        QWidget* parent)
        : QWidget(parent)
        , m_scheduler(scheduler)
        , m_factory(factory ? factory : presetMonitorFactory())
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        m_controlBar = m_factory->createControlBar(m_scheduler, this);
        if (m_controlBar)
            layout->addWidget(m_controlBar);

        m_scene = new TaskGraphScene(m_scheduler, this);
        m_view = m_factory->createView(m_scheduler, this);
        if (m_view)
        {
            m_view->setScene(m_scene);
            layout->addWidget(m_view, 1);
        }

        m_scene->rebuild();

        // Wire scheduler signals -> scene updates (auto/queued connections for thread safety)
        connect(m_scheduler, &TaskScheduler::taskStarted, this, [this](QString name) {
            m_scene->updateNodeStatus(name, Task::Status::Running);
        });
        connect(m_scheduler, &TaskScheduler::taskFinished, this, [this](QString name) {
            m_scene->updateNodeStatus(name, Task::Status::Done);
        });
        connect(m_scheduler, &TaskScheduler::taskFailed, this, [this](QString name, QString) {
            m_scene->updateNodeStatus(name, Task::Status::Failed);
        });

        connect(m_scheduler, &TaskScheduler::wasReset, this, [this]() {
            m_scene->rebuild();
        });
        connect(m_scheduler, &TaskScheduler::cancelled, this, [this]() {
            // refresh all node colors from actual status
            auto graph = m_scheduler->getTaskGraph();
            for (const auto& layer : graph)
                for (const auto& task : layer)
                    m_scene->updateNodeStatus(
                        QString::fromStdString(task->getName()),
                        task->getStatus());
        });
        connect(m_scheduler, &TaskScheduler::completed, this, [this]() {
            auto graph = m_scheduler->getTaskGraph();
            for (const auto& layer : graph)
                for (const auto& task : layer)
                    m_scene->updateNodeStatus(
                        QString::fromStdString(task->getName()),
                        task->getStatus());
        });
        connect(m_scheduler, &TaskScheduler::started, this, [this]() {
            m_scene->rebuild();
        });
    }
}
}

#endif
