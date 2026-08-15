#include "gui/TaskGraphView.h"

#if defined(QT_WIDGETS_ENABLED)
#include "gui/TaskNodeItem.h"
#include "gui/TaskLogOverlay.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QScrollBar>
#include <QPainter>
#include <QPen>
#include <QLineF>
#include <QRectF>

namespace TaskGraph
{
namespace Gui
{
    // clip a line from inside a rect to a point outside, returning the intersection
    // with the rect boundary; if no intersection found, returns the center
    static QPointF clipToRectEdge(const QRectF& rect, const QPointF& center, const QPointF& target)
    {
        QPointF edges[4][2] = {
            { rect.topLeft(),     rect.topRight()    },
            { rect.topRight(),    rect.bottomRight() },
            { rect.bottomRight(), rect.bottomLeft()  },
            { rect.bottomLeft(),  rect.topLeft()     }
        };

        QLineF ray(center, target);
        QPointF intersection;
        for (int i = 0; i < 4; ++i)
        {
            QLineF edge(edges[i][0], edges[i][1]);
            if (ray.intersects(edge, &intersection) == QLineF::BoundedIntersection)
                return intersection;
        }
        return center;
    }

    TaskGraphView::TaskGraphView(QWidget* parent)
        : QGraphicsView(parent)
    {
        setRenderHint(QPainter::Antialiasing);
        setDragMode(QGraphicsView::NoDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
        // sceneRect is kept far larger than the viewport (ensureSceneRectMargin) so
        // AnchorUnderMouse always has scroll room; hide the resulting scrollbars to
        // keep the fit-zoom appearance clean. Anchoring/panning drive the (hidden)
        // scrollbar values internally, so both still work.
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        // Accept keyboard focus on click so Spacebar (fit-to-view) reaches the view.
        setFocusPolicy(Qt::StrongFocus);
    }

    void TaskGraphView::setOverlay(TaskLogOverlay* overlay)
    {
        m_overlay = overlay;
    }

    void TaskGraphView::setLeaderTarget(const QPointF& sceneCenter, const QRectF& sceneRect)
    {
        m_leaderActive = true;
        m_leaderNodeScene = sceneCenter;
        m_leaderNodeSceneRect = sceneRect;
        viewport()->update();
    }

    void TaskGraphView::clearLeader()
    {
        m_leaderActive = false;
        viewport()->update();
    }

    void TaskGraphView::drawForeground(QPainter* painter, const QRectF& rect)
    {
        QGraphicsView::drawForeground(painter, rect);

        if (!m_leaderActive || !m_overlay || !m_overlay->isVisible())
            return;

        // node rect in viewport coords
        QPoint nodeTopLeft = mapFromScene(m_leaderNodeSceneRect.topLeft());
        QPoint nodeBottomRight = mapFromScene(m_leaderNodeSceneRect.bottomRight());
        QRectF nodeViewRect = QRectF(QPointF(nodeTopLeft), QPointF(nodeBottomRight));
        QPointF nodeCenter = nodeViewRect.center();

        // overlay rect in viewport coords
        QRect overlayRect = m_overlay->geometry();

        // nearest overlay corner to node center
        QPointF corners[4] = {
            QPointF(overlayRect.topLeft()),
            QPointF(overlayRect.topRight()),
            QPointF(overlayRect.bottomLeft()),
            QPointF(overlayRect.bottomRight())
        };
        QPointF nearest = corners[0];
        qreal minDist = QLineF(corners[0], nodeCenter).length();
        for (int i = 1; i < 4; ++i)
        {
            qreal d = QLineF(corners[i], nodeCenter).length();
            if (d < minDist)
            {
                minDist = d;
                nearest = corners[i];
            }
        }

        // clip the line to the node's boundary edge
        QPointF nodeEdge = clipToRectEdge(nodeViewRect, nodeCenter, nearest);

        painter->save();
        painter->resetTransform();
        QPen pen(QColor(128, 128, 128, 180), 1.5, Qt::DashLine);
        painter->setPen(pen);
        painter->drawLine(nearest, nodeEdge);
        painter->restore();
    }

    void TaskGraphView::ensureSceneRectMargin()
    {
        QRectF content = scene() ? scene()->itemsBoundingRect() : QRectF();
        QRectF visible = mapToScene(viewport()->rect()).boundingRect();
        QRectF base = content.united(visible);
        // Pad generously so the transformed sceneRect stays larger than the
        // viewport at every realistic zoom level; then AnchorUnderMouse never
        // hits the fits-in-viewport centering path.
        const qreal mx = base.width() * 2.0 + 2000.0;
        const qreal my = base.height() * 2.0 + 2000.0;
        setSceneRect(base.adjusted(-mx, -my, mx, my));
    }

    void TaskGraphView::frameGraph()
    {
        ensureSceneRectMargin();
        if (scene())
        {
            QRectF content = scene()->itemsBoundingRect();
            if (!content.isEmpty())
                centerOn(content.center());
        }
    }

    void TaskGraphView::fitGraph()
    {
        if (!scene())
            return;
        const QRectF content = scene()->itemsBoundingRect();
        if (content.isEmpty())
            return;
        fitInView(content, Qt::KeepAspectRatio);
        // Restore the large sceneRect margin so wheel-zoom-to-cursor keeps working.
        ensureSceneRectMargin();
    }

    void TaskGraphView::keyPressEvent(QKeyEvent* event)
    {
        if (event->key() == Qt::Key_Space)
        {
            fitGraph();
            event->accept();
            return;
        }
        QGraphicsView::keyPressEvent(event);
    }

    void TaskGraphView::wheelEvent(QWheelEvent* event)
    {
        // Guarantee scroll room before scaling so the cursor anchor holds.
        ensureSceneRectMargin();
        double factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        scale(factor, factor);
    }

    void TaskGraphView::resizeEvent(QResizeEvent* event)
    {
        QGraphicsView::resizeEvent(event);
        ensureSceneRectMargin();
    }

    void TaskGraphView::showEvent(QShowEvent* event)
    {
        QGraphicsView::showEvent(event);
        // Frame the graph once the viewport has its real (post-show) size;
        // earlier framing from the constructor/rebuilds runs against an invalid size.
        if (!m_initialFramed)
        {
            m_initialFramed = true;
            fitGraph();
        }
    }

    void TaskGraphView::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::MiddleButton)
        {
            m_panning = true;
            m_lastPanPos = event->pos();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }

