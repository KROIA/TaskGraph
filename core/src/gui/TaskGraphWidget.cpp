#include "gui/TaskGraphWidget.h"

#if defined(QT_WIDGETS_ENABLED)
#include "gui/TaskGraphScene.h"
#include "gui/TaskGraphView.h"
#include "gui/SchedulerControlBar.h"
#include "gui/TaskInspectorPanel.h"
#include "gui/TaskLogOverlay.h"
#include "gui/TaskLogBuffer.h"
#include "gui/AggregateTaskLogView.h"
#include "gui/GuiPromptService.h"
#include "TaskScheduler.h"
#include "Task.h"
#include <QVBoxLayout>
#include <QSplitter>

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
        m_baseFeatures = m_factory->features();

        createComponents();
        buildLayout();
        wireComponents();
        wireScheduler();

        m_scene->setVisualConfig(m_visualConfig);
        setUiIdle(true);
    }

    void TaskGraphWidget::setVisualConfig(const GraphVisualConfig& config)
    {
        m_visualConfig = config;
        if (m_scene)
            m_scene->setVisualConfig(config); // stores + rebuilds
    }

    void TaskGraphWidget::createComponents()
    {
        m_logBuffer = new TaskLogBuffer(this);
        registerTaskLoggers();

        m_controlBar = m_factory->createControlBar(m_scheduler, this);
        m_scene = m_factory->createScene(m_scheduler, this);
        m_view = m_factory->createView(m_scheduler, this);
        if (m_view)
            m_view->setScene(m_scene);

        auto* inspectorWidget = m_factory->createInspector(m_scheduler, this);
        m_inspector = qobject_cast<TaskInspectorPanel*>(inspectorWidget);
        if (m_inspector)
            m_inspector->setFeatures(m_factory->features());

        m_logView = m_factory->createLogView(m_scheduler, m_logBuffer, this);
        m_aggregateLogView = qobject_cast<AggregateTaskLogView*>(m_logView);

        m_promptService = m_factory->createGuiPromptService(m_scheduler, this, this);

        m_logOverlay = m_factory->createLogOverlay(m_logBuffer,
            m_view ? static_cast<QWidget*>(m_view) : this);
        if (m_logOverlay)
            m_logOverlay->move(10, 10);
    }

    void TaskGraphWidget::buildLayout()
    {
        auto* outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);

        if (m_controlBar)
            outerLayout->addWidget(m_controlBar);

        QWidget* graphArea = nullptr;
        if (m_logView && m_view)
        {
            auto* vSplitter = new QSplitter(Qt::Vertical, this);
            vSplitter->addWidget(m_view);
            vSplitter->addWidget(m_logView);
            vSplitter->setStretchFactor(0, 3);
            vSplitter->setStretchFactor(1, 1);
            graphArea = vSplitter;
        }
        else if (m_view)
        {
            graphArea = m_view;
        }
        else if (m_logView)
        {
            graphArea = m_logView;
        }

        if (m_inspector && graphArea)
        {
            auto* hSplitter = new QSplitter(Qt::Horizontal, this);
            hSplitter->addWidget(graphArea);
            m_inspector->setMinimumWidth(220);
            m_inspector->setMaximumWidth(400);
            hSplitter->addWidget(m_inspector);
            hSplitter->setStretchFactor(0, 4);
            hSplitter->setStretchFactor(1, 1);
            outerLayout->addWidget(hSplitter, 1);
        }
        else if (m_inspector)
        {
            outerLayout->addWidget(m_inspector, 1);
        }
        else if (graphArea)
        {
            outerLayout->addWidget(graphArea, 1);
        }
    }

    void TaskGraphWidget::wireComponents()
    {
        if (m_inspector)
        {
            connect(m_inspector, &TaskInspectorPanel::graphStructureChanged,
                    this, &TaskGraphWidget::onGraphStructureChanged);
        }

        if (m_view)
        {
            connect(m_view, &TaskGraphView::nodeSelected,
                    this, &TaskGraphWidget::onNodeSelected);
            connect(m_view, &TaskGraphView::nodeDoubleClicked,
                    this, &TaskGraphWidget::onNodeDoubleClicked);
        }

        if (m_controlBar)
        {
            connect(m_controlBar, &SchedulerControlBar::viewerModeToggled,
                    this, &TaskGraphWidget::setReadOnly);
        }

        if (m_view && m_logOverlay)
        {
            m_view->setOverlay(m_logOverlay);

            connect(m_logOverlay, &TaskLogOverlay::overlayMoved,
                    this, [this]() {
                m_view->viewport()->update();
            });

            connect(m_logOverlay, &TaskLogOverlay::dismissed,
                    this, [this]() {
                m_view->clearLeader();
            });
        }
    }

    void TaskGraphWidget::wireScheduler()
    {
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
            m_logBuffer->clearAll();
            if (m_logOverlay)
                m_logOverlay->dismiss();
            if (m_aggregateLogView)
                m_aggregateLogView->clearAll();
            if (m_view)
                m_view->clearLeader();
            if (m_inspector)
                m_inspector->clearSelection();
            setUiIdle(true);
        });

        connect(m_scheduler, &TaskScheduler::started, this, [this]() {
            m_scene->rebuild();
            registerTaskLoggers();
            setUiIdle(false);
        });

        connect(m_scheduler, &TaskScheduler::completed, this, [this]() {
            refreshAllNodeStatuses();
            setUiIdle(true);
        });

        connect(m_scheduler, &TaskScheduler::cancelled, this, [this]() {
            refreshAllNodeStatuses();
            setUiIdle(true);
        });
    }

    void TaskGraphWidget::refreshAllNodeStatuses()
    {
        auto graph = m_scheduler->getTaskGraph();
        for (const auto& layer : graph)
            for (const auto& task : layer)
                m_scene->updateNodeStatus(
                    QString::fromStdString(task->getName()),
                    task->getStatus());
    }

    void TaskGraphWidget::setUiIdle(bool idle)
    {
        m_idle = idle;
        if (m_inspector)
            m_inspector->setEditingEnabled(idle);
    }

    void TaskGraphWidget::setReadOnly(bool ro)
    {
        if (ro == m_readOnly)
            return;
        m_readOnly = ro;

        FeatureSet effective = m_baseFeatures;
        if (ro)
        {
            effective.set(Feature::EditAddTask, false);
            effective.set(Feature::EditRemoveTask, false);
            effective.set(Feature::EditDependencies, false);
            effective.set(Feature::EditTaskConfig, false);
        }

        if (m_inspector)
            m_inspector->setFeatures(effective);
        if (m_controlBar)
            m_controlBar->setRunControlsEnabled(!ro);

        emit readOnlyChanged(ro);
    }

    void TaskGraphWidget::registerTaskLoggers()
    {
        auto graph = m_scheduler->getTaskGraph();
        for (const auto& layer : graph)
            for (const auto& task : layer)
                m_logBuffer->trackLogger(
                    task->logger().getID(),
                    QString::fromStdString(task->getName()));
    }

    void TaskGraphWidget::onNodeSelected(const QString& taskName)
    {
        if (taskName.isEmpty())
        {
            if (m_inspector)
                m_inspector->clearSelection();
            if (m_scene)
                m_scene->clearEdgeHighlights();
            return;
        }

        if (m_inspector)
            m_inspector->inspectTask(taskName);

        // Highlight connected edges
        if (m_scene)
            m_scene->highlightEdgesForNode(taskName);
    }

    void TaskGraphWidget::onNodeDoubleClicked(const QString& taskName)
    {
        Log::LoggerID lid = m_logBuffer->loggerIdForTask(taskName);
        if (lid == 0 || !m_logOverlay || !m_view)
            return;

        m_logOverlay->showForTask(taskName, lid);
        m_view->setLeaderTarget(m_scene->nodeCenter(taskName),
                                m_scene->nodeSceneRect(taskName));
    }

    void TaskGraphWidget::onGraphStructureChanged()
    {
        m_scene->rebuild();
        registerTaskLoggers();
    }
}
}

#endif
