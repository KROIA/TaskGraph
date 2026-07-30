#pragma once

#include "TaskGraph_global.h"
#include <QFrame>
#include <QString>
#include <QPoint>

namespace Log { using LoggerID = unsigned int; class Message; }
class QTextEdit;
class QPushButton;
class QLabel;

namespace TaskGraph
{
namespace Gui
{
    class TaskLogBuffer;

    class TASK_GRAPH_API TaskLogOverlay : public QFrame
    {
        Q_OBJECT
    public:
        explicit TaskLogOverlay(TaskLogBuffer* buffer, QWidget* parent = nullptr);

        void showForTask(const QString& taskName, Log::LoggerID loggerID);
        void dismiss();
        void clearForTask(const QString& taskName);
        QString currentTask() const { return m_currentTask; }
        Log::LoggerID currentLoggerId() const { return m_currentId; }

    signals:
        void dismissed();
        void overlayMoved();

    protected:
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

    private:
        enum InteractMode { None, Dragging, ResizingRight, ResizingBottom, ResizingCorner };

        void appendColoredLine(const Log::Message& msg);
        void clampToParent();
        InteractMode hitTest(const QPoint& pos) const;
        void updateCursorForPos(const QPoint& pos);

        TaskLogBuffer* m_buffer;
        QTextEdit* m_textEdit = nullptr;
        QPushButton* m_closeBtn = nullptr;
        QLabel* m_titleLabel = nullptr;
        QString m_currentTask;
        Log::LoggerID m_currentId = 0;

        InteractMode m_mode = None;
        QPoint m_dragOffset;
        QRect m_resizeStartGeom;
        QPoint m_resizeStartPos;

        static constexpr int kResizeMargin = 8;
        static constexpr int kTitleBarHeight = 30;
    };
}
}
