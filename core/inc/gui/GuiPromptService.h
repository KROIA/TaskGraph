#pragma once

#include "TaskGraph_global.h"
#include <QObject>
#include <QVariant>
#include <QString>

class QDialog;

namespace TaskGraph
{
    class TaskScheduler;

namespace Gui
{
    class TASK_GRAPH_API GuiPromptService : public QObject
    {
        Q_OBJECT
    public:
        explicit GuiPromptService(TaskScheduler* scheduler, QWidget* parentWidget, QObject* parent = nullptr);

    private slots:
        void onGuiEventRequested(int requestId, QString taskName, QVariant payload);
        void onCancelled();

    private:
        TaskScheduler* m_scheduler;
        QWidget* m_parentWidget;
        QDialog* m_activeDialog = nullptr;
        int m_activeRequestId = -1;
    };
}
}
