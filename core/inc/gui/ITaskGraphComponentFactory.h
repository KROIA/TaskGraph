#pragma once

#include "TaskGraph_global.h"
#include "gui/FeatureSet.h"
#include <memory>

class QWidget;

namespace TaskGraph
{
    class TaskScheduler;

namespace Gui
{
    class TaskGraphView;
    class SchedulerControlBar;

    class TASK_GRAPH_API ITaskGraphComponentFactory
    {
    public:
        virtual ~ITaskGraphComponentFactory() = default;
        virtual const FeatureSet& features() const = 0;
        virtual TaskGraphView* createView(TaskScheduler* scheduler, QWidget* parent) = 0;
        virtual SchedulerControlBar* createControlBar(TaskScheduler* scheduler, QWidget* parent) = 0;
        virtual QWidget* createInspector(TaskScheduler* scheduler, QWidget* parent) = 0;
        virtual QWidget* createLogView(TaskScheduler* scheduler, QWidget* parent) = 0;
        virtual QWidget* createGuiPromptService(TaskScheduler* scheduler, QWidget* parent) = 0;
    };

    TASK_GRAPH_API std::shared_ptr<ITaskGraphComponentFactory> presetViewOnlyFactory();
    TASK_GRAPH_API std::shared_ptr<ITaskGraphComponentFactory> presetMonitorFactory();
    TASK_GRAPH_API std::shared_ptr<ITaskGraphComponentFactory> presetEditorFactory();
    TASK_GRAPH_API std::shared_ptr<ITaskGraphComponentFactory> makeFactory(FeatureSet features);
}
}
