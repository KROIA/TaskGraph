# TaskGraph

A C++23 / Qt5 task-graph scheduler library. Define tasks as nodes in a directed acyclic graph, wire up dependencies, and run them in parallel on a thread pool with ready-queue dispatch. TaskGraph provides GUI-thread integration via Qt's queued signal/slot mechanism, per-task logging through the Logger dependency, cooperative cancellation, error isolation, retries with backoff, timeouts, weighted float progress, result passing, pause/resume, dynamic child spawning at runtime, and an optional live Qt visualization widget (`TaskGraph::Gui`).

## Features

- **DAG dependency model** with cycle rejection at `addDependency` time
- **Ready-queue parallel dispatch** -- a task runs as soon as its dependencies complete, no barrier stalls
- **GUI-thread affinity** -- tasks can target `TaskAffinity::Any` (worker pool) or `TaskAffinity::Gui` (marshaled to the Qt event loop); all scheduler signals are queued-connection safe
- **Cooperative cancellation** -- `scheduler.cancel()` and per-task `task->cancel()`; poll via `ctx.isCancelRequested()` in the body
- **Error isolation** -- `FailurePolicy::FailFast` (default) skips dependents on failure; `FailurePolicy::ContinueOthers` lets independent branches finish
- **Retries with backoff** -- per-task `setMaxRetries(n)` and `setRetryBackoff(ms)`
- **Timeouts** -- per-task wall-clock timeout via `setTimeout(ms)`; fires `signalTimeout()` on expiry
- **Weighted float progress** -- per-task `setWeight(float)` for proportional `progressChangedF(float)` updates (0.0 to 1.0)
- **Result passing** -- `setResult(std::any)` / `getResult()` / `getResultAs<T>()` on tasks; `ctx.getDependencyResult<T>(dep)` in a body
- **TaskGroup** -- lightweight named collection; depend on a group to depend on all its members
- **Pause / resume** -- `scheduler.pause()` / `scheduler.resume()`
- **Dynamic spawn** -- `ctx.spawn(child)` from within a running task body; `scheduler.addDynamicTask(child, parent)` for external callers
- **Remove task** -- `scheduler.removeTask(task)` while idle; detaches from all dependency lists
- **Per-task logging** -- each `Task` has its own `Log::LogObject` via `task->logger()`; `ctx.log()` in bodies; scheduler-level `scheduler.logger()`
- **GUI round-trip** -- `ctx.askGui(payload)` blocks a worker until the GUI thread responds via `respondToGuiEvent`; cancellation-aware
- **Pluggable context** -- `scheduler.setContextFactory(...)` supplies an app-specific `TaskContext` subclass to every task body without templatizing the scheduler
- **Qt visualization** -- drop-in `TaskGraphWidget` with orthogonal edge routing, live status recolor, control bar, inspector panel, log views, and interactive editing (see [Qt Visualization Widget](#qt-visualization-widget))

## Requirements and Build

| Requirement | Version |
|---|---|
| C++ standard | C++23 |
| Qt | 5.15.x (Core, Widgets) |
| CMake | >= 3.20 |
| Compiler | MSVC (Windows) |

### Building

Using the provided build script:

```
build.bat x64-Debug
build.bat x64-Release
```

Or directly with CMake presets:

```
cmake --preset x64-Debug
cmake --build --preset x64-Debug
```

### CMake toggles

| Variable | Default | Description |
|---|---|---|
| `QT_ENABLE` | `ON` | Enable Qt integration |
| `QT_DEPLOY` | `ON` | Deploy Qt DLLs to the output directory |
| `COMPILE_EXAMPLES` | `ON` | Build the example application |
| `COMPILE_UNITTESTS` | `ON` | Build the unit test suite |

## Quick Start

### Lambda form

```cpp
#include "TaskGraph.h"

TaskGraph::TaskScheduler scheduler(4); // 4 worker threads

auto task1 = std::make_shared<TaskGraph::Task>("Step1");
task1->setWorkFunction([](TaskGraph::TaskContext& ctx) {
    ctx.log().logInfo("Step 1 running");
    ctx.setResult(std::any(42));
});

auto task2 = std::make_shared<TaskGraph::Task>("Step2");
// Capture task1 to read its result once Step1 has completed.
task2->setWorkFunction([task1](TaskGraph::TaskContext& ctx) {
    int fromStep1 = ctx.getDependencyResult<int>(*task1); // 42
    ctx.log().logInfo("Step 2 running with " + std::to_string(fromStep1));
});

task2->addDependency(task1);

scheduler.addTask(task1);
scheduler.addTask(task2);

scheduler.runTasks(); // blocks until all tasks complete
```

### Subclass form

```cpp
#include "TaskGraph.h"

class MyTask : public TaskGraph::Task
{
public:
    MyTask(const std::string& name) : TaskGraph::Task(name) {}

private:
    void work(TaskGraph::TaskContext& ctx) override
    {
        logger().logInfo(getName() + " started");
        int steps = 4;
        for (int i = 0; i < steps; ++i)
        {
            if (ctx.isCancelRequested())
                return;
            logger().logInfo(getName() + " step " + std::to_string(i + 1));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        logger().logInfo(getName() + " done");
    }
};
```

Both `work()` (no context) and `work(TaskContext&)` overrides are supported. The context-taking form is preferred as it provides cancellation polling, result passing, spawn, and GUI round-trip.

## Dependencies and DAG Construction

Build a diamond graph:

```cpp
auto a = std::make_shared<TaskGraph::Task>("A");
auto b = std::make_shared<TaskGraph::Task>("B");
auto c = std::make_shared<TaskGraph::Task>("C");
auto d = std::make_shared<TaskGraph::Task>("D");

// D depends on B and C; B and C both depend on A
b->addDependency(a);
c->addDependency(a);
d->addDependency(b);
d->addDependency(c);

scheduler.addTask(a);
scheduler.addTask(b);
scheduler.addTask(c);
scheduler.addTask(d);
```

Execution order: A runs first, then B and C in parallel, then D.

`addDependency` performs an incremental cycle check and returns `false` if the edge would create a cycle. Always check the return value when constructing graphs dynamically.

### TaskGroup

```cpp
TaskGraph::TaskGroup preprocessing("preprocessing");
preprocessing.addTask(taskA);
preprocessing.addTask(taskB);
preprocessing.addTask(taskC);

// taskD depends on all members of the group
taskD->addDependency(preprocessing);
```

## Progress and Signals

Connect to scheduler signals for live updates:

```cpp
QObject::connect(&scheduler, &TaskGraph::TaskScheduler::progressChangedF,
    [](float p) { qDebug() << "Progress:" << p; });

QObject::connect(&scheduler, &TaskGraph::TaskScheduler::taskStarted,
    [](QString name) { qDebug() << name << "started"; });

QObject::connect(&scheduler, &TaskGraph::TaskScheduler::taskFinished,
    [](QString name) { qDebug() << name << "finished"; });

QObject::connect(&scheduler, &TaskGraph::TaskScheduler::taskFailed,
    [](QString name, QString error) { qDebug() << name << "failed:" << error; });

// Lifecycle
QObject::connect(&scheduler, &TaskGraph::TaskScheduler::started, []() { /* ... */ });
QObject::connect(&scheduler, &TaskGraph::TaskScheduler::completed, []() { /* ... */ });
QObject::connect(&scheduler, &TaskGraph::TaskScheduler::cancelled, []() { /* ... */ });
QObject::connect(&scheduler, &TaskGraph::TaskScheduler::paused, []() { /* ... */ });
QObject::connect(&scheduler, &TaskGraph::TaskScheduler::resumed, []() { /* ... */ });
```

### Weighted progress

By default every task contributes equally. Assign weights for proportional reporting:

```cpp
heavyTask->setWeight(5.0f);   // counts 5x in progress
lightTask->setWeight(0.5f);   // counts 0.5x
```

`progressChangedF(float)` emits values from 0.0 to 1.0 based on completed weight / total weight.

All scheduler signals are safe to connect from the GUI thread using default (auto) connections. The scheduler emits from worker threads; Qt automatically promotes cross-thread connections to `QueuedConnection`, so slots run on the GUI event loop.

## Cancellation

```cpp
// Cancel the entire graph
scheduler.cancel();

// Cancel a single task
task->cancel();

// In a task body, poll cooperatively
void work(TaskGraph::TaskContext& ctx) override
{
    for (int i = 0; i < 1000; ++i)
    {
        if (ctx.isCancelRequested())
            return; // exits cleanly
        // ... do work ...
    }
}
```

After cancellation, pending tasks are marked `Cancelled` and dependents are `Skipped`. A subsequent `runTasks`/`runTasksAsync` call automatically resets all tasks to `Pending` before re-executing (no manual `resetTasks()` required).

## Errors, Retries, and Timeouts

### Error handling

Throwing an exception from a task body puts the task into `Failed` status. The error message is accessible via `task->getLastError()` and emitted via `taskFailed(name, error)`.

```cpp
task->setWorkFunction([](TaskGraph::TaskContext& ctx) {
    throw std::runtime_error("something went wrong");
});
```

### Failure policy

```cpp
// Default: abort remaining tasks when one fails
scheduler.setFailurePolicy(TaskGraph::TaskScheduler::FailurePolicy::FailFast);

// Alternative: let independent branches finish
scheduler.setFailurePolicy(TaskGraph::TaskScheduler::FailurePolicy::ContinueOthers);
```

### Retries and backoff

```cpp
task->setMaxRetries(3);
task->setRetryBackoff(std::chrono::milliseconds(500));
// On failure: retry up to 3 times, waiting 500ms between attempts
```

### Timeouts

```cpp
task->setTimeout(std::chrono::milliseconds(5000));
// If the body exceeds 5 seconds, the task is interrupted (timeout signal)
```

## Passing Results

Tasks can produce and consume results via `std::any`:

```cpp
// Producer
producerTask->setWorkFunction([](TaskGraph::TaskContext& ctx) {
    ctx.setResult(std::any(std::string("hello")));
});

// Consumer (must depend on producerTask)
consumerTask->setWorkFunction([&producerTask](TaskGraph::TaskContext& ctx) {
    auto value = ctx.getDependencyResult<std::string>(*producerTask);
    ctx.log().logInfo("Got: " + value);
});
```

Outside a task body, read results from completed tasks:

```cpp
auto result = TaskGraph::getResultAs<int>(*task);
```

`getDependencyResult` throws `std::runtime_error` if the dependency has no result set, and `std::bad_any_cast` on type mismatch.

## Advanced

### Dynamic child spawning

From within a running task body, spawn a child that runs after the current task:

```cpp
void work(TaskGraph::TaskContext& ctx) override
{
    auto child = std::make_shared<TaskGraph::Task>("DynamicChild");
    child->setWorkFunction([]() { /* ... */ });
    ctx.spawn(child); // child runs after this task finishes
}
```

Externally (e.g., from the GUI), use `addDynamicTask`:

```cpp
scheduler.addDynamicTask(child, runningParentTask.get());
```

### Removing tasks

While the scheduler is idle:

```cpp
scheduler.removeTask(task); // detaches from all dependency lists, removes from graph
```

Returns `false` if the scheduler is currently running.

### Re-running after cancel

`runTasks` / `runTasksAsync` automatically resets all tasks to `Pending` at the start of each run. There is no need to call `resetTasks()` between runs. Each invocation is a fresh full re-run; prior results are cleared.

### Custom execution context

By default every task body receives a base `TaskContext`. To hand tasks an application-specific context -- carrying app services (resource maps, config, IO wrappers bound to the task's logger) -- supply a factory. The scheduler builds your derived context per task-run and passes it to the body as a base `TaskContext&`; downcast in the body.

```cpp
struct MyCtx : TaskGraph::TaskContext
{
    MyCtx(TaskGraph::Task* t, TaskGraph::TaskScheduler* s, AppServices* svc)
        : TaskGraph::TaskContext(t, s), services(svc) {}
    AppServices* services;
};

scheduler.setContextFactory(
    [svc](TaskGraph::Task* t, TaskGraph::TaskScheduler* s)
        -> std::unique_ptr<TaskGraph::TaskContext>
    {
        return std::make_unique<MyCtx>(t, s, svc);
    });

task->setWorkFunction([](TaskGraph::TaskContext& ctx) {
    auto& my = static_cast<MyCtx&>(ctx);
    my.services->doThing();
    my.setResult(std::any(1)); // base facilities still work
});
```

Pass `{}` to `setContextFactory` to clear it and restore the default base context. One context is built per task-run and reused across that task's retry attempts, then destroyed when the run returns (`TaskContext` has a virtual destructor). The factory is invoked on the worker thread that runs the task, so it must be safe to call concurrently -- a typical implementation just allocates a struct holding references. `Task` and `TaskScheduler` remain non-template `QObject`s; the work-function signature is unchanged.

### Task affinity

```cpp
task->setAffinity(TaskGraph::Task::TaskAffinity::Gui);
// This task's body will execute on the GUI thread via QMetaObject::invokeMethod
```

Use `TaskAffinity::Any` (default) for worker-pool execution.

## Per-Task Logging

Every task has its own `Log::LogObject` instance:

```cpp
// From within a task body (subclass)
logger().logInfo("processing item " + std::to_string(i));
logger().logWarning("slow path taken");
logger().logError("failed to open file");

// From within a TaskContext body (lambda)
ctx.log().logInfo("...");

// Scheduler-level logger
scheduler.logger().logInfo("scheduler message");
```

Log messages carry the task's `LoggerID` and can be filtered per-task in the GUI visualization.

## GUI Round-Trip

A worker task can request input from the GUI thread:

```cpp
class AskGuiTask : public TaskGraph::Task
{
    void work(TaskGraph::TaskContext& ctx) override
    {
        QVariant response = ctx.askGui(QVariant("Enter a value:"));
        if (!response.isValid())
        {
            logger().logWarning("cancelled or no response");
            return;
        }
        std::string val = response.toString().toStdString();
        logger().logInfo("got: " + val);
    }
};
```

On the scheduler side, `guiEventRequested(int requestId, QString taskName, QVariant payload)` is emitted. The GUI code (or `GuiPromptService` in `TaskGraph::Gui`) presents a dialog and calls `scheduler.respondToGuiEvent(requestId, response)` to unblock the worker.

**Deadlock caveat:** Never call `askGui` from the GUI thread or from a task with `TaskAffinity::Gui`. The worker blocks waiting for the GUI thread to respond -- if the caller IS the GUI thread, it deadlocks.

On cancellation, `askGui` returns an invalid `QVariant` and the worker can exit cleanly.

## Qt Visualization Widget

`TaskGraph::Gui::TaskGraphWidget` is a drop-in Qt widget that renders the task DAG with live execution state, orthogonal edge routing, a control bar, inspector panel, log views, and optional graph editing.

```cpp
#include "gui/TaskGraphWidget.h"

// One-liner: create the widget with a preset factory
TaskGraph::Gui::TaskGraphWidget widget(&scheduler, TaskGraph::Gui::presetEditorFactory());
widget.resize(1100, 700);
widget.setWindowTitle("TaskGraph Editor");
widget.show();

scheduler.runTasksAsync();
```

### Preset factories

| Factory | Features |
|---|---|
| `presetViewOnlyFactory()` | Graph view, zoom/pan, node selection, read-only inspector |
| `presetMonitorFactory()` | View-only + progress bar, thread stats, log panel, run/cancel controls, GUI round-trip |
| `presetEditorFactory()` | Monitor + add/remove tasks, edit dependencies, edit task config |

### Custom feature set

```cpp
TaskGraph::Gui::FeatureSet fs = TaskGraph::Gui::FeatureSet::monitor();
fs.set(TaskGraph::Gui::Feature::EditAddTask);
fs.set(TaskGraph::Gui::Feature::EditDependencies);
// Leave EditRemoveTask and EditTaskConfig off

auto factory = TaskGraph::Gui::makeFactory(fs);
TaskGraph::Gui::TaskGraphWidget widget(&scheduler, factory);
```

### Interaction

- **Single click** a node to select it, populate the inspector, and highlight connected edges (blue = incoming dependencies, amber = outgoing dependents)
- **Double click** a node to open the per-task log overlay with a leader line
- **Middle-click drag** to pan; **scroll wheel** to zoom
- **Control bar** provides Run, Cancel, Reset, and thread/progress display
- **Inspector panel** shows task config (affinity, weight, timeout, retries) and dependencies; editable when idle with editor features enabled
- Editing is gated by `UiMode`: structural edits are disabled while the scheduler is running, except "Spawn Child" which calls `addDynamicTask`

The visualization layer (`TaskGraph::Gui`) is an evolving prototype. The visual style, layout algorithm, and interaction details may change across releases.

## Running the Example and Tests

### Example application

```
build\Debug\LibraryExample.exe
```

The example builds a multi-task DAG with varied weights, timeouts, retries, and a GUI round-trip task, then displays the editor widget.

### Unit tests

```
build\Debug\CoreTests.exe
```

**Important:** The test framework uses an inverted exit code convention: **exit code 1 = all tests passed**, exit code 0 = at least one test failed. This is not a bug. Check `$LASTEXITCODE` (PowerShell) or `%ERRORLEVEL%` (cmd) after running.

## Project Layout

```
TaskGraph/
  core/
    inc/              # Public headers
      gui/            # TaskGraph::Gui headers
    src/              # Implementation
      gui/            # TaskGraph::Gui implementation
  examples/
    LibraryExample/   # Demo application
  unittests/
    CoreTests/        # Unit test suite
  cmake/              # CMake utilities
  changelogs/         # Per-version changelogs
  CLAUDE.md           # Project manager instructions
  CMakeLists.txt      # Root build file
  build.bat           # Build script (usage: build.bat <preset>)
```

## Version and Stability

**Version:** 1.0.0 (unreleased)

The public core API (`TaskGraph::Task`, `TaskGraph::TaskScheduler`, `TaskGraph::TaskContext`, `TaskGraph::TaskGroup`) is considered additive on the 1.x line -- existing signatures will not be removed or changed incompatibly.

`TaskGraph::Gui` is **experimental**. The widget API (`TaskGraphWidget` constructor, preset factories, `FeatureSet`) is expected to remain stable, but visual layout, interaction behavior, and internal component classes may change without notice.
