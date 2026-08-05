#pragma once

#include "TaskGraph_global.h"
#include "gui/ITaskGraphComponentFactory.h"
#include "gui/GraphVisualConfig.h"
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
    class AggregateTaskLogView;
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

        void setReadOnly(bool ro);
        bool isReadOnly() const { return m_readOnly; }

        void setVisualConfig(const GraphVisualConfig& config);
        const GraphVisualConfig& visualConfig() const { return m_visualConfig; }

    signals:
        void readOnlyChanged(bool);

    private slots:
        void onNodeSelected(const QString& taskName);
        void onNodeDoubleClicked(const QString& taskName);
        void onGraphStructureChanged();

    private:
        void createComponents();
        void buildLayout();
        void wireComponents();
        void wireScheduler();
        void registerTaskLoggers();
        void refreshAllNodeStatuses();
        void setUiIdle(bool idle);

        TaskScheduler* m_scheduler;
        std::shared_ptr<ITaskGraphComponentFactory> m_factory;
        TaskGraphScene* m_scene = nullptr;
        TaskGraphView* m_view = nullptr;
        SchedulerControlBar* m_controlBar = nullptr;
        TaskInspectorPanel* m_inspector = nullptr;
        QWidget* m_logView = nullptr;
        AggregateTaskLogView* m_aggregateLogView = nullptr;
        TaskLogOverlay* m_logOverlay = nullptr;
        TaskLogBuffer* m_logBuffer = nullptr;
        GuiPromptService* m_promptService = nullptr;
        FeatureSet m_baseFeatures;
        GraphVisualConfig m_visualConfig = GraphVisualConfig::light();
        bool m_idle = true;
        bool m_readOnly = false;
    };
}
}
