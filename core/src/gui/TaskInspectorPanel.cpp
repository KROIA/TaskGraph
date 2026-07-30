#include "gui/TaskInspectorPanel.h"

#if defined(QT_WIDGETS_ENABLED)
#include "TaskScheduler.h"
#include "Task.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFont>

namespace TaskGraph
{
namespace Gui
{
    TaskInspectorPanel::TaskInspectorPanel(TaskScheduler* scheduler, QWidget* parent)
        : QWidget(parent)
        , m_scheduler(scheduler)
    {
        auto* outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(8, 8, 8, 8);

        auto* titleLabel = new QLabel("Inspector", this);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize() + 1);
        titleLabel->setFont(titleFont);
        outerLayout->addWidget(titleLabel);

        m_noSelectionLabel = new QLabel("No task selected", this);
        m_noSelectionLabel->setStyleSheet("QLabel { color: #888888; font-style: italic; }");
        outerLayout->addWidget(m_noSelectionLabel);

        m_formContainer = new QWidget(this);
        auto* form = new QFormLayout(m_formContainer);
        form->setContentsMargins(0, 4, 0, 0);
        form->setSpacing(4);
        form->setLabelAlignment(Qt::AlignRight);

        m_nameValue = new QLabel(m_formContainer);
        m_nameValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow("Name:", m_nameValue);

        m_statusValue = new QLabel(m_formContainer);
        form->addRow("Status:", m_statusValue);

        m_affinityValue = new QLabel(m_formContainer);
        form->addRow("Affinity:", m_affinityValue);

        m_weightValue = new QLabel(m_formContainer);
        form->addRow("Weight:", m_weightValue);

        m_timeoutValue = new QLabel(m_formContainer);
        form->addRow("Timeout:", m_timeoutValue);

        m_retriesValue = new QLabel(m_formContainer);
        form->addRow("Max retries:", m_retriesValue);

        m_backoffValue = new QLabel(m_formContainer);
        form->addRow("Retry backoff:", m_backoffValue);

        m_depsValue = new QLabel(m_formContainer);
        m_depsValue->setWordWrap(true);
        form->addRow("Dependencies:", m_depsValue);

        m_errorValue = new QLabel(m_formContainer);
        m_errorValue->setWordWrap(true);
        m_errorValue->setStyleSheet("QLabel { color: #ff6e6e; }");
        form->addRow("Last error:", m_errorValue);

        m_resultValue = new QLabel(m_formContainer);
        form->addRow("Result:", m_resultValue);

        outerLayout->addWidget(m_formContainer);
        outerLayout->addStretch(1);

        m_formContainer->hide();

        // live status updates from scheduler
        connect(m_scheduler, &TaskScheduler::taskStarted, this, [this](QString name) {
            if (name == m_currentName) refresh();
        });
        connect(m_scheduler, &TaskScheduler::taskFinished, this, [this](QString name) {
            if (name == m_currentName) refresh();
        });
        connect(m_scheduler, &TaskScheduler::taskFailed, this, [this](QString name, QString) {
            if (name == m_currentName) refresh();
        });
        connect(m_scheduler, &TaskScheduler::wasReset, this, [this]() {
            if (m_currentTask) refresh();
        });
    }

    void TaskInspectorPanel::inspectTask(const QString& taskName)
    {
        m_currentName = taskName;
        m_currentTask.reset();

        auto graph = m_scheduler->getTaskGraph();
        for (const auto& layer : graph)
        {
            for (const auto& task : layer)
            {
                if (QString::fromStdString(task->getName()) == taskName)
                {
                    m_currentTask = task;
                    break;
                }
            }
            if (m_currentTask) break;
        }

        if (m_currentTask)
        {
            m_noSelectionLabel->hide();
            m_formContainer->show();
            refresh();
        }
        else
        {
            clearSelection();
        }
    }

    void TaskInspectorPanel::clearSelection()
    {
        m_currentTask.reset();
        m_currentName.clear();
        m_formContainer->hide();
        m_noSelectionLabel->show();
    }

    void TaskInspectorPanel::refresh()
    {
        if (!m_currentTask)
            return;

        m_nameValue->setText(QString::fromStdString(m_currentTask->getName()));
        m_statusValue->setText(statusToString(static_cast<int>(m_currentTask->getStatus())));

        m_affinityValue->setText(
            m_currentTask->getAffinity() == Task::TaskAffinity::Gui ? "Gui" : "Any");

        m_weightValue->setText(QString::number(m_currentTask->getWeight(), 'f', 2));

        auto timeout = m_currentTask->getTimeout();
        m_timeoutValue->setText(timeout.count() > 0
            ? QString::number(timeout.count()) + " ms"
            : "none");

        m_retriesValue->setText(QString::number(m_currentTask->getMaxRetries()));

        auto backoff = m_currentTask->getRetryBackoff();
        m_backoffValue->setText(backoff.count() > 0
            ? QString::number(backoff.count()) + " ms"
            : "none");

        auto deps = m_currentTask->getDependencies();
        if (deps.empty())
        {
            m_depsValue->setText("none");
        }
        else
        {
            QStringList names;
            for (const auto& dep : deps)
                names.append(QString::fromStdString(dep->getName()));
            m_depsValue->setText(names.join(", "));
        }

        QString lastErr = m_currentTask->getLastError();
        if (lastErr.isEmpty())
        {
            m_errorValue->setText("none");
            m_errorValue->setStyleSheet("QLabel { color: #888888; }");
        }
        else
        {
            m_errorValue->setText(lastErr);
            m_errorValue->setStyleSheet("QLabel { color: #ff6e6e; }");
        }

        bool hasResult = m_currentTask->getResult().has_value();
        m_resultValue->setText(hasResult ? "present" : "empty");
    }

    QString TaskInspectorPanel::statusToString(int status)
    {
        switch (static_cast<Task::Status>(status))
        {
            case Task::Status::Pending:   return "Pending";
            case Task::Status::Ready:     return "Ready";
            case Task::Status::Running:   return "Running";
            case Task::Status::Done:      return "Done";
            case Task::Status::Failed:    return "Failed";
            case Task::Status::Cancelled: return "Cancelled";
            case Task::Status::Skipped:   return "Skipped";
            default:                      return "Unknown";
        }
    }
}
}

#endif
