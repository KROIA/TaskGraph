#include "gui/SchedulerControlBar.h"

#if defined(QT_WIDGETS_ENABLED)
#include "TaskScheduler.h"
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>

namespace TaskGraph
{
namespace Gui
{
    SchedulerControlBar::SchedulerControlBar(TaskScheduler* scheduler, QWidget* parent)
        : QWidget(parent)
        , m_scheduler(scheduler)
    {
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);

        m_runBtn = new QPushButton("Run", this);
        m_cancelBtn = new QPushButton("Cancel", this);
        m_resetBtn = new QPushButton("Reset", this);
        m_progressBar = new QProgressBar(this);
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        m_threadLabel = new QLabel("Threads: 0/0", this);

        layout->addWidget(m_runBtn);
        layout->addWidget(m_cancelBtn);
        layout->addWidget(m_resetBtn);
        layout->addWidget(m_progressBar, 1);
        layout->addWidget(m_threadLabel);

        connect(m_runBtn, &QPushButton::clicked, this, &SchedulerControlBar::onRunClicked);
        connect(m_cancelBtn, &QPushButton::clicked, this, &SchedulerControlBar::onCancelClicked);
        connect(m_resetBtn, &QPushButton::clicked, this, &SchedulerControlBar::onResetClicked);

        connect(m_scheduler, &TaskScheduler::progressUpdate, this, &SchedulerControlBar::onProgressUpdate);

        // poll thread stats at 250ms
        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &SchedulerControlBar::refreshThreadStats);
        timer->start(250);
    }

    void SchedulerControlBar::setRunControlsEnabled(bool enabled)
    {
        m_runBtn->setEnabled(enabled);
        m_cancelBtn->setEnabled(enabled);
        m_resetBtn->setEnabled(enabled);
    }

    void SchedulerControlBar::setRunControlsVisible(bool visible)
    {
        m_runBtn->setVisible(visible);
        m_cancelBtn->setVisible(visible);
        m_resetBtn->setVisible(visible);
    }

    void SchedulerControlBar::onRunClicked()
    {
        m_scheduler->runTasksAsync();
    }

    void SchedulerControlBar::onCancelClicked()
    {
        m_scheduler->cancel();
    }

    void SchedulerControlBar::onResetClicked()
    {
        m_scheduler->resetTasks();
    }

    void SchedulerControlBar::onProgressUpdate(int progress)
    {
        m_progressBar->setValue(progress);
    }

    void SchedulerControlBar::refreshThreadStats()
    {
        unsigned int busy = m_scheduler->getBusyThreadCount();
        unsigned int total = m_scheduler->getThreadCount();
        m_threadLabel->setText(QString("Threads: %1/%2").arg(busy).arg(total));
    }
}
}

#endif
