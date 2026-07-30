#pragma once

#include "TaskGraph_global.h"
#include "gui/FeatureSet.h"
#include <QWidget>
#include <QString>
#include <memory>

class QLabel;
class QFormLayout;
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QPushButton;
class QListWidget;
class QVBoxLayout;

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
        void setFeatures(const FeatureSet& fs);
        void setEditingEnabled(bool idle);

    signals:
        void graphStructureChanged();

    private:
        void refresh();
        void rebuildDepsList();
        void applyConfigChanges();
        void onRemoveDependency();
        void onAddDependency();
        void onAddTask();
        void onRemoveTask();
        void onSpawnChild();
        static QString statusToString(int status);

        TaskScheduler* m_scheduler;
        std::shared_ptr<Task> m_currentTask;
        QString m_currentName;
        FeatureSet m_features;
        bool m_idle = true;
        bool m_blockSignals = false;

        QLabel* m_noSelectionLabel = nullptr;
        QWidget* m_formContainer = nullptr;

        // read-only fields
        QLabel* m_nameValue = nullptr;
        QLabel* m_statusValue = nullptr;
        QLabel* m_errorValue = nullptr;
        QLabel* m_resultValue = nullptr;

        // editable fields (EditTaskConfig)
        QComboBox* m_affinityCombo = nullptr;
        QDoubleSpinBox* m_weightSpin = nullptr;
        QSpinBox* m_timeoutSpin = nullptr;
        QSpinBox* m_retriesSpin = nullptr;
        QSpinBox* m_backoffSpin = nullptr;

        // dependency editing (EditDependencies)
        QWidget* m_depsContainer = nullptr;
        QListWidget* m_depsList = nullptr;
        QPushButton* m_removeDepBtn = nullptr;
        QComboBox* m_addDepCombo = nullptr;
        QPushButton* m_addDepBtn = nullptr;

        // structural actions
        QPushButton* m_addTaskBtn = nullptr;
        QPushButton* m_removeTaskBtn = nullptr;
        QPushButton* m_spawnChildBtn = nullptr;
    };
}
}
