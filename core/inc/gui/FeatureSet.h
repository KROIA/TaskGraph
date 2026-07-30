#pragma once

#include "TaskGraph_global.h"
#include <cstdint>
#include <initializer_list>

namespace TaskGraph
{
namespace Gui
{
    enum class Feature : uint32_t
    {
        ShowGraph       = 0,
        ShowProgress    = 1,
        ShowThreadStats = 2,
        ShowInspector   = 3,
        ShowLog         = 4,
        ZoomPan         = 5,
        NodeSelection   = 6,
        RunControls     = 7,
        GuiRoundTrip    = 8,
        EditAddTask     = 9,
        EditRemoveTask  = 10,
        EditDependencies = 11,
        EditTaskConfig  = 12
    };

    class TASK_GRAPH_API FeatureSet
    {
    public:
        FeatureSet() : m_bits(0) {}
        FeatureSet(std::initializer_list<Feature> features) : m_bits(0)
        {
            for (auto f : features)
                set(f);
        }

        FeatureSet& set(Feature f, bool on = true)
        {
            if (on)
                m_bits |= (1u << static_cast<uint32_t>(f));
            else
                m_bits &= ~(1u << static_cast<uint32_t>(f));
            return *this;
        }

        bool has(Feature f) const
        {
            return (m_bits & (1u << static_cast<uint32_t>(f))) != 0;
        }

        bool anyEdit() const
        {
            return has(Feature::EditAddTask) || has(Feature::EditRemoveTask)
                || has(Feature::EditDependencies) || has(Feature::EditTaskConfig);
        }

        static FeatureSet viewOnly()
        {
            return { Feature::ShowGraph, Feature::ZoomPan, Feature::NodeSelection };
        }

        static FeatureSet monitor()
        {
            return { Feature::ShowGraph, Feature::ZoomPan, Feature::NodeSelection,
                     Feature::ShowProgress, Feature::ShowThreadStats,
                     Feature::ShowLog, Feature::RunControls, Feature::GuiRoundTrip };
        }

        static FeatureSet editor()
        {
            FeatureSet fs = monitor();
            fs.set(Feature::ShowInspector);
            fs.set(Feature::ShowLog);
            fs.set(Feature::EditAddTask);
            fs.set(Feature::EditRemoveTask);
            fs.set(Feature::EditDependencies);
            fs.set(Feature::EditTaskConfig);
            return fs;
        }

    private:
        uint32_t m_bits;
    };
}
}
