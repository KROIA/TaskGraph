#include "gui/AggregateTaskLogView.h"

#if defined(QT_WIDGETS_ENABLED)
#include "gui/TaskLogBuffer.h"
#include "LogMessage.h"
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHeaderView>

namespace TaskGraph
{
namespace Gui
{
    static void colorTreeItem(QTreeWidgetItem* item, const Log::Message& msg)
    {
        QColor color = Log::Message::getLevelColor(msg.getLevel()).toQColor();
        item->setForeground(0, QBrush(color));
    }

    AggregateTaskLogView::AggregateTaskLogView(
        TaskLogBuffer* buffer,
        QWidget* parent)
        : QWidget(parent)
        , m_buffer(buffer)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        m_tree = new QTreeWidget(this);
        m_tree->setHeaderLabels({"Task / Log Entry"});
        m_tree->header()->setStretchLastSection(true);
        m_tree->setRootIsDecorated(true);
        m_tree->setAnimated(true);
        layout->addWidget(m_tree);

        connect(m_buffer, &TaskLogBuffer::messageBuffered,
                this, [this](Log::LoggerID, QString taskName, Log::Message msg) {
            m_tree->scrollToItem(appendMessage(taskName, msg));
        });

        auto tasks = m_buffer->tasksInOrder();
        for (const auto& taskName : tasks)
        {
            auto msgs = m_buffer->messagesForTask(taskName);
            for (const auto& msg : msgs)
                appendMessage(taskName, msg);
        }
    }

    QTreeWidgetItem* AggregateTaskLogView::appendMessage(const QString& taskName, const Log::Message& msg)
    {
        QTreeWidgetItem* parent = ensureTaskItem(taskName);
        auto* child = new QTreeWidgetItem(parent);
        child->setText(0, QString::fromStdString(msg.getText()));
        colorTreeItem(child, msg);
        parent->setExpanded(true);
        return child;
    }

    void AggregateTaskLogView::clearAll()
    {
        m_tree->clear();
        m_taskItems.clear();
    }

    QTreeWidgetItem* AggregateTaskLogView::ensureTaskItem(const QString& taskName)
    {
        auto it = m_taskItems.find(taskName);
        if (it != m_taskItems.end())
            return it.value();

        auto* item = new QTreeWidgetItem();
        item->setText(0, taskName);
        QFont f = item->font(0);
        f.setBold(true);
        item->setFont(0, f);
        m_tree->addTopLevelItem(item);
        m_taskItems[taskName] = item;
        return item;
    }
}
}

#endif
