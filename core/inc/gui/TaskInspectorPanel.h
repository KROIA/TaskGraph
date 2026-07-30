#pragma once

#include "TaskGraph_global.h"
#include <QWidget>
#include <QString>
#include <memory>

class QLabel;
class QFormLayout;

namespace TaskGraph
{
    class Task;
    class TaskScheduler;

namespace Gui
{
    class TASK_GRAPH_API TaskInspectorPanel : public QWidget
    {
        Q_OBJECT
    public:
        explicit TaskInspectorPanel(TaskScheduler* scheduler, QWidget* parent = nullptr);

        void inspectTask(const QString& taskName);
        void clearSelection();

    private:
        void refresh();
        static QString statusToString(int status);

        TaskScheduler* m_scheduler;
        std::shared_ptr<Task> m_currentTask;
        QString m_currentName;

        QLabel* m_nameValue = nullptr;
        QLabel* m_statusValue = nullptr;
        QLabel* m_affinityValue = nullptr;
        QLabel* m_weightValue = nullptr;
        QLabel* m_timeoutValue = nullptr;
        QLabel* m_retriesValue = nullptr;
        QLabel* m_backoffValue = nullptr;
        QLabel* m_depsValue = nullptr;
        QLabel* m_errorValue = nullptr;
        QLabel* m_resultValue = nullptr;
        QLabel* m_noSelectionLabel = nullptr;
        QWidget* m_formContainer = nullptr;
    };
}
}
