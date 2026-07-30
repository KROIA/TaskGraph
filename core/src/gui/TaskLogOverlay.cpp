#include "gui/TaskLogOverlay.h"

#if defined(QT_WIDGETS_ENABLED)
#include "gui/TaskLogBuffer.h"
#include "LogMessage.h"
#include "LogLevel.h"
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>

namespace TaskGraph
{
namespace Gui
{
    static QColor colorForLevel(Log::Level level)
    {
        switch (level)
        {
            case Log::Level::error:   return QColor(255, 110, 110);
            case Log::Level::warning: return QColor(255, 190, 60);
            case Log::Level::info:    return QColor(220, 220, 220);
            case Log::Level::debug:   return QColor(140, 140, 140);
            case Log::Level::trace:   return QColor(110, 110, 110);
            default:                  return QColor(180, 180, 255);
        }
    }

    TaskLogOverlay::TaskLogOverlay(TaskLogBuffer* buffer, QWidget* parent)
        : QFrame(parent)
        , m_buffer(buffer)
    {
        setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
        setLineWidth(1);
        setAutoFillBackground(true);
        setMouseTracking(true);
        setStyleSheet(
            "TaskGraph--Gui--TaskLogOverlay {"
            "  background: #3c3c3c;"
            "  border: 1px solid #6a6a6a;"
            "  border-radius: 4px;"
            "}"
        );

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(6, 4, 6, 6);
        layout->setSpacing(2);

        auto* header = new QHBoxLayout();
        header->setContentsMargins(0, 0, 0, 0);
        m_titleLabel = new QLabel("Task Log", this);
        m_titleLabel->setCursor(Qt::OpenHandCursor);
        m_titleLabel->setStyleSheet(
            "QLabel { color: #e0e0e0; background: #4a4a4a;"
            "  border-radius: 2px; padding: 2px 4px; }"
        );
        QFont f = m_titleLabel->font();
        f.setBold(true);
        m_titleLabel->setFont(f);
        header->addWidget(m_titleLabel, 1);

        m_closeBtn = new QPushButton("X", this);
        m_closeBtn->setFixedSize(20, 20);
        m_closeBtn->setFlat(true);
        m_closeBtn->setStyleSheet(
            "QPushButton { color: #cccccc; background: transparent; }"
            "QPushButton:hover { color: #ffffff; background: #5a5a5a; }"
        );
        header->addWidget(m_closeBtn);
        layout->addLayout(header);

        m_textEdit = new QTextEdit(this);
        m_textEdit->setReadOnly(true);
        m_textEdit->document()->setMaximumBlockCount(500);
        m_textEdit->setStyleSheet(
            "QTextEdit { background: #2a2a2a; border: 1px solid #555555;"
            "  border-radius: 2px; color: #dcdcdc; }"
        );
        layout->addWidget(m_textEdit, 1);

        setMinimumSize(300, 180);
        resize(400, 250);

        connect(m_closeBtn, &QPushButton::clicked, this, &TaskLogOverlay::dismiss);

        connect(m_buffer, &TaskLogBuffer::messageBuffered,
                this, [this](Log::LoggerID id, QString, Log::Message msg) {
            if (id == m_currentId && isVisible())
                appendColoredLine(msg);
        });

        hide();
    }

    void TaskLogOverlay::appendColoredLine(const Log::Message& msg)
    {
        QColor color = colorForLevel(msg.getLevel());
        QString text = QString::fromStdString(msg.getText());
        QString html = QString("<span style=\"color:%1;\">%2</span>")
            .arg(color.name())
            .arg(text.toHtmlEscaped());
        m_textEdit->append(html);
    }

    void TaskLogOverlay::showForTask(const QString& taskName, Log::LoggerID loggerID)
    {
        m_currentTask = taskName;
        m_currentId = loggerID;
        m_titleLabel->setText("Log: " + taskName);
        m_textEdit->clear();

        auto history = m_buffer->messagesFor(loggerID);
        if (history.isEmpty())
        {
            m_textEdit->setPlaceholderText("No log output from " + taskName);
        }
        else
        {
            m_textEdit->setPlaceholderText("No log output");
            for (const auto& msg : history)
                appendColoredLine(msg);
        }

        show();
        raise();
        emit overlayMoved();
    }

