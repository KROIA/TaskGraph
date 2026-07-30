#include "gui/TaskInspectorPanel.h"

#if defined(QT_WIDGETS_ENABLED)
#include "TaskScheduler.h"
#include "Task.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QInputDialog>
#include <QMessageBox>

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

        // structural buttons at top
        auto* structRow = new QHBoxLayout();
        m_addTaskBtn = new QPushButton("+ Task", this);
        m_removeTaskBtn = new QPushButton("- Task", this);
        m_spawnChildBtn = new QPushButton("Spawn Child", this);
        m_spawnChildBtn->setToolTip("Add a child task to the selected running task (only during run)");
        structRow->addWidget(m_addTaskBtn);
        structRow->addWidget(m_removeTaskBtn);
        structRow->addWidget(m_spawnChildBtn);
        structRow->addStretch(1);
        outerLayout->addLayout(structRow);

        connect(m_addTaskBtn, &QPushButton::clicked, this, &TaskInspectorPanel::onAddTask);
        connect(m_removeTaskBtn, &QPushButton::clicked, this, &TaskInspectorPanel::onRemoveTask);
        connect(m_spawnChildBtn, &QPushButton::clicked, this, &TaskInspectorPanel::onSpawnChild);

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

        m_affinityCombo = new QComboBox(m_formContainer);
        m_affinityCombo->addItem("Any", static_cast<int>(Task::TaskAffinity::Any));
        m_affinityCombo->addItem("Gui", static_cast<int>(Task::TaskAffinity::Gui));
        form->addRow("Affinity:", m_affinityCombo);

        m_weightSpin = new QDoubleSpinBox(m_formContainer);
        m_weightSpin->setRange(0.01, 1000.0);
        m_weightSpin->setDecimals(2);
        m_weightSpin->setSingleStep(0.1);
        form->addRow("Weight:", m_weightSpin);

        m_timeoutSpin = new QSpinBox(m_formContainer);
        m_timeoutSpin->setRange(0, 600000);
        m_timeoutSpin->setSuffix(" ms");
        m_timeoutSpin->setSpecialValueText("none");
        form->addRow("Timeout:", m_timeoutSpin);

        m_retriesSpin = new QSpinBox(m_formContainer);
        m_retriesSpin->setRange(0, 100);
        form->addRow("Max retries:", m_retriesSpin);

        m_backoffSpin = new QSpinBox(m_formContainer);
        m_backoffSpin->setRange(0, 60000);
        m_backoffSpin->setSuffix(" ms");
        m_backoffSpin->setSpecialValueText("none");
        form->addRow("Retry backoff:", m_backoffSpin);

        // dependency editing section
        m_depsContainer = new QWidget(m_formContainer);
        auto* depsLayout = new QVBoxLayout(m_depsContainer);
        depsLayout->setContentsMargins(0, 0, 0, 0);
        depsLayout->setSpacing(2);

        m_depsList = new QListWidget(m_depsContainer);
        m_depsList->setMaximumHeight(80);
        depsLayout->addWidget(m_depsList);

        auto* depBtnRow = new QHBoxLayout();
        m_removeDepBtn = new QPushButton("Remove", m_depsContainer);
        m_removeDepBtn->setFixedHeight(24);
        depBtnRow->addWidget(m_removeDepBtn);

        m_addDepCombo = new QComboBox(m_depsContainer);
        m_addDepCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        depBtnRow->addWidget(m_addDepCombo);

        m_addDepBtn = new QPushButton("Add", m_depsContainer);
        m_addDepBtn->setFixedHeight(24);
        depBtnRow->addWidget(m_addDepBtn);
        depsLayout->addLayout(depBtnRow);

        form->addRow("Dependencies:", m_depsContainer);

        connect(m_removeDepBtn, &QPushButton::clicked, this, &TaskInspectorPanel::onRemoveDependency);
        connect(m_addDepBtn, &QPushButton::clicked, this, &TaskInspectorPanel::onAddDependency);

        m_errorValue = new QLabel(m_formContainer);
        m_errorValue->setWordWrap(true);
        m_errorValue->setStyleSheet("QLabel { color: #ff6e6e; }");
        form->addRow("Last error:", m_errorValue);

        m_resultValue = new QLabel(m_formContainer);
        form->addRow("Result:", m_resultValue);

        outerLayout->addWidget(m_formContainer);
        outerLayout->addStretch(1);

        m_formContainer->hide();

        // apply changes on spinbox/combo edits
        connect(m_affinityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { applyConfigChanges(); });
        connect(m_weightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) { applyConfigChanges(); });
        connect(m_timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyConfigChanges(); });
        connect(m_retriesSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyConfigChanges(); });
        connect(m_backoffSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { applyConfigChanges(); });

        // live updates
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

    void TaskInspectorPanel::setFeatures(const FeatureSet& fs)
    {
        m_features = fs;
        setEditingEnabled(m_idle);
    }

    void TaskInspectorPanel::setEditingEnabled(bool idle)
    {
        m_idle = idle;
        bool canEditConfig = idle && m_features.has(Feature::EditTaskConfig);
        bool canEditDeps = idle && m_features.has(Feature::EditDependencies);
        bool canAdd = m_features.has(Feature::EditAddTask);
        bool canRemove = idle && m_features.has(Feature::EditRemoveTask);

        m_affinityCombo->setEnabled(canEditConfig);
        m_weightSpin->setEnabled(canEditConfig);
        m_timeoutSpin->setEnabled(canEditConfig);
        m_retriesSpin->setEnabled(canEditConfig);
        m_backoffSpin->setEnabled(canEditConfig);

        m_removeDepBtn->setEnabled(canEditDeps);
        m_addDepBtn->setEnabled(canEditDeps);
        m_addDepCombo->setEnabled(canEditDeps);

        // add task: allowed when idle AND feature present
        m_addTaskBtn->setEnabled(idle && canAdd);
        m_addTaskBtn->setVisible(canAdd);

        m_removeTaskBtn->setEnabled(canRemove && m_currentTask != nullptr);
        m_removeTaskBtn->setVisible(m_features.has(Feature::EditRemoveTask));

        // spawn child: only during run, and only when a task is selected
        m_spawnChildBtn->setEnabled(!idle && canAdd && m_currentTask != nullptr);
        m_spawnChildBtn->setVisible(canAdd);
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
            setEditingEnabled(m_idle);
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
        setEditingEnabled(m_idle);
    }

    void TaskInspectorPanel::refresh()
    {
        if (!m_currentTask)
            return;

        m_blockSignals = true;

        m_nameValue->setText(QString::fromStdString(m_currentTask->getName()));
        m_statusValue->setText(statusToString(static_cast<int>(m_currentTask->getStatus())));

        int affinityIdx = m_affinityCombo->findData(
            static_cast<int>(m_currentTask->getAffinity()));
        if (affinityIdx >= 0)
            m_affinityCombo->setCurrentIndex(affinityIdx);

        m_weightSpin->setValue(m_currentTask->getWeight());

        m_timeoutSpin->setValue(
            static_cast<int>(m_currentTask->getTimeout().count()));

        m_retriesSpin->setValue(m_currentTask->getMaxRetries());

        m_backoffSpin->setValue(
            static_cast<int>(m_currentTask->getRetryBackoff().count()));

        rebuildDepsList();

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

        m_blockSignals = false;
    }

    void TaskInspectorPanel::rebuildDepsList()
    {
        m_depsList->clear();
        if (!m_currentTask)
            return;

        auto deps = m_currentTask->getDependencies();
        for (const auto& dep : deps)
            m_depsList->addItem(QString::fromStdString(dep->getName()));

        // populate add-dep combo with tasks that are NOT already dependencies and not self
        m_addDepCombo->clear();
        auto graph = m_scheduler->getTaskGraph();
        for (const auto& layer : graph)
        {
            for (const auto& task : layer)
            {
                if (task == m_currentTask)
                    continue;
                QString name = QString::fromStdString(task->getName());
                bool alreadyDep = false;
                for (const auto& dep : deps)
                {
                    if (dep == task)
                    {
                        alreadyDep = true;
                        break;
                    }
                }
                if (!alreadyDep)
                    m_addDepCombo->addItem(name);
            }
        }
    }

    void TaskInspectorPanel::applyConfigChanges()
    {
        if (m_blockSignals || !m_currentTask || !m_idle)
            return;

        auto affinity = static_cast<Task::TaskAffinity>(
            m_affinityCombo->currentData().toInt());
        m_currentTask->setAffinity(affinity);
        m_currentTask->setWeight(static_cast<float>(m_weightSpin->value()));
        m_currentTask->setTimeout(
            std::chrono::milliseconds(m_timeoutSpin->value()));
        m_currentTask->setMaxRetries(m_retriesSpin->value());
        m_currentTask->setRetryBackoff(
            std::chrono::milliseconds(m_backoffSpin->value()));
    }

    void TaskInspectorPanel::onRemoveDependency()
    {
        if (!m_currentTask || !m_idle)
            return;
        auto item = m_depsList->currentItem();
        if (!item)
            return;

        QString depName = item->text();
        auto deps = m_currentTask->getDependencies();
        m_currentTask->clearDependencies();
        for (const auto& dep : deps)
        {
            if (QString::fromStdString(dep->getName()) != depName)
                m_currentTask->addDependency(dep);
        }

        rebuildDepsList();
        emit graphStructureChanged();
    }

    void TaskInspectorPanel::onAddDependency()
    {
        if (!m_currentTask || !m_idle || m_addDepCombo->count() == 0)
            return;

        QString depName = m_addDepCombo->currentText();
        auto graph = m_scheduler->getTaskGraph();
        for (const auto& layer : graph)
        {
            for (const auto& task : layer)
            {
                if (QString::fromStdString(task->getName()) == depName)
                {
                    if (!m_currentTask->addDependency(task))
                    {
                        QMessageBox::warning(this, "Cycle Detected",
                            "Adding dependency \"" + depName +
                            "\" would create a cycle.");
                        return;
                    }
                    rebuildDepsList();
                    emit graphStructureChanged();
                    return;
                }
            }
        }
    }

    void TaskInspectorPanel::onAddTask()
    {
        if (!m_idle)
            return;

        bool ok = false;
        QString name = QInputDialog::getText(this, "Add Task",
            "Task name:", QLineEdit::Normal, "", &ok);
        if (!ok || name.trimmed().isEmpty())
            return;

        auto task = std::make_shared<Task>(name.trimmed().toStdString());
        task->setWorkFunction([n = name.trimmed().toStdString()](TaskContext& ctx) {
            ctx.log().logInfo(n + " started");
            for (int i = 0; i < 4; ++i)
            {
                if (ctx.isCancelRequested()) return;
                ctx.log().logInfo(n + " step " + std::to_string(i + 1) + "/4");
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
            ctx.log().logInfo(n + " done");
        });

        if (!m_scheduler->addTask(task))
        {
            QMessageBox::warning(this, "Add Failed",
                "Could not add task \"" + name + "\".");
            return;
        }
        emit graphStructureChanged();
    }

    void TaskInspectorPanel::onRemoveTask()
    {
        if (!m_currentTask || !m_idle)
            return;

        QString name = m_currentName;
        if (!m_scheduler->removeTask(m_currentTask))
        {
            QMessageBox::warning(this, "Remove Failed",
                "Could not remove task \"" + name + "\".");
            return;
        }
        clearSelection();
        emit graphStructureChanged();
    }

    void TaskInspectorPanel::onSpawnChild()
    {
        if (!m_currentTask || m_idle)
            return;

        bool ok = false;
        QString name = QInputDialog::getText(this, "Spawn Child Task",
            "Child task name:", QLineEdit::Normal, "", &ok);
        if (!ok || name.trimmed().isEmpty())
            return;

        auto child = std::make_shared<Task>(name.trimmed().toStdString());
        child->setWorkFunction([n = name.trimmed().toStdString()](TaskContext& ctx) {
            ctx.log().logInfo(n + " (spawned) started");
            for (int i = 0; i < 3; ++i)
            {
                if (ctx.isCancelRequested()) return;
                ctx.log().logInfo(n + " step " + std::to_string(i + 1) + "/3");
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            ctx.log().logInfo(n + " done");
        });

        if (!m_scheduler->addDynamicTask(child, m_currentTask.get()))
        {
            QMessageBox::warning(this, "Spawn Failed",
                "Could not spawn child task. The selected task may not be running.");
            return;
        }
        emit graphStructureChanged();
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
