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
    class TaskInspectorPanel;
    class TaskLogOverlay;
    class TaskLogBuffer;
    class GuiPromptService;

    class TASK_GRAPH_API TaskGraphWidget : public QWidget
    {
        Q_OBJECT
    public:
        explicit TaskGraphWidget(
            TaskScheduler* scheduler,
            std::shared_ptr<ITaskGraphComponentFactory> factory = nullptr,
            QWidget* parent = nullptr);

    private slots:
        void onNodeSelected(const QString& taskName);
        void onGraphStructureChanged();

    private:
        void registerTaskLoggers();
        void setUiIdle(bool idle);

        TaskScheduler* m_scheduler;
        std::shared_ptr<ITaskGraphComponentFactory> m_factory;
        TaskGraphScene* m_scene = nullptr;
        TaskGraphView* m_view = nullptr;
        SchedulerControlBar* m_controlBar = nullptr;
        TaskInspectorPanel* m_inspector = nullptr;
        QWidget* m_logView = nullptr;
        TaskLogOverlay* m_logOverlay = nullptr;
        TaskLogBuffer* m_logBuffer = nullptr;
        GuiPromptService* m_promptService = nullptr;
        bool m_idle = true;
    };
}
}
