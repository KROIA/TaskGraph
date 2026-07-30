#include "gui/TaskGraphScene.h"

#if defined(QT_WIDGETS_ENABLED)
#include "gui/TaskNodeItem.h"
#include "gui/TaskEdgeItem.h"
#include "gui/GraphLayout.h"
#include "TaskScheduler.h"

#include <QMap>
#include <QSet>
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

        std::vector<TaskList> graph = m_scheduler->getTaskGraph();
        QHash<QString, QPointF> positions = GraphLayout::compute(graph);

        constexpr qreal nodeW = GraphLayout::nodeWidth;
        constexpr qreal nodeH = GraphLayout::nodeHeight;
        constexpr qreal hGap  = GraphLayout::hGap;
        constexpr qreal channelMargin = 12.0; // clearance from node edges to channels
        constexpr qreal portMargin   = 6.0;  // inset ports from node top/bottom

        // Build name -> layer index map
        QHash<QString, int> layerOf;
        for (size_t layer = 0; layer < graph.size(); ++layer)
        {
            for (const auto& task : graph[layer])
            {
                QString name = QString::fromStdString(task->getName());
                layerOf[name] = static_cast<int>(layer);
            }
        }

        // Create nodes
        for (const auto& layer : graph)
        {
            for (const auto& task : layer)
            {
                QString name = QString::fromStdString(task->getName());
                auto* node = new TaskNodeItem(name);
                if (positions.contains(name))
                    node->setPos(positions.value(name));
                node->setTaskStatus(task->getStatus());
                addItem(node);
                m_nodesByName[name].append(node);
            }
        }

        // Collect all edges
        struct PendingEdge
        {
            TaskNodeItem* from;
            TaskNodeItem* to;
            QString fromName;
            QString toName;
            int channel; // source layer index
        };
        QVector<PendingEdge> edges;

        for (const auto& layer : graph)
        {
            for (const auto& task : layer)
            {
                QString taskName = QString::fromStdString(task->getName());
                auto deps = task->getDependencies();
                for (const auto& dep : deps)
                {
                    QString depName = QString::fromStdString(dep->getName());
                    if (m_nodesByName.contains(depName) && m_nodesByName.contains(taskName))
                    {
                        TaskNodeItem* fromNode = m_nodesByName[depName].first();
                        TaskNodeItem* toNode = m_nodesByName[taskName].first();
                        int channel = layerOf.value(depName, 0);
                        edges.append({ fromNode, toNode, depName, taskName, channel });
                    }
                }
            }
        }

        // --- Port assignment ---
        // Group edges by source node (outgoing) and target node (incoming)
        QHash<QString, QVector<int>> outgoingEdges;
        QHash<QString, QVector<int>> incomingEdges;

        for (int i = 0; i < edges.size(); ++i)
        {
            outgoingEdges[edges[i].fromName].append(i);
            incomingEdges[edges[i].toName].append(i);
        }

        // Sort outgoing edges by target Y so ports are ordered top-to-bottom
        for (auto it = outgoingEdges.begin(); it != outgoingEdges.end(); ++it)
        {
            QVector<int>& idx = it.value();
            std::sort(idx.begin(), idx.end(), [&](int a, int b)
            {
                return edges[a].to->pos().y() < edges[b].to->pos().y();
            });
        }

        // Sort incoming edges by source Y
        for (auto it = incomingEdges.begin(); it != incomingEdges.end(); ++it)
        {
            QVector<int>& idx = it.value();
            std::sort(idx.begin(), idx.end(), [&](int a, int b)
            {
                return edges[a].from->pos().y() < edges[b].from->pos().y();
            });
        }

        // Compute per-edge exit port (right edge of source node)
        // and entry port (left edge of target node)
        QVector<QPointF> exitPort(edges.size());
        QVector<QPointF> entryPort(edges.size());

        // Distribute ports within [portMargin, nodeH - portMargin] to keep
        // stubs away from node top/bottom edges
        qreal portBandHeight = nodeH - 2.0 * portMargin;

        for (auto it = outgoingEdges.constBegin(); it != outgoingEdges.constEnd(); ++it)
        {
            const QVector<int>& idx = it.value();
            int n = idx.size();
            TaskNodeItem* node = edges[idx[0]].from;
            qreal ny = node->pos().y();
            qreal nx = node->pos().x();
            for (int j = 0; j < n; ++j)
            {
                qreal portY = ny + portMargin + (static_cast<qreal>(j) + 1.0)
                             * portBandHeight / (static_cast<qreal>(n) + 1.0);
                exitPort[idx[j]] = QPointF(nx + nodeW, portY);
            }
        }

        for (auto it = incomingEdges.constBegin(); it != incomingEdges.constEnd(); ++it)
        {
            const QVector<int>& idx = it.value();
            int n = idx.size();
            TaskNodeItem* node = edges[idx[0]].to;
            qreal ny = node->pos().y();
            qreal nx = node->pos().x();
            for (int j = 0; j < n; ++j)
            {
                qreal portY = ny + portMargin + (static_cast<qreal>(j) + 1.0)
                             * portBandHeight / (static_cast<qreal>(n) + 1.0);
                entryPort[idx[j]] = QPointF(nx, portY);
            }
        }

        // --- Channel assignment ---
        // Group edges by channel (source layer), spread vertical X across gap
        QMap<int, QVector<int>> edgesByChannel;
        for (int i = 0; i < edges.size(); ++i)
            edgesByChannel[edges[i].channel].append(i);

        QVector<qreal> channelX(edges.size());

        for (auto it = edgesByChannel.begin(); it != edgesByChannel.end(); ++it)
        {
            QVector<int>& idx = it.value();

            // Sort by midpoint Y of (exit, entry) to order channels
            std::sort(idx.begin(), idx.end(), [&](int a, int b)
            {
                qreal midA = (exitPort[a].y() + entryPort[a].y()) * 0.5;
                qreal midB = (exitPort[b].y() + entryPort[b].y()) * 0.5;
                return midA < midB;
            });

            int n = idx.size();
            // Gap runs from source right edge + margin to next layer left edge - margin
            qreal gapStart = edges[idx[0]].from->pos().x() + nodeW + channelMargin;
            qreal gapWidth = hGap - 2.0 * channelMargin;

            for (int j = 0; j < n; ++j)
            {
                qreal cx = gapStart + (static_cast<qreal>(j) + 1.0)
                         * gapWidth / (static_cast<qreal>(n) + 1.0);
                channelX[idx[j]] = cx;
            }
        }

        // --- Build orthogonal routes ---
        for (int i = 0; i < edges.size(); ++i)
        {
            QPointF start = exitPort[i];
            QPointF end = entryPort[i];
            qreal cx = channelX[i];

            QVector<QPointF> route;
            route.append(start);
            route.append(QPointF(cx, start.y()));
            route.append(QPointF(cx, end.y()));
            route.append(end);

            auto* edge = new TaskEdgeItem(edges[i].from, edges[i].to);
            edge->setRoute(route);
            addItem(edge);
            m_edges.append(edge);
        }
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
