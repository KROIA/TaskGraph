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
        auto* outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);

        m_logBuffer = new TaskLogBuffer(this);
        registerTaskLoggers();

        m_controlBar = m_factory->createControlBar(m_scheduler, this);
        if (m_controlBar)
            outerLayout->addWidget(m_controlBar);

        m_scene = new TaskGraphScene(m_scheduler, this);
        m_view = m_factory->createView(m_scheduler, this);

        // inspector from factory
        auto* inspectorWidget = m_factory->createInspector(m_scheduler, this);
        if (inspectorWidget)
        {
            m_inspector = qobject_cast<TaskInspectorPanel*>(inspectorWidget);
            if (m_inspector)
            {
                m_inspector->setFeatures(m_factory->features());
                connect(m_inspector, &TaskInspectorPanel::graphStructureChanged,
                        this, &TaskGraphWidget::onGraphStructureChanged);
            }
        }

        AggregateTaskLogView* aggView = nullptr;
        if (m_factory->features().has(Feature::ShowLog))
        {
            aggView = new AggregateTaskLogView(m_scheduler, m_logBuffer, this);
            m_logView = aggView;
        }

        // GUI prompt service (gated by feature)
        if (m_factory->features().has(Feature::GuiRoundTrip))
            m_promptService = new GuiPromptService(m_scheduler, this, this);

        // build the center area
        QWidget* graphArea = nullptr;
        if (m_logView && m_view)
        {
            auto* vSplitter = new QSplitter(Qt::Vertical, this);
            m_view->setScene(m_scene);
            vSplitter->addWidget(m_view);
            vSplitter->addWidget(m_logView);
            vSplitter->setStretchFactor(0, 3);
            vSplitter->setStretchFactor(1, 1);
            graphArea = vSplitter;
        }
        else if (m_view)
        {
            m_view->setScene(m_scene);
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

        m_logOverlay = new TaskLogOverlay(m_logBuffer,
            m_view ? static_cast<QWidget*>(m_view) : this);
        m_logOverlay->move(10, 10);

        if (m_view)
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

        m_scene->rebuild();

        if (m_view)
        {
            connect(m_view, &TaskGraphView::nodeSelected,
                    this, &TaskGraphWidget::onNodeSelected);
        }

        connect(m_scheduler, &TaskScheduler::taskStarted, this, [this](QString name) {
            m_scene->updateNodeStatus(name, Task::Status::Running);
        });
        connect(m_scheduler, &TaskScheduler::taskFinished, this, [this](QString name) {
            m_scene->updateNodeStatus(name, Task::Status::Done);
        });
        connect(m_scheduler, &TaskScheduler::taskFailed, this, [this](QString name, QString) {
            m_scene->updateNodeStatus(name, Task::Status::Failed);
        });

        connect(m_scheduler, &TaskScheduler::wasReset, this, [this, aggView]() {
            m_scene->rebuild();
            m_logBuffer->clearAll();
            m_logOverlay->dismiss();
            if (aggView)
                aggView->clearAll();
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
            auto graph = m_scheduler->getTaskGraph();
            for (const auto& layer : graph)
                for (const auto& task : layer)
                    m_scene->updateNodeStatus(
                        QString::fromStdString(task->getName()),
                        task->getStatus());
            setUiIdle(true);
        });

        connect(m_scheduler, &TaskScheduler::cancelled, this, [this]() {
            auto graph = m_scheduler->getTaskGraph();
            for (const auto& layer : graph)
                for (const auto& task : layer)
                    m_scene->updateNodeStatus(
                        QString::fromStdString(task->getName()),
                        task->getStatus());
            setUiIdle(true);
        });

        // initial state
        setUiIdle(true);
    }

    void TaskGraphWidget::setUiIdle(bool idle)
    {
        m_idle = idle;
        if (m_inspector)
            m_inspector->setEditingEnabled(idle);
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
        if (m_inspector)
            m_inspector->inspectTask(taskName);

        auto graph = m_scheduler->getTaskGraph();
        for (const auto& layer : graph)
        {
            for (const auto& task : layer)
            {
                if (QString::fromStdString(task->getName()) == taskName)
                {
                    Log::LoggerID lid = task->logger().getID();
                    m_logOverlay->showForTask(taskName, lid);

                    QPointF nodeCenter = m_scene->nodeCenter(taskName);
                    QRectF nodeRect = m_scene->nodeSceneRect(taskName);
                    m_view->setLeaderTarget(taskName, nodeCenter, nodeRect);
                    return;
                }
            }
        }
    }

    void TaskGraphWidget::onGraphStructureChanged()
    {
        m_scene->rebuild();
        registerTaskLoggers();
    }
}
}

#endif
