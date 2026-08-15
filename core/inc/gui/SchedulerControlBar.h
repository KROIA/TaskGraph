#pragma once

#include "TaskGraph_global.h"

#include <QWidget>

class QPushButton;
class QProgressBar;
class QLabel;

namespace TaskGraph
{
    class TaskScheduler;

namespace Gui
{
    class TASK_GRAPH_API SchedulerControlBar : public QWidget
    {
        Q_OBJECT
    public:
        explicit SchedulerControlBar(TaskScheduler* scheduler, QWidget* parent = nullptr);

        void setRunControlsEnabled(bool enabled);
        void setRunControlsVisible(bool visible);

    private slots:
        void onRunClicked();
        void onCancelClicked();
        void onResetClicked();
        void onProgressUpdate(int progress);
        void refreshThreadStats();

    private:
        TaskScheduler* m_scheduler;
        QPushButton* m_runBtn;
        QPushButton* m_cancelBtn;
        QPushButton* m_resetBtn;
        QProgressBar* m_progressBar;
        QLabel* m_threadLabel;
    };
}
}
