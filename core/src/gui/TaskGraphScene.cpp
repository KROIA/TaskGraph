#include "gui/TaskGraphScene.h"

#if defined(QT_WIDGETS_ENABLED)
#include "gui/TaskNodeItem.h"
#include "gui/TaskEdgeItem.h"
#include "gui/GraphLayout.h"
#include "TaskScheduler.h"
#include "TaskGraph_debug.h"

#include <QMap>
#include <QSet>
#include <QColor>
#include <QBrush>
#include <QPen>
#include <algorithm>

namespace TaskGraph
{
namespace Gui
{
    TaskGraphScene::TaskGraphScene(TaskScheduler* scheduler, QObject* parent)
        : QGraphicsScene(parent)
        , m_scheduler(scheduler)
    {
    }

    void TaskGraphScene::rebuild()
    {
        clear();
        m_nodesByName.clear();
        m_edges.clear();

        setBackgroundBrush(QBrush(m_config.background));

        std::vector<TaskList> graph = m_scheduler->getTaskGraph();
        LayoutResult layout = GraphLayout::computeLayout(graph, m_config);

        // Create nodes from the computed positions.
        for (const auto& layer : graph)
        {
            for (const auto& task : layer)
            {
                QString name = QString::fromStdString(task->getName());
                auto* node = new TaskNodeItem(name);
                if (layout.nodePositions.contains(name))
                    node->setPos(layout.nodePositions.value(name));
                node->setPalette(m_config.nodeBorder, m_config.nodeText);
                node->setStatusColors(m_config.statusPending, m_config.statusReady,
                    m_config.statusRunning, m_config.statusDone, m_config.statusFailed,
                    m_config.statusCancelled, m_config.statusSkipped);
                node->setTaskStatus(task->getStatus());
                addItem(node);
                m_nodesByName[name].append(node);
            }
        }

        // Create edges from the computed routes.
        for (const EdgeRoute& route : layout.edges)
        {
            if (!m_nodesByName.contains(route.fromName)
                || !m_nodesByName.contains(route.toName))
                continue;

            TaskNodeItem* fromNode = m_nodesByName[route.fromName].first();
            TaskNodeItem* toNode = m_nodesByName[route.toName].first();
            auto* edge = new TaskEdgeItem(fromNode, toNode);
            edge->setLineColor(m_config.edgeLine);
            edge->setArrowColor(m_config.edgeArrow);
            edge->setHighlightColors(m_config.edgeHighlightIncoming,
                                     m_config.edgeHighlightOutgoing);
            edge->setRoute(route.points, route.exact);
            addItem(edge);
            m_edges.append(edge);
        }

        // Debug overlay: draw the invisible routing waypoints.
        // Toggle by hand via TASKGRAPH_DEBUG_DRAW_WAYPOINTS in TaskGraph_debug.h,
        // or force on at runtime with the TASKGRAPH_DEBUG_WAYPOINTS env var.
        if (TASKGRAPH_DEBUG_DRAW_WAYPOINTS
            || qEnvironmentVariableIsSet("TASKGRAPH_DEBUG_WAYPOINTS"))
        {
            for (const QPointF& d : layout.dummyPoints)
            {
                auto* marker = addEllipse(d.x() - 5.0, d.y() - 5.0, 10.0, 10.0,
                    QPen(Qt::NoPen), QBrush(m_config.debugWaypoint));
                marker->setZValue(100.0);
            }
            for (const QPointF& a : layout.anchorPoints)
            {
                auto* marker = addEllipse(a.x() - 3.5, a.y() - 3.5, 7.0, 7.0,
                    QPen(Qt::NoPen), QBrush(m_config.debugAnchor));
                marker->setZValue(100.0);
            }
        }
    }

    void TaskGraphScene::setVisualConfig(const GraphVisualConfig& config)
    {
        m_config = config;
        rebuild();
    }

    void TaskGraphScene::highlightEdgesForNode(const QString& taskName)
    {
        // Find the task to get its dependencies
        std::shared_ptr<Task> selectedTask;
        auto graph = m_scheduler->getTaskGraph();
        for (const auto& layer : graph)
        {
            for (const auto& task : layer)
            {
                if (QString::fromStdString(task->getName()) == taskName)
                {
                    selectedTask = task;
                    break;
                }
            }
            if (selectedTask)
                break;
        }

        // Build the set of dependency names (selected depends on these)
        QSet<QString> depNames;
        if (selectedTask)
        {
            auto deps = selectedTask->getDependencies();
            for (const auto& dep : deps)
                depNames.insert(QString::fromStdString(dep->getName()));
        }

        for (auto* edge : m_edges)
        {
            QString fromName = edge->fromNode()->taskName();
            QString toName = edge->toNode()->taskName();

            if (toName == taskName && depNames.contains(fromName))
            {
                // Edge INTO selected node (selected depends on fromNode)
                edge->setHighlight(TaskEdgeItem::Highlight::Incoming);
            }
            else if (fromName == taskName)
            {
                // Edge FROM selected node (toNode depends on selected)
                edge->setHighlight(TaskEdgeItem::Highlight::Outgoing);
            }
            else
            {
                edge->setHighlight(TaskEdgeItem::Highlight::None);
            }
        }
    }

    void TaskGraphScene::clearEdgeHighlights()
    {
        for (auto* edge : m_edges)
            edge->setHighlight(TaskEdgeItem::Highlight::None);
    }

    void TaskGraphScene::updateNodeStatus(const QString& name, Task::Status status)
    {
        if (m_nodesByName.contains(name))
        {
            for (auto* node : m_nodesByName[name])
                node->setTaskStatus(status);
        }
    }

    QPointF TaskGraphScene::nodeCenter(const QString& name) const
    {
        if (m_nodesByName.contains(name) && !m_nodesByName[name].isEmpty())
        {
            auto* node = m_nodesByName[name].first();
            return node->pos() + node->boundingRect().center();
        }
        return QPointF();
    }

    QRectF TaskGraphScene::nodeSceneRect(const QString& name) const
    {
        if (m_nodesByName.contains(name) && !m_nodesByName[name].isEmpty())
        {
            auto* node = m_nodesByName[name].first();
            return node->boundingRect().translated(node->pos());
        }
        return QRectF();
    }
}
}

#endif
