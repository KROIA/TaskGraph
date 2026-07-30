#pragma once

#include "TaskGraph_global.h"
#include "gui/FeatureSet.h"
#include <memory>

class QObject;
class QWidget;

namespace TaskGraph
{
    class TaskScheduler;

namespace Gui
{
    class TaskGraphScene;
    class TaskGraphView;
    class SchedulerControlBar;
    class TaskLogBuffer;
    class TaskLogOverlay;
    class GuiPromptService;

    class TASK_GRAPH_API ITaskGraphComponentFactory
    {
    public:
        virtual ~ITaskGraphComponentFactory() = default;
        virtual const FeatureSet& features() const = 0;
        virtual TaskGraphScene* createScene(TaskScheduler* scheduler, QObject* parent) = 0;
        virtual TaskGraphView* createView(TaskScheduler* scheduler, QWidget* parent) = 0;
        virtual SchedulerControlBar* createControlBar(TaskScheduler* scheduler, QWidget* parent) = 0;
        virtual QWidget* createInspector(TaskScheduler* scheduler, QWidget* parent) = 0;
        virtual QWidget* createLogView(TaskScheduler* scheduler, TaskLogBuffer* logBuffer, QWidget* parent) = 0;
        virtual TaskLogOverlay* createLogOverlay(TaskLogBuffer* logBuffer, QWidget* parent) = 0;
        virtual GuiPromptService* createGuiPromptService(TaskScheduler* scheduler, QWidget* parentWidget, QObject* parent) = 0;
    };

    TASK_GRAPH_API std::shared_ptr<ITaskGraphComponentFactory> presetViewOnlyFactory();
    TASK_GRAPH_API std::shared_ptr<ITaskGraphComponentFactory> presetMonitorFactory();
    TASK_GRAPH_API std::shared_ptr<ITaskGraphComponentFactory> presetEditorFactory();
    TASK_GRAPH_API std::shared_ptr<ITaskGraphComponentFactory> makeFactory(FeatureSet features);
}
}
