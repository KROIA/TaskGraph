#include "gui/ITaskGraphComponentFactory.h"

#if defined(QT_WIDGETS_ENABLED)
#include "gui/TaskGraphScene.h"
#include "gui/TaskGraphView.h"
#include "gui/SchedulerControlBar.h"
#include "gui/TaskInspectorPanel.h"
#include "gui/AggregateTaskLogView.h"
#include "gui/TaskLogOverlay.h"
#include "gui/GuiPromptService.h"

namespace TaskGraph
{
namespace Gui
{
    class DefaultComponentFactory : public ITaskGraphComponentFactory
    {
    public:
        explicit DefaultComponentFactory(FeatureSet fs) : m_features(fs) {}

        const FeatureSet& features() const override { return m_features; }

        TaskGraphScene* createScene(TaskScheduler* scheduler, QObject* parent) override
        {
            return new TaskGraphScene(scheduler, parent);
        }

        TaskGraphView* createView(TaskScheduler* scheduler, QWidget* parent) override
        {
            TG_UNUSED(scheduler);
            if (!m_features.has(Feature::ShowGraph))
                return nullptr;
            return new TaskGraphView(parent);
        }

        SchedulerControlBar* createControlBar(TaskScheduler* scheduler, QWidget* parent) override
        {
            if (!m_features.has(Feature::RunControls))
                return nullptr;
            return new SchedulerControlBar(scheduler, parent);
        }

        QWidget* createInspector(TaskScheduler* scheduler, QWidget* parent) override
        {
            if (!m_features.has(Feature::ShowInspector))
                return nullptr;
            return new TaskInspectorPanel(scheduler, parent);
        }

        QWidget* createLogView(TaskScheduler* scheduler, TaskLogBuffer* logBuffer, QWidget* parent) override
        {
            TG_UNUSED(scheduler);
            if (!m_features.has(Feature::ShowLog))
                return nullptr;
            return new AggregateTaskLogView(logBuffer, parent);
        }

        TaskLogOverlay* createLogOverlay(TaskLogBuffer* logBuffer, QWidget* parent) override
        {
            return new TaskLogOverlay(logBuffer, parent);
        }

        GuiPromptService* createGuiPromptService(TaskScheduler* scheduler, QWidget* parentWidget, QObject* parent) override
        {
            if (!m_features.has(Feature::GuiRoundTrip))
                return nullptr;
            return new GuiPromptService(scheduler, parentWidget, parent);
        }

    private:
        FeatureSet m_features;
    };

    std::shared_ptr<ITaskGraphComponentFactory> makeFactory(FeatureSet features)
    {
        return std::make_shared<DefaultComponentFactory>(features);
    }

    std::shared_ptr<ITaskGraphComponentFactory> presetViewOnlyFactory()
    {
        return makeFactory(FeatureSet::viewOnly());
    }

    std::shared_ptr<ITaskGraphComponentFactory> presetMonitorFactory()
    {
        return makeFactory(FeatureSet::monitor());
    }

    std::shared_ptr<ITaskGraphComponentFactory> presetEditorFactory()
    {
        return makeFactory(FeatureSet::editor());
    }
}
}
#else
namespace TaskGraph
{
namespace Gui
{
    std::shared_ptr<ITaskGraphComponentFactory> makeFactory(FeatureSet)
    {
        return nullptr;
    }

    std::shared_ptr<ITaskGraphComponentFactory> presetViewOnlyFactory()
    {
        return nullptr;
    }

    std::shared_ptr<ITaskGraphComponentFactory> presetMonitorFactory()
    {
        return nullptr;
    }

    std::shared_ptr<ITaskGraphComponentFactory> presetEditorFactory()
    {
        return nullptr;
    }
}
}
#endif