        QGraphicsView::mousePressEvent(event);

        auto items = scene()->selectedItems();
        if (!items.isEmpty())
        {
            auto* node = dynamic_cast<TaskNodeItem*>(items.first());
            if (node)
                emit nodeSelected(node->taskName());
        }
        else
        {
            emit nodeSelected(QString());
        }
    }

    void TaskGraphView::mouseDoubleClickEvent(QMouseEvent* event)
    {
        QGraphicsView::mouseDoubleClickEvent(event);

        // Double-click also selects, so emit both signals
        auto items = scene()->selectedItems();
        if (!items.isEmpty())
        {
            auto* node = dynamic_cast<TaskNodeItem*>(items.first());
            if (node)
            {
                emit nodeSelected(node->taskName());
                emit nodeDoubleClicked(node->taskName());
            }
        }
    }

    void TaskGraphView::mouseMoveEvent(QMouseEvent* event)
    {
        if (m_panning)
        {
            QPoint delta = event->pos() - m_lastPanPos;
            m_lastPanPos = event->pos();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            event->accept();
            return;
        }
        QGraphicsView::mouseMoveEvent(event);
    }

    void TaskGraphView::mouseReleaseEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::MiddleButton && m_panning)
        {
            m_panning = false;
            setCursor(Qt::ArrowCursor);
            event->accept();
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }
}
}

#endif