    void TaskLogOverlay::dismiss()
    {
        hide();
        m_currentTask.clear();
        m_currentId = 0;
        emit dismissed();
    }

    void TaskLogOverlay::clearForTask(const QString& taskName)
    {
        if (m_currentTask == taskName && isVisible())
        {
            m_textEdit->clear();
            m_textEdit->setPlaceholderText("No log output from " + taskName);
        }
    }

    TaskLogOverlay::InteractMode TaskLogOverlay::hitTest(const QPoint& pos) const
    {
        bool atRight = pos.x() >= width() - kResizeMargin;
        bool atBottom = pos.y() >= height() - kResizeMargin;

        if (atRight && atBottom)
            return ResizingCorner;
        if (atRight)
            return ResizingRight;
        if (atBottom)
            return ResizingBottom;
        if (pos.y() <= kTitleBarHeight)
            return Dragging;
        return None;
    }

    void TaskLogOverlay::updateCursorForPos(const QPoint& pos)
    {
        switch (hitTest(pos))
        {
            case ResizingCorner: setCursor(Qt::SizeFDiagCursor); break;
            case ResizingRight:  setCursor(Qt::SizeHorCursor);   break;
            case ResizingBottom: setCursor(Qt::SizeVerCursor);   break;
            case Dragging:       setCursor(Qt::OpenHandCursor);  break;
            default:             setCursor(Qt::ArrowCursor);     break;
        }
    }

    void TaskLogOverlay::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton)
        {
            InteractMode mode = hitTest(event->pos());
            if (mode == Dragging)
            {
                m_mode = Dragging;
                m_dragOffset = event->pos();
                setCursor(Qt::ClosedHandCursor);
                event->accept();
                return;
            }
            if (mode == ResizingCorner || mode == ResizingRight || mode == ResizingBottom)
            {
                m_mode = mode;
                m_resizeStartGeom = geometry();
                m_resizeStartPos = event->globalPos();
                event->accept();
                return;
            }
        }
        QFrame::mousePressEvent(event);
    }

    void TaskLogOverlay::mouseMoveEvent(QMouseEvent* event)
    {
        if (m_mode == Dragging)
        {
            QPoint newPos = mapToParent(event->pos() - m_dragOffset);
            move(newPos);
            clampToParent();
            emit overlayMoved();
            event->accept();
            return;
        }
        if (m_mode == ResizingCorner || m_mode == ResizingRight || m_mode == ResizingBottom)
        {
            QPoint delta = event->globalPos() - m_resizeStartPos;
            QRect newGeom = m_resizeStartGeom;

            if (m_mode == ResizingRight || m_mode == ResizingCorner)
                newGeom.setWidth(qMax(minimumWidth(), m_resizeStartGeom.width() + delta.x()));
            if (m_mode == ResizingBottom || m_mode == ResizingCorner)
                newGeom.setHeight(qMax(minimumHeight(), m_resizeStartGeom.height() + delta.y()));

            // clamp to parent bounds
            if (parentWidget())
            {
                QRect parentRect = parentWidget()->rect();
                if (newGeom.right() > parentRect.right())
                    newGeom.setRight(parentRect.right());
                if (newGeom.bottom() > parentRect.bottom())
                    newGeom.setBottom(parentRect.bottom());
            }

            setGeometry(newGeom);
            emit overlayMoved();
            event->accept();
            return;
        }

        // hover cursor update
        updateCursorForPos(event->pos());
        QFrame::mouseMoveEvent(event);
    }

    void TaskLogOverlay::mouseReleaseEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton && m_mode != None)
        {
            m_mode = None;
            updateCursorForPos(event->pos());
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }

    void TaskLogOverlay::clampToParent()
    {
        if (!parentWidget())
            return;
        QRect parentRect = parentWidget()->rect();
        QPoint pos = this->pos();
        pos.setX(qBound(0, pos.x(), parentRect.width() - width()));
        pos.setY(qBound(0, pos.y(), parentRect.height() - height()));
        move(pos);
    }
}
}

#endif
