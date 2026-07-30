#pragma once

#include "TaskGraph_global.h"
#include "gui/ITaskGraphComponentFactory.h"
#include <memory>
#include <QWidget>

namespace TaskGraph
{
    class TaskScheduler;

namespace Gui
{
    class TaskGraphScene;
    class TaskGraphView;
    class SchedulerControlBar;

    class TASK_GRAPH_API TaskGraphWidget : public QWidget
    {
        Q_OBJECT
    public:
        explicit TaskGraphWidget(
            TaskScheduler* scheduler,
            std::shared_ptr<ITaskGraphComponentFactory> factory = nullptr,
            QWidget* parent = nullptr);

    private:
        TaskScheduler* m_scheduler;
        std::shared_ptr<ITaskGraphComponentFactory> m_factory;
        TaskGraphScene* m_scene = nullptr;
        TaskGraphView* m_view = nullptr;
        SchedulerControlBar* m_controlBar = nullptr;
    };
}
}
