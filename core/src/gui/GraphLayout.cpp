#include "gui/GraphLayout.h"
#include "Task.h"

#include <algorithm>
#include <limits>

namespace TaskGraph
{
namespace Gui
{
    namespace
    {
        struct Vertex
        {
            int layer;
            bool isDummy;
            QString name;   // real node only
        };

        struct Edge
        {
            QString fromName;
            QString toName;
            int La;
            int Lb;
            QVector<int> chain; // vertex ids, layer La .. Lb inclusive
        };
    }

    LayoutResult GraphLayout::computeLayout(const std::vector<TaskList>& graph,
                                            const GraphVisualConfig& cfg)
    {
        LayoutResult result;

        const int numLayers = static_cast<int>(graph.size());
        if (numLayers == 0)
            return result;

        // 1. LAYER MAP
        QHash<QString, int> layerOf;
        for (int k = 0; k < numLayers; ++k)
        {
            for (const auto& task : graph[k])
                layerOf[QString::fromStdString(task->getName())] = k;
        }

        // Real vertices, one per task, plus per-layer slot lists.
        QVector<Vertex> vertices;
        QHash<QString, int> realVertexId;
        QVector<QVector<int>> layerSlots(numLayers);

        for (int k = 0; k < numLayers; ++k)
        {
            for (const auto& task : graph[k])
            {
                QString name = QString::fromStdString(task->getName());
                int id = vertices.size();
                vertices.append({ k, false, name });
                realVertexId[name] = id;
                layerSlots[k].append(id);
            }
        }

        // 2. EDGES + 3. DUMMY WAYPOINTS
        QVector<Edge> edges;
        for (int k = 0; k < numLayers; ++k)
        {
            for (const auto& task : graph[k])
            {
                QString toName = QString::fromStdString(task->getName());
                int Lb = k;
                for (const auto& dep : task->getDependencies())
                {
                    QString fromName = QString::fromStdString(dep->getName());
                    if (!layerOf.contains(fromName) || !realVertexId.contains(fromName))
                        continue;
                    int La = layerOf.value(fromName);
                    if (La >= Lb)
                        continue; // guard: not a forward layered edge

                    Edge e;
                    e.fromName = fromName;
                    e.toName = toName;
                    e.La = La;
                    e.Lb = Lb;
                    e.chain.append(realVertexId[fromName]);
                    for (int mid = La + 1; mid <= Lb - 1; ++mid)
                    {
                        int id = vertices.size();
                        vertices.append({ mid, true, QString() });
                        layerSlots[mid].append(id);
                        e.chain.append(id);
                    }
                    e.chain.append(realVertexId[toName]);
                    edges.append(e);
                }
            }
        }

        // Expanded adjacency between consecutive chain vertices.
        QVector<QVector<int>> upNeighbors(vertices.size());
        QVector<QVector<int>> downNeighbors(vertices.size());
        for (const Edge& e : edges)
        {
            for (int i = 0; i + 1 < e.chain.size(); ++i)
            {
                int a = e.chain[i];
                int b = e.chain[i + 1];
                downNeighbors[a].append(b);
                upNeighbors[b].append(a);
            }
        }

        // 4. ORDERING (barycenter sweeps)
        QVector<int> pos(vertices.size(), 0);
        auto updatePos = [&](int layer)
        {
            const QVector<int>& order = layerSlots[layer];
            for (int i = 0; i < order.size(); ++i)
                pos[order[i]] = i;
        };
        for (int k = 0; k < numLayers; ++k)
            updatePos(k);

        auto barycenter = [&](int v, const QVector<QVector<int>>& neighbors) -> qreal
        {
            const QVector<int>& nb = neighbors[v];
            if (nb.isEmpty())
                return static_cast<qreal>(pos[v]);
            qreal sum = 0.0;
            for (int n : nb)
                sum += pos[n];
            return sum / static_cast<qreal>(nb.size());
        };

        constexpr int sweeps = 4;
        for (int s = 0; s < sweeps; ++s)
        {
            // down sweep: order by upper neighbors
            for (int k = 1; k < numLayers; ++k)
            {
                QVector<int>& order = layerSlots[k];
                std::stable_sort(order.begin(), order.end(), [&](int a, int b)
                {
                    return barycenter(a, upNeighbors) < barycenter(b, upNeighbors);
                });
                updatePos(k);
            }
            // up sweep: order by lower neighbors
            for (int k = numLayers - 2; k >= 0; --k)
            {
                QVector<int>& order = layerSlots[k];
                std::stable_sort(order.begin(), order.end(), [&](int a, int b)
                {
                    return barycenter(a, downNeighbors) < barycenter(b, downNeighbors);
                });
                updatePos(k);
            }
        }

        // 5. INITIAL Y ASSIGNMENT (dense stacking per layer, centered)
        QVector<qreal> centerY(vertices.size(), 0.0);
        auto heightOf = [&](int v)
        {
            return vertices[v].isDummy ? dummySlot : nodeHeight;
        };
        auto sepOf = [&](int a, int b)
        {
            return (heightOf(a) + heightOf(b)) / 2.0 + vGap;
        };

        for (int k = 0; k < numLayers; ++k)
        {
            const QVector<int>& order = layerSlots[k];
            qreal totalHeight = 0.0;
            for (int idx = 0; idx < order.size(); ++idx)
            {
                totalHeight += heightOf(order[idx]);
                if (idx > 0)
                    totalHeight += vGap;
            }

            qreal cursor = -totalHeight / 2.0;
            for (int v : order)
            {
                qreal h = heightOf(v);
                centerY[v] = cursor + h / 2.0;
                cursor += h + vGap;
            }
        }

        // 5b. CHAIN-PRIORITY STRAIGHTENING
        // Straighten long-edge dummy chains first (longest span wins), so each
        // chain's dummies share one Y; then relax real nodes around them. Order
        // per layer is fixed here; only Y moves, preserving min-separation.
        // targetY = median of the chain's own current dummy Ys (self-contained,
        // keeps the band near where dummies were stacked; no port dependency).
        auto medianNeighborY = [&](int v) -> qreal
        {
            QVector<qreal> ys;
            for (int n : upNeighbors[v])
                ys.append(centerY[n]);
            for (int n : downNeighbors[v])
                ys.append(centerY[n]);
            if (ys.isEmpty())
                return centerY[v];
            std::sort(ys.begin(), ys.end());
            int m = ys.size();
            if (m % 2 == 1)
                return ys[m / 2];
            return (ys[m / 2 - 1] + ys[m / 2]) * 0.5;
        };

        QVector<bool> fixed(vertices.size(), false);
        auto obstacle = [&](int v)
        {
            // Fixed chain dummies and all real nodes block chain placement.
            return fixed[v] || !vertices[v].isDummy;
        };

        QVector<int> longEdges;
        for (int i = 0; i < edges.size(); ++i)
        {
            if (edges[i].Lb - edges[i].La >= 2)
                longEdges.append(i);
        }
        std::stable_sort(longEdges.begin(), longEdges.end(), [&](int a, int b)
        {
            return (edges[a].Lb - edges[a].La) > (edges[b].Lb - edges[b].La);
        });

        for (int ei : longEdges)
        {
            const Edge& e = edges[ei];

            QVector<qreal> ys;
            for (int c = 1; c + 1 < e.chain.size(); ++c)
                ys.append(centerY[e.chain[c]]);
            if (ys.isEmpty())
                continue;
            std::sort(ys.begin(), ys.end());
            qreal targetY = (ys.size() % 2 == 1)
                          ? ys[ys.size() / 2]
                          : (ys[ys.size() / 2 - 1] + ys[ys.size() / 2]) * 0.5;

            for (int c = 1; c + 1 < e.chain.size(); ++c)
            {
                int v = e.chain[c];
                const QVector<int>& order = layerSlots[vertices[v].layer];
                int idx = order.indexOf(v);

                // Nearest obstacle above/below sets the feasible band, counting
                // separation needed for any unfixed slots in between.
                qreal lower = -std::numeric_limits<qreal>::max();
                qreal need = 0.0;
                for (int j = idx - 1; j >= 0; --j)
                {
                    need += sepOf(order[j], order[j + 1]);
                    if (obstacle(order[j]))
                    {
                        lower = centerY[order[j]] + need;
                        break;
                    }
                }
                qreal upper = std::numeric_limits<qreal>::max();
                need = 0.0;
                for (int j = idx + 1; j < order.size(); ++j)
                {
                    need += sepOf(order[j - 1], order[j]);
                    if (obstacle(order[j]))
                    {
                        upper = centerY[order[j]] - need;
                        break;
                    }
                }

                qreal y = targetY;
                if (y < lower)
                    y = lower;
                if (y > upper)
                    y = upper;
                centerY[v] = y;
                fixed[v] = true;
            }
        }

        // Relax the remaining (non-fixed) slots toward their neighbor medians,
        // clamped against immediate in-layer neighbors. Fixed chain dummies stay
        // put so the straightened chains are not re-bent.
        constexpr int relaxPasses = 4;
        for (int p = 0; p < relaxPasses; ++p)
        {
            bool downward = (p % 2 == 0);
            for (int step = 0; step < numLayers; ++step)
            {
                int k = downward ? step : (numLayers - 1 - step);
                const QVector<int>& order = layerSlots[k];
                for (int idx = 0; idx < order.size(); ++idx)
                {
                    int v = order[idx];
                    if (fixed[v])
                        continue;

                    qreal lower = -std::numeric_limits<qreal>::max();
                    qreal upper = std::numeric_limits<qreal>::max();
                    if (idx > 0)
                        lower = centerY[order[idx - 1]] + sepOf(order[idx - 1], v);
                    if (idx + 1 < order.size())
                        upper = centerY[order[idx + 1]] - sepOf(v, order[idx + 1]);

                    qreal desired = medianNeighborY(v);
                    if (desired < lower)
                        desired = lower;
                    if (desired > upper)
                        desired = upper;
                    centerY[v] = desired;
                }
            }
        }

        // Real-node top Y from final centers.
        QVector<qreal> topY(vertices.size(), 0.0);
        for (int v = 0; v < vertices.size(); ++v)
            topY[v] = centerY[v] - heightOf(v) / 2.0;

        // 6. GAP WIDTH (horizontal), density-aware but bounded
        QVector<int> crossings(numLayers > 0 ? numLayers - 1 : 0, 0);
        for (const Edge& e : edges)
        {
            for (int g = e.La; g <= e.Lb - 1; ++g)
                crossings[g] += 1;
        }

        QVector<qreal> gapWidth(crossings.size(), minGap);
        for (int g = 0; g < gapWidth.size(); ++g)
        {
            qreal w = minGap + crossings[g] * 4.0;
            gapWidth[g] = std::min(maxGap, std::max(minGap, w));
        }

        QVector<qreal> layerX(numLayers, 0.0);
        for (int k = 1; k < numLayers; ++k)
            layerX[k] = layerX[k - 1] + nodeWidth + gapWidth[k - 1];

        // Node positions (real vertices only)
        for (auto it = realVertexId.constBegin(); it != realVertexId.constEnd(); ++it)
        {
            int id = it.value();
            result.nodePositions.insert(it.key(),
                QPointF(layerX[vertices[id].layer], topY[id]));
        }

        // 8. PORTS on real nodes
        QHash<QString, QVector<int>> outgoing; // by source name
        QHash<QString, QVector<int>> incoming; // by target name
        for (int i = 0; i < edges.size(); ++i)
        {
            outgoing[edges[i].fromName].append(i);
            incoming[edges[i].toName].append(i);
        }

        QVector<qreal> exitPortY(edges.size(), 0.0);
        QVector<qreal> entryPortY(edges.size(), 0.0);
        const qreal portBand = nodeHeight - 2.0 * portMargin;

        for (auto it = outgoing.begin(); it != outgoing.end(); ++it)
        {
            QVector<int>& idx = it.value();
            std::stable_sort(idx.begin(), idx.end(), [&](int a, int b)
            {
                int na = edges[a].chain[1];
                int nb = edges[b].chain[1];
                return centerY[na] < centerY[nb];
            });
            int n = idx.size();
            int srcId = realVertexId.value(it.key());
            qreal ny = topY[srcId];
            for (int j = 0; j < n; ++j)
            {
                qreal portY = ny + portMargin + (static_cast<qreal>(j) + 1.0)
                            * portBand / (static_cast<qreal>(n) + 1.0);
                exitPortY[idx[j]] = portY;
            }
        }

        for (auto it = incoming.begin(); it != incoming.end(); ++it)
        {
            QVector<int>& idx = it.value();
            std::stable_sort(idx.begin(), idx.end(), [&](int a, int b)
            {
                int na = edges[a].chain[edges[a].chain.size() - 2];
                int nb = edges[b].chain[edges[b].chain.size() - 2];
                return centerY[na] < centerY[nb];
            });
            int n = idx.size();
            int dstId = realVertexId.value(it.key());
            qreal ny = topY[dstId];
            for (int j = 0; j < n; ++j)
            {
                qreal portY = ny + portMargin + (static_cast<qreal>(j) + 1.0)
                            * portBand / (static_cast<qreal>(n) + 1.0);
                entryPortY[idx[j]] = portY;
            }
        }

        // 9. BUILD ROUTE as horizontal-run PAIRS per edge
        // exit stub -> flat column run at each crossed layer's chain Y -> entry stub.
        // Each consecutive pair (points[2k], points[2k+1]) is a straight run; the
        // edge builder joins runs with gap S-curves.
        result.edges.reserve(edges.size());
        for (int i = 0; i < edges.size(); ++i)
        {
            const Edge& e = edges[i];
            EdgeRoute route;
            route.fromName = e.fromName;
            route.toName = e.toName;

            qreal uRight = layerX[e.La] + nodeWidth;
            qreal vLeft = layerX[e.Lb];

            // Clamp stubs so they never reach past the adjacent layer.
            qreal srcPerp = std::min(perpLen, gapWidth[e.La] * 0.35);
            qreal dstPerp = std::min(perpLen, gapWidth[e.Lb - 1] * 0.35);

            auto push = [&](const QPointF& pt, bool isExact)
            {
                route.points.append(pt);
                route.exact.append(isExact);
            };

            // 1. Exit port (exact) + steering port anchor.
            push(QPointF(uRight, exitPortY[i]), true);
            push(QPointF(uRight + srcPerp, exitPortY[i]), cfg.portAnchorExact);
            result.anchorPoints.append(QPointF(uRight + srcPerp, exitPortY[i]));

            // 2. Column waypoints across each intermediate layer at its chain Y.
            for (int c = 1; c + 1 < e.chain.size(); ++c)
            {
                int d = e.chain[c];
                qreal lx = layerX[vertices[d].layer];
                qreal chainY = centerY[d];
                if (cfg.waypointMode == ColumnWaypointMode::OneCenter)
                {
                    QPointF center(lx + nodeWidth / 2.0, chainY);
                    push(center, cfg.centerWaypointExact);
                    result.dummyPoints.append(center);
                }
                else
                {
                    QPointF left(lx, chainY);
                    QPointF right(lx + nodeWidth, chainY);
                    push(left, cfg.leftWaypointExact);
                    push(right, cfg.rightWaypointExact);
                    result.dummyPoints.append(left);
                    result.dummyPoints.append(right);
                }
            }

            // 3. Steering port anchor + entry port (exact).
            push(QPointF(vLeft - dstPerp, entryPortY[i]), cfg.portAnchorExact);
            push(QPointF(vLeft, entryPortY[i]), true);
            result.anchorPoints.append(QPointF(vLeft - dstPerp, entryPortY[i]));

            result.edges.append(route);
        }

        return result;
    }
}
}
